#!/usr/bin/env python
from math import log
import os
import util

def linreg(X, Y):
    """
    Summary
        Simple linear regression : y = ax + b
    Usage
        (a,b,R2) = linreg(x, y)
    Returns coefficients and R^2 value for the regression line y=ax+b, given x[] and y[]
    """
    if len(X) != len(Y):  raise ValueError, 'unequal length'
    N = len(X)
    Sx = Sy = Sxx = Syy = Sxy = 0.0
    for x, y in map(None, X, Y):
        Sx = Sx + x
        Sy = Sy + y
        Sxx = Sxx + x*x
        Syy = Syy + y*y
        Sxy = Sxy + x*y
    det = Sxx * N - Sx * Sx
    a, b = (Sxy * N - Sy * Sx)/det, (Sxx * Sy - Sx * Sxy)/det
    meanerror = residual = 0.0
    for x, y in map(None, X, Y):
        meanerror = meanerror + (y - Sy/N)**2
        residual = residual + (y - a*x - b)**2
    if meanerror > 0:
      RR = 1 - residual/meanerror
    else:
      print "Warning: meanerror = 0. Num points = " + str(len(X))
      RR = 1
    ss = residual / (N-2)
    Var_a, Var_b = ss * N / det, ss * Sxx / det
    #print "N= %d" % N
    #print "a= %g \\pm t_{%d;\\alpha/2} %g" % (a, N-2, sqrt(Var_a))
    #print "b= %g \\pm t_{%d;\\alpha/2} %g" % (b, N-2, sqrt(Var_b))
    #print "R^2= %g" % RR
    #print "s^2= %g" % ss
    return a, b, RR

def main():
  util.check_include()
  util.set_log_file()

  tickersFile = open(os.environ['RUN_DIR'] + '/tickers.txt', 'r')
  tickerLines = [line.strip().split('|') for line in tickersFile]
  tickersFile.close()
  sec2tic = {}
  tic2sec = {}
  for line in tickerLines:
    (ticker, secid) = (line[0], int(line[1]))
    sec2tic[secid] = ticker
    tic2sec[ticker] = secid

  prevDay = util.exchangeTradingOffset(os.environ['PRIMARY_EXCHANGE'],os.environ['DATE'],-1)
  tradeSzFile = open(os.environ['DATA_DIR'] + '/bars/' + str(prevDay) + '/tradeSz.txt', 'r')
  tradeSzLines = [line.strip().split('|') for line in tradeSzFile]
  tradeSzFile.close()

  dataBySecID = {}
  allSizes = {}
  for line in tradeSzLines[1:]:
    (secid, size, count) = (int(line[0]), int(line[1]), int(line[2]))
    if secid not in dataBySecID:
      dataBySecID[secid] = []
    dataBySecID[secid].append((size,count))
    if size not in allSizes:
      allSizes[size] = 0
    allSizes[size] += count

  klist = {}
  for secid in sec2tic.keys():
    if secid not in dataBySecID:
      klist[secid] = (-2,-2,-2,-2)
      continue
    dataBySecID[secid].sort(key=lambda x: x[1])
    keys = []; values = []; scaledKeys = []; logKeys = []; logValues = []
    for (key,value) in dataBySecID[secid]:
      if (key < 100) or (key > 10000): continue
      keys.append(key)
      values.append(value)
      scaledKeys.append(key/100.0)
      logKeys.append(log(key))
      logValues.append(log(value))
    klist[secid] = (-1,-1,-1,-1)
    for ii in range(-3,-1*(len(keys)+1),-1):
      (a,b,R2) = linreg(logKeys[ii:],logValues[ii:])
      if R2 == 1:
        print 'R2 = 1 for ' + sec2tic[secid] + ' for ' + str(-1*ii) + ' out of ' + str(len(keys))
      if R2 > 0.9:
        klist[secid] = (a,R2,-1*ii,len(keys))
      else:
        break

  # read previous day's klist file and compute an average
  prevKlistFile = open(os.environ['RUN_DIR'] + '/../' + str(prevDay) + '/exec/klist', 'r')
  prevKlistLines = [line.strip().split(',') for line in prevKlistFile]
  prevKlistFile.close()

  prevKlist = {}
  # ignore header
  for line in prevKlistLines[1:]:
    prevKlist[line[0]] = float(line[1])

  klistFilename = os.environ['RUN_DIR'] + '/exec/klist'
  outFile = open(klistFilename, 'w')
  #outFile.write('Stock,K,R2,Reg,Tot\n')
  outFile.write('Stock,K,R2\n')
  tickers = tic2sec.keys()
  tickers.sort()
  count = 0
  notInPrev = 0
  for ticker in tickers:
    secid = tic2sec.get(ticker, None)
    if secid not in klist:
      continue
    kPoints = klist[secid][2]
    if kPoints < 0:
      continue
    kValue = klist[secid][0]
    if ticker in prevKlist:
      kValue = 0.5 * (kValue + prevKlist[ticker])
    else:
      notInPrev += 1
    if kValue > -1:
      util.error('For {}, kValue = {}, learned today = {}, R2 = {}, points for today = {} out of {}\n'.format(ticker, kValue, klist[secid][0], klist[secid][1], klist[secid][2], klist[secid][3]))

    #outFile.write('{0},{1:.4f},{2:.4f}{3}{4}\n'.format(ticker, klist[secid][0], kValue, klist[secid][2], klist[secid][3])
    outFile.write('{0},{1:.4f},{2:.4f}\n'.format(ticker, kValue, klist[secid][1]))
    count += 1

  outFile.close()
  print 'Finished writing klist file: {} for {} tickers. {} tickers not present in previous klist file.'.format(klistFilename, count, notInPrev)

if __name__ == '__main__':
  main()

#from pylab import *
#truncRange = arange(1, 101, 1)
#s2400 = truncRange**(-2.4)
#a2400 = s2400 / sum(s2400)
#sNewMSFT = truncRange**(-2.155)
#aNewMSFT = sNewMSFT / sum(sNewMSFT)
#sOldMSFT = truncRange**(-1.1713)
#aOldMSFT = sOldMSFT / sum(sOldMSFT)
#sNewAAPL = truncRange**(-3.1095)
#aNewAAPL = sNewAAPL / sum(sNewAAPL)
#sOldAAPL = truncRange**(-1.7108)
#aOldAAPL = sOldAAPL / sum(sOldAAPL)
#aNewASNA = truncRange**(-3.449)
#sNewASNA = truncRange**(-3.449)
#aNewASNA = sNewASNA / sum(sNewASNA)
#sNewAUQ = truncRange**(-2.602)
#aNewAUQ = sNewAUQ / sum(sNewAUQ)
#sNewNEE = truncRange**(-2.744)
#aNewNEE = sNewNEE / sum(sNewNEE)
#
#aaplKeys = []; aaplValues = []; aaplScaledKeys = []
#for (key,value) in dataBySecID[239]:
#  aaplKeys.append(key)
#  aaplValues.append(value)
#  aaplScaledKeys.append(key/100.0)
#
#bins=[item/100.0 for item in range(10001)]
#(out1aapl,out2aapl,out3aapl) = hist(asarray(aaplScaledKeys), bins, facecolor='green', alpha=0.75, weights=asarray([item*1.0/sum(aaplValues) for item in aaplValues]))
#
#plot(truncRange,aNewAAPL,linewidth=1.0)
#plot(truncRange,aOldAAPL,linewidth=1.0)
