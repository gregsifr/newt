#ifndef _CONFIGSIMTRADER_H_
#define _CONFIGSIMTRADER_H_

#include <Client/lib2/SimMasterTrader.h>

class ConfigSimTrader : public SimExtendedTrader {

     int seq, conf, live, fill, cxl;
    
    void fillMktMdl ( MarketModel *mm ) {
        mm->ttSeq    = TimeVal(0, seq);
        mm->ttConf   = TimeVal(0, conf);
        mm->ttLive   = TimeVal(0, live);
        mm->ttNoLive = TimeVal(0, cxl);
        mm->ttMktReply = TimeVal(0, fill);
    }

    // GVNOTE: Remove Lime brokerage stuff from the code below.

    public:
  ConfigSimTrader(const char *logfile, CIndex &ci, AggrDataFileSource *a,
                       EventCBBase *ecb, TickRegistry *tr, OrderManager *m,
                           MasterBook *mstrb, int nolog, 
                           int seq_, int conf_, int live_, int fill_, int cxl_,
                           const char *acct = NULL)
    : SimExtendedTrader(logfile, ci, a, ecb, tr, m, mstrb->isld, mstrb->l2, mstrb->lime, mstrb->nyse,nolog,  acct, mstrb->cme2fastbook), seq(seq_), conf(conf_), live(live_), fill(fill_), cxl(cxl_)
  {
     ISLDMarketModel *isldm = new ISLDMarketModel(this); fillMktMdl(isldm);
     LimeMarketModel *tracm = new LimeMarketModel(this,ecb, TRAC); fillMktMdl(tracm);
     LimeMarketModel *arcam = new LimeMarketModel(this,ecb, ARCA); fillMktMdl(arcam);
     LimeMarketModel *edgxm = new LimeMarketModel(this, ecb, EDGX); fillMktMdl(edgxm);
     LimeMarketModel *edgam = new LimeMarketModel(this, ecb, EDGA); fillMktMdl(edgam);
     LimeMarketModel *btrdm = new LimeMarketModel(this, ecb, BTRD); fillMktMdl(btrdm);
     LimeMarketModel *batsm = new LimeMarketModel(this, ecb, BATS); fillMktMdl(batsm);

     setModel(ISLD, isldm);
     setModel(TRAC, tracm);
     setModel(ARCA, arcam);
     setModel(EDGX, edgxm);
     setModel(EDGA, edgam);
     setModel(BTRD, btrdm);
     setModel(BATS, batsm);
     setModel(LSE, new LimeMarketModel(this, ecb, LSE));
     setModel(EXCHG, new NyseMarketModel(this,ecb));
     setModel(EX_NYSE, new NyseMarketModel(this,ecb));
     setModel(SILK, new NyseMarketModel(this,ecb));
     setModel(WDSB, new NyseMarketModel(this,ecb));
     setModel(TCCC, new NyseMarketModel(this,ecb));
     setModel(LIME_NYSE, new NyseMarketModel(this,ecb));
     setModel(CME2FAST, new CME2FastMarketModel(this, ecb));
  }
};


#endif
