#!/usr/bin/env python

import sys
import string

def tv2sec(tv):
    t=map(float,string.split(tv,':'))
    return int((t[0]*60+t[1])*60 + t[2])
          
class req:
    def __init__(self,d):
        if (len(d)>7):
            self.load(d)
    def load(self,d):
        self.n=0
        self.mpx=0
        self.oldtgt=int(d[3])
        self.pos=int(d[5])
        self.tgt=int(d[4])
        self.mpx = 0.5* (float(d[8]) + float(d[7]))
        self.spd = abs(float(d[8]) - float(d[7]))
        self.side = (self.tgt<0)
        self.tv = d[0]


    
clog=open(sys.argv[1],'r')
reqlist={}
for l in clog.readlines():
    d=string.split(l)
    if (d[1]=="REQ"):
        sym=d[2]
        rnew = req(d)
        if (sym in reqlist.keys()):
            # We've an old request, generate a timeslice request
            r=reqlist[sym]
            dt = tv2sec(rnew.tv) - tv2sec(r.tv)
            print "%s,%s,%d,%d"%(sym,r.tv,r.tgt-r.pos,dt-1)
        reqlist[sym]=rnew
            
