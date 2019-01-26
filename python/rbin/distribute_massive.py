#!/bin/env python

from optparse import OptionParser
import os.path
import os
from sys import stderr
import re
from math import floor, ceil
import random
import util
import newdb

database = None

def get_dist(date, secid2tickers, massive, overflow_mult):
    global database
    total = 0.0
    dist ={}
    volsumFile = "/".join((os.environ["DATA_DIR"], "bars", str(date), "volsum.txt"))
    if not os.path.isfile(volsumFile):
        raise Exception("Problem finding historical volume data in file {}".format(volsumFile))
    with open(volsumFile, "r") as f:
        #skip the header
        f.readline()
        for line in f:
            tokens = line.strip().split("|")
            secid = tokens[0]
            ticker = secid2tickers.get(secid, None)
            if ticker is None: 
                ticker = database.getXrefFromSecid("TIC", int(secid), util.convert_date_to_millis(date))
            if ticker is None:
                continue
            mult = 1
            if ticker not in massive:
                mult = overflow_mult
            vol = mult * sum([float(field) for field in tokens[1:]])
            total += vol
            if vol > 0:
                dist[ticker] = vol
    
    for ticker in dist.keys():
        dist[ticker] = dist[ticker] / total
            
    return dist
    
def get_secid2tickers(file):
    res = dict()
    with open(file, "r") as f:
        for line in f:
            tokens = line.strip().split("|")
            secid = tokens[1]
            ticker = tokens[0]
            res[secid] = ticker
    return res

def get_massive(file):
    return set([sym for (sym, group) in map(lambda x: x.strip().split(), open(file).readlines())])

def get_symlist(file):
    return set(map(lambda x: x.strip(), open(file).readlines()))

def distribute(dist, universe, massive, instances,keep_together,add_to_all):
    def sum_vol(symset): return reduce(lambda t, sym: t+dist[sym], symset, 0.0)

    overflow = set(dist.keys()) - massive
    used_overflow = overflow & universe

    insts = [(0, add_to_all, inst) for inst in instances]
    
    items = [(dist[sym], set([sym])) for sym in massive & universe - keep_together]
    if (len(used_overflow)) > 0:
        items.append((sum_vol(overflow), used_overflow))
    if (len(keep_together)>0):
        items.append((sum_vol(keep_together),keep_together))
    util.info("Overflow %d syms, %5.2f vol" %(len(overflow), sum_vol(overflow)*100))
    util.info("Need %d syms, %5.2f vol" %(len(used_overflow), sum_vol(used_overflow)*100))
    util.info("\t%s" % " ".join(used_overflow))
    items.sort(lambda x,y: -1*cmp(x,y))
    for (vol, symset) in items:
        curvol, cursym, name = insts.pop()
        insts.append((curvol + vol, cursym | symset, name))
        insts.sort(lambda (lw, ls, ln), (rw, rs,rr): -1*cmp(lw, rw))
    return insts


def main():
    global database 
    parser = OptionParser()
    parser.add_option('-d', '--date',         dest='date',         help='Date to get data distribution')
    parser.add_option('-g', '--groups',       dest='groups',       help='groups.ports file')
    parser.add_option('-t', '--tickers_file',     dest='tickers_file',     help='tickers file')
    parser.add_option('-k', '--keeptogether', dest='keeptogether', help='list of stocks to keep on the same server')
    parser.add_option('-a', '--addtoall',     dest='addtoall',     help='add symbols to all universes (SHOULD NOT BE TRADED)"', default = "SPY")
    parser.add_option('-s', '--secmaster',    dest='secmaster',    help='Security master file for list of valid symbols')
    parser.add_option('-m', '--overflow_mult', dest='overflow_mult', help='Multiply the overflow bucket\'s volume', default = 1.0)
    opt, insts = parser.parse_args()
    random.shuffle(insts) 
    if opt.date is None or opt.groups is None or opt.tickers_file is None or opt.secmaster is None:
        util.error("All options must be set:")
        exit(2)

    if len(insts) < 1:
        util.error("Must specify at least one instance.")
        
    newdb.init_db()
    database = newdb.get_db()
        
    secmaster = get_symlist(opt.secmaster)
    secid2tickers = get_secid2tickers(opt.tickers_file)
    for secid, ticker in secid2tickers.items():
        if ticker not in secmaster: del secid2tickers[secid]
    universe = set(secid2tickers.values())
    massive = get_massive(opt.groups)
    dist = get_dist(opt.date, secid2tickers, massive, float(opt.overflow_mult))
    nodist = universe - set(dist.keys())
    util.info("%d symbols without data distribution: %s" % (len(nodist), " ".join(nodist)))
    dist.update(map(lambda k: (k, 0.0), universe - set(dist.keys())))
    if opt.keeptogether is not None:
        keep=get_symlist(opt.keeptogether)
    else:
        keep=set()
    if opt.addtoall is not None:
        all = set(opt.addtoall.split(","))
    else:
        all=set()
    assign = distribute(dist, universe, massive, insts,keep,all)
    for (vol, symset, instname) in assign:
        util.info("Instance %s sees %4.2f%% volume, trades %d symbols" % (instname, vol*100, len(symset)))
        
        symfile = open(instname, 'w')
        symlist = list(symset)
        symlist.sort()
        symfile.writelines(map(lambda s: s + "\n", symlist))
        symfile.close()

if __name__ == "__main__":
    main()
