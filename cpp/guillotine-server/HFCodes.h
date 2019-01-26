#ifndef __HFCODES_H__
#define __HFCODES_H__


namespace guillotine { namespace server {
    namespace defoption {
        static const int BaseCode = 2000300;
        static const int StopCode = 100;
        static const int ECNCode  = 200;
        static const int RateCode  = 300;
        static const int ReloadCode  = 400;
        static const int CapCode = 500;
        static const int PcrCode = 600;
        static const int PnlCode = 700;
        static const int TcCode = 800;
        static const int InvCode = 900;
        static const int CrossCode = 1000;
        static const int ModelCode = 1100;
        static const int ScaleCode = 1200;
        static const int ChunkCode = 1300;
        static const int POPlusCode = 1400;
        static const int IgnoreTickCode = 1500;
        static const int SyncUpdateCode = 1600;
        static const int OrderProbCode = 1700;
    }
}
}
#endif
