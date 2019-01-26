#!/bin/env python
from collections import defaultdict
import sys 
import numpy
import math

ECN_TAKE_FEE = 0.000

# a set of stats and data related to a particular request, including the details of the requests, the fills, the lost opportunities etc.
class TradeRequest(object):
    def __init__( self, request_message ):

        assert isinstance( request_message, RequestMessage )
        assert request_message.delta_target != 0

        self.time = request_message.time
        self.symbol = request_message.symbol
        self.server = request_message.server
        self.init_size = request_message.delta_target
        # self.aggr: a list of tuples of aggr+time (a change in aggr in the middle of filling the request should come here)
        self.aggr = [( request_message.aggr, request_message.time )] 
        self.bid = request_message.bid
        self.ask = request_message.ask
        self.basis = (self.bid + self.ask)*0.5
        if (self.basis>0):
            self.spread = abs(self.init_size)*((self.ask - self.bid)+ECN_TAKE_FEE)
        else:
            self.spread = 0
        self.fills = []
        self.opp_losts = [] # a list of opportunities lost forever (e.g. when the initial target was 1000, and the client changed
                            # to 800 when we were only at 500. The last 200 are therefore opp-lost.
        self.size_left = self.init_size
        self.total_fills = 0
        self.total_opp_losts = 0
        self.total_other_fees = 0.0
        self.total_ecn_fees = 0.0

        self.stats_calculated = False

    # how many shares were not filled (in the Fill-instances in self.fills)
    def open_size( self ):
        return self.size_left

    # Use the fill (of type Fill) to match against this request, and change the Fill-content accordingly (mainly: the inner fill-size is
    # reduced to the left-over)
    def process_a_fill( self, fill ):
        assert isinstance( fill, Fill )
        #assert fill.server == self.server # I'm not sure that's necessary
        
        if fill.get_dir() != self.get_dir():
            return
        matched_size = self.get_dir() * min( abs(self.size_left), abs(fill.size) )
        self.fills.append( fill.extract_part_of_this_fill(matched_size) )
        self.size_left -= matched_size
        self.total_fills += matched_size
        self.total_other_fees += fill.other_fees
        self.total_ecn_fees += fill.ecn_fee
        if self.size_left==0: self.on_completion()
        return

    # Use the request_message to possibly change the aggressiveness, and possibly add a new "opp_lost"
    # (e.g. if our target was 0-->1000 and we're currently at 500 and this is a request for delta-target=-200, we have
    # a new "opp_lost" of 200)
    # - update 'delta-target' of the request_message accordingly.
    def process_a_request_message( self, request_message ):
        assert isinstance( request_message, RequestMessage )
        #assert request_message.server == self.server # I'm not sure that's necessary

        if request_message.get_dir() == -self.get_dir():
            # there's lost-opp
            opp_lost_size = self.get_dir() * min( abs(self.size_left), abs(request_message.delta_target) )
            self.opp_losts.append( request_message.extract_part_of_the_delta_target(opp_lost_size) )
            self.size_left -= opp_lost_size
            self.total_opp_losts += opp_lost_size
            if self.size_left==0: self.on_completion()
        # and check the aggressiveness
        if request_message.aggr != self.get_latest_aggr() and self.is_open():
            self.aggr.append( (request_message.aggr,request_message.time) )

    # assign all the remaining size to opportunity-lost, (assuming it's the end-of-day)
    def complete( self, px, time="END_OF_DAY" ):
        if self.is_complete():
            return

        new_opp_lost_time = time
        if time=="END_OF_DAY":
            new_opp_lost_time = float( 16*3600 )
        self.opp_losts.append( OppLost( self.size_left, px, new_opp_lost_time, True ) )
        self.total_opp_losts += self.size_left
        self.size_left = 0
        self.on_completion()

    def is_open( self ):
        return self.size_left != 0

    def is_complete( self ):
        return self.size_left == 0

    def get_open_size( self ):
        return self.size_left

    def get_dir( self ):
        return numpy.sign( self.init_size )

    def get_latest_aggr( self ):
        return self.aggr[-1][0]

    def get_init_aggr( self ):
        return self.aggr[0][0]

    def __repr__( self ):
        return "%-12s: %-5s %s init_size=%-4d total_fills=%-4d total_opp_losts=%-4d aggr=%6.3f" \
               %( self.__class__.__name__, self.symbol, hhmmss_str(self.time),
                  self.init_size, self.total_fills, self.total_opp_losts, self.get_init_aggr() )

    def calc_stats( self ):
        assert self.is_complete()
        if self.stats_calculated:
            return

        if self.total_fills != 0:
            self.avg_fill_px = numpy.average( [f.price for f in self.fills], weights = [f.size for f in self.fills] )
            self.avg_fill_tm = numpy.average( [f.time for f in self.fills], weights = [f.size for f in self.fills] ) - self.time # in secs
        else:
            self.avg_fill_px = 0.0
            self.avg_fill_tm = 0.0

        if self.total_opp_losts != 0:
            weights = [ol.size for ol in self.opp_losts]
            self.avg_opp_lost_px = numpy.average( [ol.price for ol in self.opp_losts], weights = weights )
            self.avg_opp_lost_tm = numpy.average( [ol.time  for ol in self.opp_losts], weights = weights ) - self.time
        else:
            self.avg_opp_lost_px = 0.0
            self.avg_opp_lost_tm = 0.0
        
        self.dollars_lost_in_slippage  = self.total_fills * (self.avg_fill_px - self.basis)  # not including fees/rebates
        self.dollars_lost_in_opp_losts = self.total_opp_losts * (self.avg_opp_lost_px - self.basis)
        self.dollar_flow_of_actual_vol_in_ideal_px = abs(self.total_fills) * self.basis
        self.dollar_flow_of_opp_lost_one_side = abs(self.total_opp_losts) * self.basis
        self.fill_rate = float(self.total_fills) / self.init_size

        # Following Drit & Brian's request: the "$-requested" of the "way-back" of the opp-lost, unless the opp'ed is due to a stop request
        self.opp_lost_way_back_usd_requested = sum( [abs(ol.size*ol.price) for ol in self.opp_losts if not ol.due_to_stop_req] )

        self.stats_calculated = True

    def on_completion( self ):
        self.calc_stats()

# an over fill is like a trade-request that is initialized from the over-fill (the target is now the opposite direction)
class OverFill(TradeRequest):
    
    def __init__( self, fill, aggr ):
        assert isinstance( fill, Fill )

        self.time = fill.time
        self.symbol = fill.symbol
        self.server = fill.server
        self.init_size = -fill.size
        self.aggr = [(aggr,fill.time)]
        self.bid = fill.bid
        self.ask = fill.ask
        self.basis = fill.price

        self.fills = []
        self.opp_losts = []
        self.size_left = self.init_size
        self.total_fills = 0
        self.total_opp_losts = 0
        self.total_other_fees = fill.other_fees
        self.total_ecn_fees = fill.ecn_fee
        self.stats_calculated = False


class RequestMessage(object):
    def __init__( self, request_line, sourcetag, prev_target=None, comment="", crossing_opp = False ):
        self.is_stop_request = False
        try:
            #2011/07/29 10:41:30.642829 INFO REQ USB currPos: 7998 qty: 2 at aggr 3.500 (26.01,26.02) [orderID: 1]
            (date, time, loglevel, rstring, symbol, pos_string, position, qty_string, qty, at_str, aggr_str, aggr, bid_ask, orderID) = request_line.split()
        except:
            #2011/07/29 10:43:14.350830 INFO REQ BPOP pos: 0 (qtyLeft: 0) (2.31,2.32) #stop
            (date, time, loglevel, rstring, symbol, pos_string, position, qty_string, qty, bid_ask) = request_line.split()
            qty = qty[:-1]
            aggr = 0
            self.is_stop_request = True

        (hr, min, sec) = time.split(':')
           
        self.time = int(hr) * 3600 + int(min) * 60 + float(sec)
        self.symbol = symbol
        self.server = sourcetag
        self.target = int(position) + int(qty)
        self.aggr = float(aggr)
        (bid,ask) = bid_ask[1:-1].split(',')
        self.bid = float(bid)
        self.ask = float(ask)
        
        self.crossing_opp = crossing_opp
        self.delta_target = self.target - prev_target

    def get_dir( self ):
        return numpy.sign( self.delta_target )

    def get_mid( self ):
        return (self.bid + self.ask)*0.5

    def get_cpx( self ):
        if (self.delta_target>0):
            return self.bid-ECN_TAKE_FEE
        if (self.delta_target<0):
            return self.ask+ECN_TAKE_FEE
        return self.get_mid()
    
    # reduce the (absolute value of) delta-target and return a matching opp-lost instance
    # note: 'partial_size' should have a sign opposite to self.get_dir()
    def extract_part_of_the_delta_target( self, partial_size ):
        assert numpy.sign( partial_size ) == -self.get_dir()

        if (self.crossing_opp):
            ret = OppLost( partial_size, self.get_cpx(), self.time, self.is_stop_request )
        else:
            ret = OppLost( partial_size, self.get_mid(), self.time, self.is_stop_request )

        self.delta_target += partial_size
        return ret

    def is_open( self ):
        return self.delta_target != 0

    def __repr__( self ):
        return "Request: %s %s delta_target=%+5d aggr=%6.3f"%( hhmmss_str(self.time), self.symbol, self.delta_target, self.aggr )

class Fill(object):

    def __init__( self, time, symbol, server, ecn, size, price, bid, ask, liquidity, total_other_fees, ecn_fee ):
        
        self.time = time
        self.symbol = symbol
        self.server = server
        self.ecn = ecn
        self.size = size
        self.price = price
        self.bid = bid
        self.ask = ask
        self.liquidity = liquidity
        self.other_fees = total_other_fees
        self.ecn_fee = ecn_fee

    # return a new Fill object which accounts for part of this fill, and reduce the size of this fill accordingly
    def extract_part_of_this_fill( self, partial_size ):
        assert (0<partial_size and partial_size<=self.size) or (self.size<=partial_size and partial_size<0)

        ret = Fill( self.time, self.symbol, self.server, self.ecn, partial_size, self.price, self.bid, self.ask,
                    self.liquidity, self.other_fees, self.ecn_fee )
        self.size -= partial_size
        return ret

    def get_dir( self ):
        return numpy.sign( self.size )

    # meaning: there are still shares in this fill not matched with any trade-request
    def is_open( self ):
        return self.size != 0

    def get_mid( self ):
        return (self.bid + self.ask)*0.5


class FillMessage(object):
    def __init__(self, line, sourcetag, fees_calculator):
        #2011/07/29 10:41:33.561628 INFO FILL USB BATS 2@26.01 (26.01,26.02) A (orderID: 1, seqnum: 240994095) Algo: JOIN_QUEUE
        (date, time, loglevel, fstring, symbol, ecn, fill_string, bid_ask, liquidity, rest) = line.split()
        (hr, min, sec) = time.split(':')
        self.time = int(hr) * 3600 + int(min) * 60 + float(sec)
        self.symbol = symbol
        self.server = sourcetag
        self.ecn = ecn
        (size, price) = fill_string.strip().split('@')
        self.size = int(size)
        self.price = float(price)
        (bid, ask) = bid_ask[1:-1].split(',')
        self.bid = float(bid)
        self.ask = float(ask)
        if liquidity == "A":
            liq = "add"
        elif liquidity == "R":
            liq = "rem"
        else:
            liq = "other"
        self.liquidity = liq
        self.other_fees = fees_calculator.get_other_fees( self.symbol, self.ecn, self.size, self.price, self.liquidity )
        self.ecn_fee = fees_calculator.get_ecn_fee( self.symbol, self.ecn, self.size, self.price, self.liquidity )

    def get_fill_instance( self ):
        return Fill( self.time, self.symbol, self.server, self.ecn, self.size, self.price, self.bid, self.ask, self.liquidity,
                     self.other_fees, self.ecn_fee )

    def __repr__( self ):
        return "Fill: %s %s size=%+5d px=%6.2f fees=%.4f"%( hhmmss_str(self.time), self.symbol, self.size, self.price,
                                                            self.other_fees+self.ecn_fee )

    def get_mid( self ):
        return (self.bid + self.ask)*0.5

class OppLost(object):
    def __init__( self, size, px, time, due_to_stop_req ):
        self.size  = size
        self.price = px
        self.time  = time
        self.due_to_stop_req = due_to_stop_req

def read_files(filenames, fees_calculator, crossing_opp):
    sym_to_requests_and_fills = defaultdict( list ) # a dictionary that creates an empty list when a new key is used
    sym_to_curr_targets = defaultdict( lambda: None )

    for filename in filenames:
        try:
            lns = open(filename).readlines()
            for ln in lns:
                live, sep, comment = ln.partition('#') # 'live' is now the line up to the first '#'
                if comment.strip().startswith('implicit'):
                    continue
                date, ts, loglevel, name, sym, rest = live.split(' ', 3)
                if name == 'REQ':
                    new_item = RequestMessage( live, filename, sym_to_curr_targets[sym], comment, crossing_opp ) 
                    sym_to_curr_targets[sym] = new_item.target
                else:
                    new_item = FillMessage( live, filename, fees_calculator )
                sym_to_requests_and_fills[sym].append( new_item )

        except IOError, e:
            sys.stderr.write(e.strerror + ": " + e.filename)

    return sym_to_requests_and_fills

# Returns a list of TradeRequest-s for this symbol
def process_requests_and_fills_of_a_symbol( requests_and_fills_list, verbal=False ):
    completed_trade_requests = []  # a list completed trade-requests (they were completely matched against fills/opp-losts
    trade_requests_in_process = [] # a list of non-complete trade-requests, ordered from earliest to latest
    curr_aggr = "UNKNOWN"
    latest_px = None
    symbol = None
    
    for item in requests_and_fills_list:
        if verbal: print( "################################ \nNow Processing: %s"%item.__repr__() )
        latest_px = item.get_mid()
        if isinstance( item, RequestMessage ):
            symbol = item.symbol
            # request-messages are processed from the latest to the oldest trade-requests-in-process
            for i in range(len(trade_requests_in_process)-1,-1,-1):
                trip = trade_requests_in_process[ i ]
                trip.process_a_request_message( item )
                if trip.is_complete():
                    completed_trade_requests.append( trip )
                    trade_requests_in_process.pop( i )
                    if verbal: print( trip )
            # if request-message still has a non-zero delta-target, we need to open a new TradeRequest
            if item.is_open():
                trade_requests_in_process.append( TradeRequest(item) )
            # update current aggressiveness and target
            curr_aggr = item.aggr
        else:
            # This is a fill - message
            assert isinstance( item, FillMessage )
            symbol = item.symbol
            fill = item.get_fill_instance()
            # fill messages are processed from the latest order to the new one
            i=0
            while i < len(trade_requests_in_process):
                trip = trade_requests_in_process[i]
                trip.process_a_fill( fill )
                if trip.is_complete():
                    completed_trade_requests.append( trip )
                    trade_requests_in_process.pop( i )
                    if verbal: print( trip )
                else:
                    i += 1
            # if fill is still open, it is an over fill, and we should add that in the beginning of the T.R.I.P-queue
            if fill.is_open():
                trade_requests_in_process = [OverFill(fill,curr_aggr)] + trade_requests_in_process
        
        # print the open trade requests for debugging
        if verbal:
            for trip in trade_requests_in_process:
                print( trip )

    # at this point, all trade-requests should be completed (because of the end-of-day 'stop' message), but just in case:
    if len(trade_requests_in_process) > 0:
        print( "%-5s WARNING: there are uncompleted trade-requests. Completing them artificially with end-of-day time "
               "and latest px from file."%symbol )
        for trip in trade_requests_in_process:
            trip.complete( latest_px )
            completed_trade_requests.append( trip )

    return completed_trade_requests                

def hms(time):
    return (int(time/3600), int((time%3600)/60), time%60)

def hhmmss_str( time_in_secs ):
    h,m,s = hms(time_in_secs)
    return "%02d:%02d:%06.3f"%(h,m,s)

class BasketStats( object ):
    def __init__( self, trade_requests_list ):
        # $-flow
        self.total_dollar_flow_of_actual_vol_in_ideal_px = sum([ t.dollar_flow_of_actual_vol_in_ideal_px for t in trade_requests_list \
                                                                 if not isinstance(t,OverFill) ])
        self.total_dollar_flow_of_opp_lost_one_side      = sum([ t.dollar_flow_of_opp_lost_one_side      for t in trade_requests_list \
                                                                 if not isinstance(t,OverFill) ])

        # costs
        self.total_dollar_lost_in_slippage               = sum([ t.dollars_lost_in_slippage              for t in trade_requests_list ])
        self.total_other_fees                            = sum([ t.total_other_fees                      for t in trade_requests_list ])
        self.total_ecn_fees                              = sum([ t.total_ecn_fees                        for t in trade_requests_list ])
        self.total_dollar_lost_in_opp_lost               = sum([ t.dollars_lost_in_opp_losts             for t in trade_requests_list ])
        self.total_dollar_lost_in_slippage += self.total_ecn_fees
        self.total_dollar_lost = self.total_dollar_lost_in_slippage + self.total_dollar_lost_in_opp_lost

        # fill rate
        self.total_n_shares = sum([ abs(t.init_size) for t in trade_requests_list ])
        self.total_fills    = sum([ abs(t.total_fills) for t in trade_requests_list ])

        # fill time
        if self.total_fills != 0:
            self.avg_fill_time = numpy.average( [t.avg_fill_tm for t in trade_requests_list],
                                                weights = [t.dollar_flow_of_actual_vol_in_ideal_px for t in trade_requests_list] )
        else:
            self.avg_fill_time = 0.0

        # total_dollars_requested: we count an "opportunity lost" only one-way, i.e. not as if we needed to trade back and forth
        self.total_dollars_requested = self.total_dollar_flow_of_actual_vol_in_ideal_px + \
                                       self.total_dollar_flow_of_opp_lost_one_side
        # And here's a version, following Drit's and Brian's request, in which we count in "round-trip"
        self.total_dollars_requested_with_opp_round_trip = self.total_dollars_requested + \
                                                           sum([ t.opp_lost_way_back_usd_requested for t in trade_requests_list ])
        self.total_spread_basis = sum ( [ t.spread for t in trade_requests_list \
                                        if not isinstance(t,OverFill) ] )
        if self.total_dollars_requested > 0:
            self.bps_slippage = float(self.total_dollar_lost_in_slippage) / self.total_dollars_requested * 10000
            self.bps_opp_lost = float(self.total_dollar_lost_in_opp_lost) / self.total_dollars_requested * 10000
            self.spread_basis = 0.5 * float(self.total_spread_basis) / self.total_dollars_requested * 10000
        else:
            self.bps_slippage = 0.0
            self.bps_opp_lost = 0.0
            self.spread_basis = 0.0
            
        self.bps_total_TC = self.bps_slippage + self.bps_opp_lost

        if self.total_n_shares != 0 and self.total_dollars_requested != 0:
            self.fill_rate_in_shs = self.total_fills / self.total_n_shares
        else:
            self.fill_rate_in_shs = 0.0
            
        if self.total_dollars_requested > 0:
            self.fill_rate_in_usd = self.total_dollar_flow_of_actual_vol_in_ideal_px / self.total_dollars_requested
        else:
            self.fill_rate_in_usd = 0.0

    def get_very_detailed_stats_string_table( self ):
        return " | $%11.2f | %6.2f%% | %4.0f | " \
               "%8.2f | %8.2f | " \
               " $%+9.2f | %+6.2f  | $%+9.2f | %+6.2f | $%+9.2f | %+6.2f | %6.2f" \
               %( self.total_dollars_requested, self.fill_rate_in_usd*100,
                  self.avg_fill_time,
                  self.total_ecn_fees, self.total_other_fees,
                  self.total_dollar_lost_in_slippage, self.bps_slippage,
                  self.total_dollar_lost_in_opp_lost, self.bps_opp_lost,
                  self.total_dollar_lost_in_slippage+self.total_dollar_lost_in_opp_lost,
                  self.bps_slippage+self.bps_opp_lost,self.spread_basis)
    
    def get_very_detailed_stats_string( self ):
        return "\t$%11.2f requested (%6.2f%% fill) (%11.2f with round-trip for opp'ed) %4.0f secs avg\n" \
               "\t\tECN_fees = %8.2f\tother_fees = %8.2f\n" \
               "\t\t$%+9.2f (%+6.2f bps) slip, $%+9.2f (%+6.2f bps) opp" \
               %( self.total_dollars_requested, self.fill_rate_in_usd*100,
                  self.total_dollars_requested_with_opp_round_trip,
                  self.avg_fill_time,
                  self.total_ecn_fees, self.total_other_fees,
                  self.total_dollar_lost_in_slippage, self.bps_slippage,
                  self.total_dollar_lost_in_opp_lost, self.bps_opp_lost )

    def get_detailed_stats_string( self ):
        return "$%11.2f reqstd (%6.2f%% fill) %4.0f secs avg\n" \
               "\t\t$%+9.2f (%+6.2f bps) slip, $%+9.2f (%+6.2f bps) opp" \
               %( self.total_dollars_requested, self.fill_rate_in_usd*100,
                  self.avg_fill_time, 
                  self.total_dollar_lost_in_slippage, self.bps_slippage,
                  self.total_dollar_lost_in_opp_lost, self.bps_opp_lost )

    def get_short_stats_string( self ):
        return "$%11.2f (%6.2f%% fill): $%+8.2f (%+7.2f bps) slip, $%8.2f (%+7.2f bps) opp, %4.0f sec" \
               %( self.total_dollars_requested, self.fill_rate_in_usd*100,
                  self.total_dollar_lost_in_slippage, self.bps_slippage,
                  self.total_dollar_lost_in_opp_lost, self.bps_opp_lost,
                  self.avg_fill_time )
    def get_short_table_string( self ):
        return "%11.2f %6.2f %+8.2f %8.2f" \
               %( self.total_dollars_requested, self.fill_rate_in_usd*100,
                  self.total_dollar_lost_in_slippage,
                  self.total_dollar_lost_in_opp_lost )
    
def print_table_header():
    print("               | Total $      | Fill %  |  Sec |  ECN Fee | Other Fee|   Slip      | in bps  |  Opp       | in bps | Total      | bps    | 1/2 spd")

def gensymstats(params):
    files = params['files']
    Drit_mode = params['Drit_mode']
    table_mode = params['table_mode']
    server_mode = params['server-mode']
    # Instantiate the Fees-Calculator
    brokerage_fee_table_filename = params['param_files_dir'] + "/fees_table_" + params['brokerage']
    global_fee_table_filename = params['param_files_dir']+ "/global_fees"
    exchanges_filename = os.environ['RUN_DIR'] + "/exec/exchanges"
    fees_calculator = FeesCalculator(brokerage_fee_table_filename, global_fee_table_filename, exchanges_filename)

    if len(files)==0:
        print_usage()
        return
    symbol_to_requests_and_fills = read_files(files, fees_calculator) # returns a dictionary symbol ==> <list of requests+fills>
    symbols = symbol_to_requests_and_fills.keys()
    symbol_to_lists_of_completed_trade_requests = {}
    for sym in symbols:
        symbol_to_lists_of_completed_trade_requests[sym] = process_requests_and_fills_of_a_symbol(symbol_to_requests_and_fills[sym])

    sym_to_stats = {}
    symbols.sort()
    for sym in symbols:
        stat = BasketStats(symbol_to_lists_of_completed_trade_requests[sym])
        if (stat.total_dollars_requested>0):
            print "%-5s %11.2f %6.2f %7.2f %7.2f %4.0f"%(sym,stat.total_dollars_requested, stat.fill_rate_in_usd*100, stat.bps_slippage, stat.bps_opp_lost, stat.avg_fill_time)
    
def main(params):
    if (params['print-stats']):
         return gensymstats( params )
    files = params['files']
    Drit_mode = params['Drit_mode']
    table_mode = params['table_mode']
    server_mode = params['server-mode']
    crossing_opp = params['crossing-opp']
   
    # Instantiate the Fees-Calculator
    brokerage_fee_table_filename = params['param_files_dir'] + "/fees_table_" + params['brokerage']
    global_fee_table_filename = params['param_files_dir']+ "/global_fees"
    exchanges_filename = os.environ['RUN_DIR'] + "/exec/exchanges"
    fees_calculator = FeesCalculator(brokerage_fee_table_filename, global_fee_table_filename, exchanges_filename)

    if len(files)==0:
        print_usage()
        return
    symbol_to_requests_and_fills = read_files(files, fees_calculator, crossing_opp) # returns a dictionary symbol ==> <list of requests+fills>
    symbols = symbol_to_requests_and_fills.keys()
    symbol_to_lists_of_completed_trade_requests = {}
    for sym in symbols:
        symbol_to_lists_of_completed_trade_requests[sym] = process_requests_and_fills_of_a_symbol(symbol_to_requests_and_fills[sym])

    united_list_of_completed_trade_requests = sum(symbol_to_lists_of_completed_trade_requests.values(), [])
    n_ctrs = len(united_list_of_completed_trade_requests)

    if table_mode:
        print_table_header()
        
    # Total cost
    bs = BasketStats(united_list_of_completed_trade_requests)
    if table_mode: print("Overall Cost  %s"%bs.get_very_detailed_stats_string_table())
    elif Drit_mode: print("Total Cost:  %s\n\n"%bs.get_very_detailed_stats_string())
    else: print("Total Cost:  %s\n\n"%bs.get_detailed_stats_string())

    # Per side (BUY/SELL)
    bs = BasketStats( [ctr for ctr in united_list_of_completed_trade_requests if ctr.get_dir()==1] )
    if table_mode: print( "Buy           %s"%bs.get_very_detailed_stats_string_table() )
    elif Drit_mode: print( "Buy   Cost:  %s\n\n"%bs.get_very_detailed_stats_string() )
    else: print( "Buy   Cost:  %s\n\n"%bs.get_detailed_stats_string() )
    bs = BasketStats( [ctr for ctr in united_list_of_completed_trade_requests if ctr.get_dir()==-1] )
    if table_mode: print( "Sell          %s"%bs.get_very_detailed_stats_string_table() )
    elif Drit_mode: print( "Sell  Cost:  %s\n\n"%bs.get_very_detailed_stats_string() )
    else: print( "Sell  Cost:  %s\n\n"%bs.get_detailed_stats_string() )

    # OverFill Cases
    over_fills = [ctr for ctr in united_list_of_completed_trade_requests if isinstance(ctr,OverFill)]
    bs = BasketStats( over_fills )
    print( "\nOver Fills: %d cases, lost $%.2f"%( len(over_fills), bs.total_dollar_lost_in_slippage + bs.total_dollar_lost_in_opp_lost ) )
    # Per aggr (rounded to 0.1 bps scale)
    print( "\nPer aggr level:" )
    rounded_aggrs = [int(ctr.get_init_aggr()*10) for ctr in united_list_of_completed_trade_requests]
    aggr_types = list(set( rounded_aggrs )) # = uniq( rounded_aggrs )
    aggr_types.sort()
    for aggr in aggr_types:
        bs = BasketStats( [united_list_of_completed_trade_requests[i] for i in range(n_ctrs) if rounded_aggrs[i]==aggr] )
        if table_mode: print( "[%4.1f,%4.1f)   %s"%(aggr/10.0, aggr/10.0 + 0.1,bs.get_very_detailed_stats_string_table()) )
        elif Drit_mode: print( "[%4.1f,%4.1f): %s\n"%( aggr/10.0, aggr/10.0 + 0.1, bs.get_very_detailed_stats_string() ) )
        else: print( "[%4.1f,%4.1f): %s\n"%( aggr/10.0, aggr/10.0 + 0.1, bs.get_detailed_stats_string() ) )
        
    # Per time: (an arbitrary devision to 15-minute periods)
    
    print( "\nPer time basket (the time refers only to the initialization of the trade-requests)" )
    if table_mode:
        print_table_header()
        
    size_of_each_frame_in_secs = 10*60 # 15 minutes
    rounded_times = [int(ctr.time/size_of_each_frame_in_secs) for ctr in united_list_of_completed_trade_requests]
    times_types = list(set( rounded_times ))
    times_types.sort()
    for tm in times_types:
        bs = BasketStats( [united_list_of_completed_trade_requests[i] for i in range(n_ctrs) if rounded_times[i]==tm] )
        hms_start = hms( tm*size_of_each_frame_in_secs )
        hms_end   = hms( (tm+1)*size_of_each_frame_in_secs )
        if table_mode: print( "%02d:%02d-%02d:%02d   %s"%( hms_start[0], hms_start[1], hms_end[0], hms_end[1], bs.get_very_detailed_stats_string_table() ) )
        elif Drit_mode: print( "%02d:%02d-%02d:%02d : %s\n"%( hms_start[0], hms_start[1], hms_end[0], hms_end[1], bs.get_very_detailed_stats_string() ) )
        else: print( "%02d:%02d-%02d:%02d : %s\n"%( hms_start[0], hms_start[1], hms_end[0], hms_end[1], bs.get_detailed_stats_string() ) )

    # Per symbol:
    sym_to_stats = {}
    for sym in symbols:
        sym_to_stats[ sym ] = BasketStats( symbol_to_lists_of_completed_trade_requests[sym] )
    syms_sorted_by_bps_lost = list( symbols )
    syms_sorted_by_bps_lost.sort( lambda x,y: cmp(sym_to_stats[x].bps_total_TC,sym_to_stats[y].bps_total_TC) )
    syms_sorted_by_usd_lost = list( symbols )
    syms_sorted_by_usd_lost.sort( lambda x,y: cmp(sym_to_stats[y].total_dollar_lost,sym_to_stats[x].total_dollar_lost) ) # descending order
    syms_sorted_by_usd_made = list( symbols )
    syms_sorted_by_usd_made.sort( lambda x,y: -cmp(sym_to_stats[y].total_dollar_lost,sym_to_stats[x].total_dollar_lost) ) # descending order

    n_symbols = len( symbols )
    N_SYMBOLS_TO_SHOW = min( 20, n_symbols )
    print( "\n%d worst symbols (out of %d) $-lost-wise:"%(N_SYMBOLS_TO_SHOW,n_symbols) )
    for sym in syms_sorted_by_usd_lost[:N_SYMBOLS_TO_SHOW]:
        print( "%-5s: %s"%( sym, sym_to_stats[sym].get_short_stats_string() ) )
    print( "\n%d worst symbols (out of %d) bps-wise:"%(N_SYMBOLS_TO_SHOW,n_symbols) )
    for i in range(n_symbols-1,n_symbols-N_SYMBOLS_TO_SHOW-1, -1):
        sym = syms_sorted_by_bps_lost[ i ]
        print( "%-5s: %s"%( sym, sym_to_stats[sym].get_short_stats_string() ) )
    print( "\n%d best symbols (out of %d) $-wise:"%(N_SYMBOLS_TO_SHOW,n_symbols) )
    for sym in syms_sorted_by_usd_made[:N_SYMBOLS_TO_SHOW]:
        print( "%-5s: %s"%( sym, sym_to_stats[sym].get_short_stats_string() ) )
    print( "\n%d best symbols (out of %d) bps-wise:"%(N_SYMBOLS_TO_SHOW,n_symbols) )
    for sym in syms_sorted_by_bps_lost[:N_SYMBOLS_TO_SHOW]:
        print( "%-5s: %s"%( sym, sym_to_stats[sym].get_short_stats_string() ) )
    
    # per server
    print("\nPer input file (server instance)")
    server_dict = defaultdict(list)
    for req in united_list_of_completed_trade_requests:
        server_dict[req.server].append(req)
    for (server, reqs) in sorted(server_dict.items()):
        bs = BasketStats(reqs)
        snames = server.split('/');
        if (server_mode):
            print ("%s  %s" % (snames[-1], bs.get_short_table_string()))
        else:
            print ("%s: %s" % (snames[-1], bs.get_short_stats_string()))
    
def print_usage():
    print("Usage: {} --brokerage <'LB'/'MS'> [-d] <cost.log1> <cost.log2> ....".format(sys.argv[0]))
    print("-d: Drit mode. We ignore stop-requests and calculate $-requested with round-trip for opportunity losses")

# read arguments. Throws an exception if there's a problem
def read_arguments( argv ):

    if len(argv) < 3:
        print_usage()
        raise Exception("Error: too few arguments")
    ret = {}
    ret['Drit_mode'] = False
    ret['table_mode'] = False
    ret['files'] = []
    ret['print-stats'] = False
    ret['server-mode'] = False
    ret['crossing-opp'] = False
    ret['brokerage'] = "MS" # default
    ret['param_files_dir'] = os.environ['CONFIG_DIR'] + '/exec'
    ret['gt_scripts_dir'] = os.environ['ROOT_DIR'] + '/bin'
    
    i=1
    while i<len(argv):
        if argv[i] == "-c":
            ret['crossing-opp'] = True
            i +=1
        if argv[i] == "-p":
            ret['print-stats'] = True
            i +=1
        if argv[i] == "-d":
            ret['Drit_mode'] = True
            i += 1
        elif  argv[i] == "-t":
            ret['table_mode'] = True
            i += 1
        elif argv[i] == "-s":
            ret['server-mode'] = True
            i+=1
        elif argv[i] == "--brokerage":
            ret['brokerage'] = argv[i+1]
            i += 2
        else:
            ret['files'].append( argv[i] )
            i += 1

    if not ret.has_key('param_files_dir'):
        raise Exception('Error: Param-files dir not specified')
    if not ret.has_key('gt_scripts_dir'):
        raise Exception('Error: gt-scripts dir not specified')
    if not ret.has_key('brokerage') or not ret['brokerage'] in ['LB','MS','WB']:
        raise Exception('Error: brokerage not specified')

    return ret    

if __name__ == '__main__':
    params = read_arguments(sys.argv)
    execfile(os.environ['ROOT_DIR'] + '/bin/fees_calculator.py')
    main(params)

    
