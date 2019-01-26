#include "GuillotineTCP.h"
#include <map>

#include <string.h>
#include <cstdlib>

using namespace std;
namespace guillotine {
    namespace server {

    	const int ClientInfo::NO_CLIENT_ID = -2;
    	const int ClientInfo::BROADCAST = -1;
    	int ClientInfo::CLIENT_INFO_ID_POOL = 0;

		int ClientInfo::getNextClientInfoId() {
			return ++ClientInfo::CLIENT_INFO_ID_POOL;
		}

        void ClientInfo::req_handler::operator() ( const typed::connect &req ) {
            if (ci.status_ != preopen) {
                typed::error e;
                e.reason = typed::error::bad_message;
                e.message_name = "connect";
                e.info = "Connect message sent after connection established.";
                ci.sendm(typed::response(e));
                ci.status_ = error;
                return;
            }
            if (ci.password_ != req.password || ci.account_ != req.account) {
                typed::error e;
                e.reason = typed::error::bad_field;
                e.message_name = "connect";
                e.info = "incorrect account or password supplied.";
                ci.sendm(typed::response(e));
                ci.status_ = error;
                return;
            }

            ci.account_ = req.account;
            if (req.name.empty()) {
                ci.client_name = string("anonymous ") + req.account;
                TAEL_PRINTF(&ci.log, TAEL_INFO, "Thou art anonymous: I christen thee \"%s\"",
                        ci.client_name.c_str());
            } else
                ci.client_name = req.name;
            ci.listenToBcast = (req.listenToBcast > 0)? true : false;
            TAEL_PRINTF(&ci.log, TAEL_INFO, "%s.", (ci.listenToBcast)? "Client listens to broadcast" : "Client does NOT listen to broadcast");

            reqs.push_back(typed::request(req));
            ci.status_ = open;
            //return open;
            
        }
    
        void ClientInfo::req_handler::operator() ( const typed::trade &req ) {
            if (ci.status_ != open) {
                typed::error e;
                e.reason = typed::error::bad_message;
                e.message_name = "trade";
                e.info = "Trade message sent without connect or after error.";
                ci.sendm(typed::response(e));
                ci.status_ = error;
                return;
            }
            reqs.push_back(typed::request(req));
            //return ci.status_;
        }

        void ClientInfo::req_handler::operator() ( const typed::stop &req ) {
            if (ci.status_ != open) {
                typed::error e;
                e.reason = typed::error::bad_message;
                e.message_name = "stop";
                e.info = "Trade message sent without connect or after error.";
                ci.sendm(typed::response(e));
                ci.status_ = error;
                return;
            }
            reqs.push_back(typed::request(req));
            //return ci.status_;

        }
        void ClientInfo::req_handler::operator() ( const typed::halt &req ) {
            if (ci.status_ != open) {
                typed::error e;
                e.reason = typed::error::bad_message;
                e.message_name = "halt";
                e.info = "Halt message sent without connect or after error.";
                ci.sendm(typed::response(e));
                ci.status_ = error;
                return;
            }
            reqs.push_back(typed::request(req));
            //return ci.status_;

        }
    void ClientInfo::req_handler::operator() ( const typed::resume &req ) {
        if (ci.status_ != open) {
            typed::error e;
            e.reason = typed::error::bad_message;
            e.message_name = "resume";
            e.info = "Resume message sent without connect or after error.";
            ci.sendm(typed::response(e));
            ci.status_ = error;
            return;
        }
        reqs.push_back(typed::request(req));
        //return ci.status_;

    }
    void ClientInfo::req_handler::operator() ( const typed::status &req ) {
        if (ci.status_ != open) {
            typed::error e;
            e.reason = typed::error::bad_message;
            e.message_name = "status";
            e.info = "Status message sent without connect or after error.";
            ci.sendm(typed::response(e));
            ci.status_ = error;
            return;
        }
        reqs.push_back(typed::request(req));
        //return ci.status_;
    }

    ClientInfo::status ClientInfo::receive ( reqdeque &reqs ) {
        if (rdr->eof()) {
            status_ = closed;
        }
        get_req getr(*this, reqs);
        yp.receive(getr);
        return status_;
    }

ServerThread::ServerThread ( mxdeque<typed::request> &reqx, mxdeque<typed::response> &rspx) :
    Configurable("server"), reqx(reqx), rspx(rspx),/* ld(&tael::FdLogger::stdoutLogger()),*/ log(*(new tael::LoggerConfiguration((size_t) MAX_BINARY_BUFFER_FILE_SIZE)))
{
    defOption("port", &port, "port to bind to for client requests.");
    defOption("response-interval", &seltimeout, "minimum interval to check for responses (ms)", 50);
    defOption("client-log-dir", &clogpfx, "client log directory/file (client name appended)");
    defOption("account", &account, "account name");
    defOption("password", &password, "password for account.");
    defOption("client-ip", &client_str, "Acceptable client IP addresses (any if unspecified)");
    
    SelectFactory::selectImp(SelectFactory::SelectEpoll);
    srv = new TCPServerSocket();
    sel = SelectFactory::getInstance();
}

void ServerThread::setLoggerDestination(boost::shared_ptr<tael::LoggerDestination> ld) {
	this->ld = ld;
}

ServerThread::~ServerThread ( ) {
    delete srv;
    delete sel;

    closeout();
}

bool ServerThread::allow_ip ( const struct sockaddr_in *sin ) {
    if (!restrict_ip) return true;
    for (vector<struct in_addr>::iterator i = client_ip.begin(); i != client_ip.end(); ++i) {
        if (i->s_addr == sin->sin_addr.s_addr)
            return true;
    }

    return false;
}

void *ServerThread::run ( ) {

    stopping = false;
    if (!configured("port") || !srv->Bind(port)) {
        return 0;
    }
    log.addDestination(ld.get());

    sel->add(srv, (SelectMode) (SelectRead | SelectWrite | SelectError));
    sel->settimeout(seltimeout);

    if (configured("client-ip")) {
        restrict_ip = true;
        struct in_addr ina;
        for (vector<string>::iterator i = client_str.begin(); i != client_str.end(); ++i) {
            if (getAddr(&ina, i->c_str())) {
                client_ip.push_back(ina);
                TAEL_PRINTF(&log, TAEL_INFO, "Client restriction enabled: %s allowed.", i->c_str());
            } else {
                TAEL_PRINTF(&log, TAEL_ERROR, "Can't parse %s for client restriction!", i->c_str());
            }
        }
    } else {
        TAEL_PRINTF(&log, TAEL_ERROR, "Client restrictions not enabled.");
        restrict_ip = false;
    }

    while (!stopping) {
        Socket *s;
        SelectMode mode;
        while ((s = sel->next(&mode)) != 0) {

            if (s == srv) {
                if (mode != SelectRead) {
                    TAEL_PRINTF(&log, TAEL_ERROR, "Server socket broke! Trying to clean up...");
                    stopping = true;
                    sel->remove(srv);
                    break;
                }

                struct sockaddr_in nhost;
                Socket *ns = srv->Accept(&nhost);
                const char *addr = getHost(&nhost);
                uint16_t port = ntohs(nhost.sin_port);
                if (ns != 0) {
                    if (!allow_ip(&nhost)) {
                        TAEL_PRINTF(&log, TAEL_ERROR, "Client connection from %s not allowed, closing", addr);
                        ns->close();
                        delete ns;
                    } else {
                        string filename = string(getenv("EXEC_LOG_DIR")) + string("/") + clogpfx + string("/") + string(addr) + string(":")
                            + boost::lexical_cast<string>(port);

                        int fd = open(filename.c_str(), O_RDWR | O_CREAT | O_APPEND, 0640);
                        if (fd <= 0) {
                            TAEL_PRINTF(&log, TAEL_ERROR, "Failed to open logfile %s for client %s:%u",
                                    filename.c_str(), addr, port);
                        }
                        boost::shared_ptr<tael::FdLogger> fld ( new tael::FdLogger(fd) );
                        boost::shared_ptr<ClientInfo> ci ( new ClientInfo(ns, fld, account, password ) );
                        clients.insert(make_pair(ns, ci));
                        //ns->setNonBlock();
                        sel->add(ns, (SelectMode)(SelectRead | SelectError));
                        TAEL_PRINTF(&log, TAEL_INFO, "Accepted connection from %s:%u", addr, port);
                    }
                } else {
                    TAEL_PRINTF(&log, TAEL_ERROR, "Server socket failed to accept connection from %s:%u.", addr, port);
                }

            } else {
                int s_err = 0;
                s->getIntSockOpt(SO_ERROR, &s_err);
                cmap::iterator cit = clients.find(s);
                if (cit != clients.end()) {
                    if (mode == SelectRead && s_err == 0) {
                        int n = my_reqs.size();
                        int n2 = 0;
                        ClientInfo::status status = cit->second->receive(my_reqs);
                        switch (status) {
                            case ClientInfo::blocked:
                                cit->second->wtr->write_bytes();
                            case ClientInfo::preopen:
                            case ClientInfo::open:
                                n2 = my_reqs.size();
                                if (n2 > n) {
                                    TAEL_PRINTF(&log, TAEL_INFO, "Client %s sends %d requests (%d prev in queue)",
                                            cit->second->name().c_str(), n2 - n, n);
                                }
                                break;
                            case ClientInfo::error:
                                TAEL_PRINTF(&log, TAEL_ERROR, "Client %s is confused, closing.", cit->second->name().c_str());
                                sel->remove(s);
                                s->close();
                                clients.erase(cit);
                                delete s;
                                break;
                            case ClientInfo::closed:
                                TAEL_PRINTF(&log, TAEL_INFO, "Client %s is leaving, closing.", cit->second->name().c_str());
                                sel->remove(s);
                                s->close();
                                clients.erase(cit);
                                delete s;
                                break;
                        }
                    } else if (mode == SelectWrite && s_err == 0) {
                        cit->second->wtr->write_bytes();
                    } else {
                        char *my_errbuf = strerror_r(s_err, errbuf, errbufLen);
                        if (my_errbuf == 0) 
                            TAEL_PRINTF(&log, TAEL_ERROR, "Client %s error --- it's errors all the way down",
                                    cit->second->name().c_str());
                        else
                            TAEL_PRINTF(&log, TAEL_ERROR, "Client %s error: %s",
                                    cit->second->name().c_str(), my_errbuf);
                        sel->remove(s);
                        s->close();
                        clients.erase(cit);
                        delete s;
                    }
                } else {
                    TAEL_PRINTF(&log, TAEL_ERROR, "Select returned surprise socket FD #%d", s->getFD());
                    sel->remove(s); /// ???
                    s->close();     /// ???
                }
            }
            handleInteract();
        }
        handleInteract();
    }

    return 0;
}

void ServerThread::handleInteract() {
    
    if (!my_reqs.empty()) {
        int n = my_reqs.size();
        if (reqx.swap(my_reqs)) {
            TAEL_PRINTF(&log, TAEL_INFO, "Sent %d requests to trading thread.", n);
        } else {
            TAEL_PRINTF(&log, TAEL_ERROR, "Trading thread not ready for requests.");
        }
    }

    rspx.swap(my_rsps);
    if (!my_rsps.empty()) {
        int total;
        int sent;
        std::vector<bool> sentToSomeClient(my_rsps.size(), false);
        for (cmap::iterator cit = clients.begin(); cit != clients.end(); ++cit) {
            if (cit->second->account() == account && cit->second->status_ == ClientInfo::open) {
                if (cit->second->send(my_rsps, true, &total, &sent, sentToSomeClient) != ClientInfo::error)
                    TAEL_PRINTF(&log, TAEL_INFO, "Sent %d responses to client %s.", sent, cit->second->name().c_str());
                else
                    TAEL_PRINTF(&log, TAEL_ERROR, "Failed to send all %d responses to client %s. Only %d sent.", total, cit->second->name().c_str(), sent);
            }
        }
        for (unsigned int i = 0; i < my_rsps.size(); i++) {
        	if(sentToSomeClient[i] == false) {
        		TAEL_PRINTF(&log, TAEL_CRITICAL, "Failed to send some response to any client. Possible error or disconnection.");
        	}
        }
        my_rsps.clear();
    }
}

void ServerThread::closeout ( ) {
    /*
    for (cmap::iterator cit = clients.begin(); cit != clients.end(); ++cit) {
        cit->second.send(value_iterator<rspmap>::type(lastrsp.begin()), 
                value_iterator<rspmap>::type(lastrsp.end()));
        cit->first->close();
        sel->remove(cit->first);
        clients.erase(cit);
    }
    */
}

void *ServerThread::onKill ( ) {
    closeout();
    return 0;
}

}}
