import os

NA_STRING = "N.A."
FEES_FILES_DELIMITER = ","
EXCHANGES_FILE_DELIMITER = ","
MILLION_DOUBLE = 1000000.0
NA_FEE_VALUE = 10000000

def ecn_to_tape( ecn ):
    if ecn == "NYSE":
        return 'A'
    elif ecn == "ISLD" or ecn == "NSDQ" or ecn == "NASDAQ":
        return 'C'
    elif ecn == "ARCA" or ecn == "AMEX":
        return 'B'
    elif ecn == "N.A.":
        return 'C'
    return "UNKN"

# Positive Fees are bad. So rebates are written as negative numbers
class FeesCalculator(object):
    def __init__( self, brokerage_fee_table_filename, global_fee_table_filename, exchanges_filename ):

        self.ecns = {}
        # First upload the brokerage-dependent fees
        lines = open( brokerage_fee_table_filename ).readlines()
        header = lines[0]
        assert header[0] == "#"   # should be header file
        fieldNames = header[1:-1].split( FEES_FILES_DELIMITER )
        fieldNames = [f.strip() for f in fieldNames if len(f.strip())>0]
        for l in lines[1:]:
            if len(l.strip())==0: # empty line
                continue
            fields = l[:-1].split( FEES_FILES_DELIMITER )
            fields = [f.strip() for f in fields if len(f.strip())>0]
            if fields[0] == "BROKERAGE_FEE_PER_SHARE":
                assert len(fields)==2
                self.brokerage_fee_per_sh = float(fields[1])
                continue
            # a regular line which represents some ECN
            assert len(fields) == len(fieldNames) 
            self.ecns[ fields[0] ] = EcnFees( fieldNames, fields )

        # now upload the global fees
        lines = open( global_fee_table_filename ).readlines()
        for l in lines:
            if len(l.strip())==0: # empty line
                continue
            fields = l[:-1].split( FEES_FILES_DELIMITER )
            fields = [f.strip() for f in fields if len(f.strip())>0]
            assert len(fields)==2
            if fields[0] == "SEC_FEE_PER_MILLION_DOLLARS":
                self.sec_fee_per_million_dollars = float( fields[1] )
            elif fields[0] == "SEC_MIN_FEE":
                self.sec_min_fee = float( fields[1] )
            elif fields[0] == "NASD_FEE_PER_SHARE":
                self.nasd_fee_per_share = float( fields[1] )
            elif fields[0] == "NASD_MIN_FEE":
                self.nasd_min_fee = float( fields[1] )
            elif fields[0] == "NASD_MAX_FEE":
                self.nasd_max_fee = float( fields[1] )

        # And now read the exchanges of each symbol in order to get its tape
        lines = open( exchanges_filename ).readlines()
        self.symbol_to_tape = {}
        for l in lines:
            if len(l.strip())==0 or l[0]=="#":
                continue
            fields = l[:-1].split( EXCHANGES_FILE_DELIMITER )
            assert len(fields)==2
            symbol = fields[0]
            ecn = fields[1]
            tape = ecn_to_tape( ecn )
            assert tape in ['A','B','C']
            self.symbol_to_tape[ symbol ] = tape
            self.symbol_to_tape[ symbol + ".BUY"] = tape

    # This is the "Sec Section 31 Fee", which is taken from the sell side of each trade
    def get_sec_fee( self, signed_size, price ):
        if signed_size >= 0:
            return 0.0 # this fee applies only when selling shares
        dollarAmount = abs(signed_size) * price
        ret = dollarAmount / MILLION_DOUBLE * self.sec_fee_per_million_dollars
        # round in cents
        ret = int(ret*100 + 0.5) / 100.0
        # make sure it's at least $0.01 (which is the minimum)
        if ret < self.sec_min_fee:
            ret = self.sec_min_fee
        return ret

    # This is the "NASD Transaction Activity Fee(TAF)": a per-share fee on sells
    def get_nasd_fee( self, signed_size ):
        if signed_size>=0:
            return 0.0 # this fee applies only when selling shares

        ret = self.nasd_fee_per_share * abs(signed_size)
        # round UP in cents (ceiling)  (0.021 ==> 0.03, but 0.02 ==> 0.02)
        floor = int(ret*100) / 100.0;
        if ret - floor > 0.0000001:
            ret = floor + 0.01
        else:
            ret = floor
        # apply the minimum and maximum fees
        if ret < self.nasd_min_fee:
            ret = self.nasd_min_fee
        if ret > self.nasd_max_fee:
            ret = self.nasd_max_fee
            
        return ret

    # sum the following fees:
    # - ECN fee
    # - Brokerage fee
    # - SEC fee
    # - NASD fee
    # For now, this assumes there are no odd lot/routing fees involved
    def get_total_trade_fees( self, symbol, ecn, signed_size, price, liq ):
        total_fees = 0.0
        total_fees += self.ecns[ecn].getEcnFee( abs(signed_size), self.symbol_to_tape[symbol], liq )
        total_fees += abs(signed_size) * self.brokerage_fee_per_sh
        total_fees += self.get_sec_fee( signed_size, price )
        total_fees += self.get_nasd_fee( signed_size )
        return total_fees

    def get_ecn_fee( self, symbol, ecn, signed_size, price, liq ):
        if symbol in self.symbol_to_tape:
            return self.ecns[ecn].getEcnFee( abs(signed_size), self.symbol_to_tape[symbol], liq )
        return self.ecns[ecn].getEcnFee( abs(signed_size), 'A', liq )
    

    # get all fees outside the ECN fee
    def get_other_fees( self, symbol, ecn, signed_size, price, liq ):
        other_fees = 0.0
        other_fees += abs(signed_size) * self.brokerage_fee_per_sh
        other_fees += self.get_sec_fee( signed_size, price )
        other_fees += self.get_nasd_fee( signed_size )
        return other_fees


class EcnFees(object):
    def __init__( self, fieldNames, fields ):

        self.take_liq_fees = {} # tape ==> fee_per_share
        self.add_liq_fees = {} # tape ==> fee_per_share
        self.route_fees = {} # tape ==> fee_per_share
        
        for i in range(len(fields)):
            name = fieldNames[i]
            value = fields[i]
            if value == NA_STRING:
                value = NA_FEE_VALUE
            if name == "ECN":
                self.ecn = value
                
            elif name == "TAPE_A_TAKE":
                self.take_liq_fees[ 'A' ] = float(value)
            elif name == "TAPE_A_ADD":
                self.add_liq_fees[ 'A' ] = float(value)
            elif name == "TAPE_A_ROUTE":
                self.route_fees[ 'A' ] = float(value)
                
            elif name == "TAPE_B_TAKE":
                self.take_liq_fees[ 'B' ] = float(value)
            elif name == "TAPE_B_ADD":
                self.add_liq_fees[ 'B' ] = float(value)
            elif name == "TAPE_B_ROUTE":
                self.route_fees[ 'B' ] = float(value)
                
            elif name == "TAPE_C_TAKE":
                self.take_liq_fees[ 'C' ] = float(value)
            elif name == "TAPE_C_ADD":
                self.add_liq_fees[ 'C' ] = float(value)
            elif name == "TAPE_C_ROUTE":
                self.route_fees[ 'C' ] = float(value)

    def getEcnFee( self, size, tape, liq ):
        if liq == "add":
            return abs(size) * self.add_liq_fees[tape]
        elif liq == "take" or liq == "remove":
            return abs(size) * self.take_liq_fees[tape]
        elif liq == "other":
            return 0.0
        print( "WARNING: Unknown 'liq' in EcnFees.getEcnFee(): %s"%liq )
        return 0.0


    
            
