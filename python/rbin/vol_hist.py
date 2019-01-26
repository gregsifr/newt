#!/bin/env python

import argparse
import os.path
import os
import util
import bisect

def get_dist(date, secid2tickers):
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
            ticker = secid2tickers.get(tokens[0], None)
            if ticker is None: continue
            vol = sum([float(field) for field in tokens[1:]])
            total += vol
            if vol > 0:
                dist[ticker] = vol
    
    for ticker in dist.keys():
        dist[ticker] = dist[ticker] / total
            
    return dist
    
def get_vol_hist(date, secid2tickers, hist):
    bar_folders = sorted(os.listdir(os.environ["DATA_DIR"] + "/bars"))
    ii = bisect.bisect_left(bar_folders, date)
    #get the folders
    bar_folders = bar_folders[max(0,ii-hist):ii]
    
    if len(bar_folders) == 0:
        util.error("Failed to locate any historical volume data for date {} and history of {} days".format(date, hist))
        exit(1)
    if len(bar_folders) < hist:
        util.error("Only found {} days of historical volume data for date {} and history of {} days".format(len(bar_folders), date, hist))
    
    secid2relvol = {}
    secid2dailyvol = {}
    header = None
    daysUsed = 0
    for bf in bar_folders:
        dir = "/".join((os.environ["DATA_DIR"], "bars", bf))
        filepath = dir+"/volsum.txt"
        if not os.path.exists(filepath):
            util.error("Failed to locate volume summary {}".format(filepath))
            continue
        daysUsed += 1
        with open(filepath, "r") as f:
            header = f.readline().strip().split("|")[1:]
            for line in f:
                tokens = line.strip().split("|")
                secid = tokens[0]
                if not secid in secid2tickers: continue
                relvol = secid2relvol.get(secid, None)
                if relvol is None:
                    relvol = {}
                    secid2relvol[secid] = relvol
                for exch, vol in zip(header, tokens[1:]):
                    relvol[exch] = relvol.get(exch, 0.0) + float(vol)
    
    #normalize per day
    for secid, relvol in secid2relvol.iteritems():
        total = sum([vol for exch, vol in relvol.iteritems()])
        for exch in relvol.keys():
            relvol[exch] = "{:.2f}".format(100.0 * relvol[exch] / total if total >0 else 0.0)
        secid2dailyvol[secid] = "{:.3f}".format(1.0 * total / daysUsed / 1000)
        
    return secid2relvol, secid2dailyvol
    
def get_secid2tickers(file):
    res = dict()
    with open(file, "r") as f:
        for line in f:
            tokens = line.strip().split("|")
            secid = tokens[1]
            ticker = tokens[0]
            res[secid] = ticker
    return res

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--hist", action = "store", dest = "hist", default = 20)
    parser.add_argument("--date", action = "store", dest = "date", required = True)
    args = parser.parse_args()
    
    util.set_log_file()
    
    #get relevant securities
    run_dir = "/".join((os.environ["ROOT_DIR"], "run", os.environ["STRAT"], args.date))
    tickers_file = run_dir + "/tickers.txt"
    if not os.path.isfile(tickers_file):
        util.error("Failed to locate ticker file: {}".format(tickers_file))
        exit(1)
        
    secid2tickers = get_secid2tickers(tickers_file)
    secid2relvol, secid2dailyvol = get_vol_hist(args.date, secid2tickers, int(args.hist))
    
    with open(run_dir+"/exec/volume_distr", "w") as vdfile:
        #create header. get the exchanges first
        exchanges = None
        for secid, relvol in secid2relvol.iteritems():
            exchanges = sorted(relvol.keys())
            vdfile.write("#symbol,\t" + ",\t".join(exchanges) + ",\tTOTAL(K shs)\n")
            break
        for secid, relvol in secid2relvol.iteritems():
            data = []
            ticker = secid2tickers[secid]
            data.append(ticker)
            for exch in exchanges:
                data.append(relvol[exch])
            total = secid2dailyvol[secid]
            data.append(total)
            vdfile.write(",\t".join(data)+"\n")

if __name__ == "__main__":
    main()
