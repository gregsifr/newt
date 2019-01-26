#!/usr/bin/env python
import os
import sys
import util
from gzip import GzipFile
from data_sources import file_source
import datafiles

def main():
    util.check_include()
    util.set_log_file()
    
    #get last calcres of previous day
    prevDay = util.exchangeTradingOffset(os.environ['PRIMARY_EXCHANGE'],os.environ['DATE'],-1)
    fs = file_source.FileSource(os.environ['RUN_DIR'] + '/../' + str(prevDay) + '/calcres')
    calcresFiles = fs.list(r'calcres.*\.txt\.gz')
    if len(calcresFiles) == 0:
        util.error("Failed to locate calcres file")
        sys.exit(1)
        
    calcresFiles.sort(key=lambda x: x[0], reverse=True)
    lastCalcresFile = os.environ['RUN_DIR'] + '/../' + str(prevDay) + '/calcres/' + calcresFiles[0][0]    
    
    secidParams = {}
    for line in GzipFile(lastCalcresFile, 'r'):
        if line.startswith('FCOV'): continue
        secid, name, datatype, datetime, value, currency, born = line.split('|')
        if int(secid) not in secidParams:
            secidParams[int(secid)] = {}
        if name == 'F:BBETA':
            secidParams[int(secid)]['BBETA'] = float(value)
        elif name == 'F:ASE_BETA90':
            secidParams[int(secid)]['ASE_BETA'] = float(value)
        elif name == 'CAPITALIZATION': 
            secidParams[int(secid)]['CAP'] = float(value)
    
    #get tickers
    tic2sec, sec2tic = datafiles.load_tickers(os.environ['RUN_DIR'] + '/tickers.txt')

    etfbetaFilename = os.environ['RUN_DIR'] + '/exec/etfbetafile'
    etfbetaFile = open(etfbetaFilename, 'w')
    etfbetaFile.write('#ETF  BETA  MKT-CAP\n')
    tickers = tic2sec.keys()
    tickers.sort()
    count = 0    
    for ticker in tickers:
        secid = tic2sec.get(ticker, None)
        if secid not in secidParams:
          continue
        bbeta = secidParams[secid].get('BBETA', None)
        asebeta = secidParams[secid].get('ASE_BETA', None)
        mcap = secidParams[secid].get('CAP', None)
        
        if bbeta is None or asebeta is None or mcap is None:
            util.error('Error while getting data for secid {}: ticker={}, bbeta={}, asebeta={}, mcap={}'.format(secid, ticker, bbeta, asebeta, mcap))
            continue
        
        beta = 0.5 * (bbeta + asebeta)
        etfbetaFile.write('{0},SPY,{1:.3f},{2:.3f}\n'.format(ticker,beta,mcap))
        count += 1
    
    etfbetaFile.close()       
    print 'Finished writing etfbeta file: {} for {} tickers'.format(etfbetaFilename, count)

if __name__ == '__main__':
    main()
