"""
Generate a tradeoff slip/opp curve for a stock
"""

import string
BASE_DIR="/spare/local/apratap/costanalysis"
import sys

class fill:
    def __init__(self,d):
        self.n=abs(int(d[5]))
        self.fpx = float(d[6])
        self.side = 2 * (int(d[5])>0) - 1
        self.add = (d[-1]=="add")
        return

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
        
class day:
    def __init__(self):
        self.cost=[0,0]
        self.opp=[0,0]
        self.n=[0,0]
        self.dol=[0,0]
        self.liq=[0,0]
        self.cleanup=[0,0]
    def __str__(self):
        return( "%d %.2f %.2f %.2f %d %.2f %.2f %.2f"%(self.n[0],self.dol[0],self.cost[0],self.opp[0],self.n[1],self.dol[1],self.cost[1],self.opp[1]))
    
    def __iadd__(self,other):
        for i in range(2):
            self.cost[i]+=other.cost[i]
            self.opp[i]+=other.opp[i]
            self.n[i]+=other.n[i]
            self.dol[i]+=other.dol[i]
            self.liq[i]+=other.liq[i]
            self.cleanup[i]+=other.cleanup[i]
        return self
    
    def adjust(self,base):
        # Adjust opp cost relative to base
        self.opp[0]-=base.opp[0]
        self.opp[1]-=base.opp[1]
        self.cleanup[0]-=base.cleanup[0]
        self.cleanup[1]-=base.cleanup[1]
        return
    def addfill(self,f,r):
        idx = 1 - (f.side+1)/2
        cost[idx]+= f.side*f.n*(f.fpx - r.mpx)
        n[idx]+= f.n
        dol[idx]+=f.n*r.mpx
        liq[f.add]+=f.n
    
def analyze(fname,endmpx=0.0):
    ret = day()
    try:
        f=open(fname,'r')
    except:
        print "Could not open %s"%fname
        return ret
    r=req([])
    mpx = 0
    stopped = 0
    pos = 0
    side = 0
    for l in f.readlines():
        d = string.split(l)
        if (d[1]=="REQ"):
            r.load(d)
            if (pos<>r.pos):
                # position mismatch for buys, process a fake stop before processing this request
                ret.opp[side]+=(tgt-pos)*(mpx - endmpx)
                pass
            ret.opp[r.side]+= (r.oldtgt - r.pos) * (r.mpx-mpx) 
            ret.cleanup[r.side]+= abs(r.oldtgt - r.pos) * r.spd*0.5
            mpx=r.mpx
            pos = r.pos
            side = r.side
            tgt = r.tgt
                
              
        if (d[1]=="FILL"):
            f = fill(d)
            idx = 1 - (f.side+1)/2
            ret.cost[idx]+= f.side*f.n*(f.fpx - r.mpx)
            ret.n[idx]+= f.n
            ret.dol[idx]+=f.n*r.mpx
            ret.liq[f.add]+=f.n
            pos+= f.n*f.side
    if (pos <> r.pos):
        # Missing Stop req for sells
        pass
    return(ret)

def process(symbol,daterange,cutoff_range):
    costs=[]
    for i in range(len(cutoff_range)):
        costs.append(day())
    for d in daterange:
        for i in range(len(cutoff_range)):
            costs[i]+=analyze("%s/v%s/Cost.%s.%s.%s"%(BASE_DIR,cutoff_range[i],symbol,d,cutoff_range[i]))
    #rprint(cutoff_range,costs)
    for i in range(1,len(cutoff_range)):
        costs[i].adjust(costs[0])
    costs[0].adjust(costs[0])
    print "Adjusted Costs:"
    rprint(cutoff_range,costs)
    return costs

def rprint(crange,costs):
    nall = sum(costs[0].n)
    for i in range(len(crange)):
        n=sum(costs[i].n)
        d=sum(costs[i].dol)
        s=1e4*sum(costs[i].cost)/d
        o=1e4*sum(costs[i].opp)/d
        cl=1e4*sum(costs[i].cleanup)/d
        s=int(s*100)/100.0
        o=int(o*100)/100.0
        loss=int((s+o)*d*1e-2)/100.0
        print "%4.2f %5d %2.2f %7.2f %1.2f %1.2f %4.2f"%( float(crange[i]),n,n*100.0/nall,d,s,o,loss)


def getEndpx(fname):
    epx=[-1,-1]
    try:
        f=open(fname,'r')
    except:
        print "Could not open %s"%fname
        return epx
    i=0
    for l in f.readlines():
        d = string.split(l)
        if (d[1]<>"REQ"):
            pass
        if (d[0][0:7]=="15:59:0"):
            r=req(d)
            e[i]=r.mpx
            i+=1
    
        
crange=["-100","-3","-2.5","-2","-1.5","-1","-0.5","0","0.5","1","1.5","2"]

date_range=["20100104","20100105","20100106","20100107","20100108","20100111","20100112","20100113","20100114","20100115","20100119","20100120","20100121","20100122","20100125","20100126"]
date_range1=date_range[0:7]
date_range2=date_range[7:]


if __name__=="__main__":
    process(sys.argv[1],date_range,crange)
    process(sys.argv[1],date_range1,crange)
    process(sys.argv[1],date_range2,crange)
    for d in date_range:
        print d, " ",
        process(sys.argv[1],[d],crange)
