#!/usr/bin/env python

import sys
import string

def  usage():
    sys.exit(0)
    
f=open(sys.argv[1],'r')
try:
    p=sys.argv[2]
except:
    p="1.0"

try:
    flip = int(sys.argv[3])
    if (flip<>-1):
        flip=1
except:
    flip=1
stkname = []
vol = []
for d in f.readlines():
    if (d[0]=='#'):
        continue
    l=string.split(d,',')
    stkname.append(string.strip(l[0]))
    v = int(3*float(l[17])/650.0)*100
    vol.append(v) # 3% of the period volume
 
for b in range(6):
    for i in range(len(stkname)):
        print "%s,%d:00:0%d.000,%d,3540,%s"%(stkname[i],(b+10),b,flip*vol[i],p)
