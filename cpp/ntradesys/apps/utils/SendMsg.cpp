#include "myUMS.h"
#include <Configurable.h>

#include <string>

using namespace std; 

int main(int argc,char *argv[]){
  
  
    CmdLineFileConfig cfg(argc, argv, "config,C");
    string host;
    int port;
    string msg1;
    string msg2;
    int code;
    bool help;
    int strat;

    cfg.defOption("help,h", &help, "print this help message");
    cfg.defOption("host", &host, "Host");
    cfg.defOption("port", &port, "port");
    cfg.defOption("strategy", &strat, "strategy");
    cfg.defOption("code", &code, "code");
    cfg.defOption("msg1", &msg1, "msg1");
    cfg.defOption("msg2", &msg2, "msg2");

    if (!cfg.configure()) { cerr << cfg << endl; return 1; }
    if (help) { cerr << cfg << endl; return 1; }
    UMS um(host.c_str());
    um.AddPort(port);
    for (int i=0;i<10;i++){
      um.Send(strat,code,msg1.c_str(),msg2.c_str());
    }
}
