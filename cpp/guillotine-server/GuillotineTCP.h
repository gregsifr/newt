#ifndef _GUILLOTINE_TCP_H_
#define _GUILLOTINE_TCP_H_

#include "mxdeque.h"
#include <guillotine/typed_message.h>
#include <guillotine/yaml_message.h>

#include <Util/Socket.h>
#include <Util/SelectFactory.h>
#include <c_util/Thread.h>
using trc::compat::util::Thread;

#include <tael/Log.h>
#include <tael/FdLogger.h>

#include <Configurable.h>

#include <boost/iterator/transform_iterator.hpp>
#include <boost/variant.hpp>
#include <boost/shared_array.hpp>
#include <functional>

using namespace trc;

#define MAX_BINARY_BUFFER_FILE_SIZE (1 << 26)

namespace guillotine {
    namespace server {

    typedef mxdeque<typed::request>::deque reqdeque;
    typedef mxdeque<typed::response>::deque rspdeque;

template <typename P> 
struct second_fn : public std::unary_function<P &, typename P::second_type &> {
    typename P::second_type &operator()(P & p) const { return p.second; }
};

template <typename map> 
struct value_iterator { 
    typedef boost::transform_iterator<second_fn<typename map::value_type>, typename map::iterator> type;
};

struct ClientInfo {

	static const int NO_CLIENT_ID;
	static const int BROADCAST;
	static int CLIENT_INFO_ID_POOL;
	static int getNextClientInfoId();

    enum status {
        preopen,
        open,
        blocked,
        error,
        closed
    } status_;

    struct req_handler : public boost::static_visitor<> {
        ClientInfo &ci;
        reqdeque &reqs;
        req_handler(ClientInfo &ci, reqdeque &reqs ) : ci(ci), reqs(reqs) { }
        void operator() ( const typed::connect &req );
        void operator() ( const typed::trade &req );
        void operator() ( const typed::stop &req );
        void operator() ( const typed::halt &req );
        void operator() ( const typed::resume &req );
        void operator() ( const typed::status &req );
    };

    struct get_req : public std::unary_function<yaml::message, void> {
        ClientInfo &ci;
        reqdeque &reqs;
        get_req ( ClientInfo &ci, reqdeque &reqs ) : ci(ci), reqs(reqs) { }
        void operator () ( yaml::message &m ) {
            try {
                req_handler rh(ci, reqs);
                typed::message t(typed::get_message(m, ci.id()));
                std::string temp = typed::show(t);
                TAEL_PRINTF(&ci.log, TAEL_INFO, " <- %s", temp.c_str());
                typed::request r(typed::get_request(t));
                boost::apply_visitor(rh, r);
            } catch (typed::parse_error &pe) {
                ci.sendm(typed::response(pe.msg));
            }
        }
    };

    struct rsp_to_yaml : public std::unary_function<typed::response, yaml::message> {
        yaml::message operator() ( const typed::response &m) const {
            return typed::put_message(m);
        }
    };

    status sendm ( const typed::response &m ) {
    	std::string temp = typed::show(m);
        TAEL_PRINTF(&log, TAEL_INFO, " -> %s", temp.c_str());
        if (ye.send(typed::put_message(m)) == 1)
            status_ = open;
        else if (wtr->error())
            status_ = error;
        else 
            status_ = blocked;
        return status_;
    }
    Socket *s;

    bool listenToBcast;
    int client_id;
    std::string client_name;
    std::string account_;
    std::string password_;

    boost::shared_ptr<yaml::socket_reader> rdr;
    boost::shared_ptr<yaml::socket_writer> wtr;
    yaml::parser yp;
    yaml::emitter ye;

    boost::shared_ptr<tael::LoggerDestination> ld;
    tael::Logger log;

    public:

    const std::string &name() const { return client_name; }
    const int &id() const { return client_id; }
    const std::string &account() const { return account_; }
    ClientInfo ( Socket *s, boost::shared_ptr<tael::LoggerDestination> ld,
            const std::string &acct, const std::string &pass ) : status_(preopen),
        s(s), account_(acct), password_(pass),
        rdr(new yaml::socket_reader(s)), wtr(new yaml::socket_writer(s)),
        yp(rdr), ye(wtr),
        ld(ld), log(*(new tael::LoggerConfiguration((size_t) MAX_BINARY_BUFFER_FILE_SIZE)))
    { 
    	client_id = getNextClientInfoId();
        log.addDestination(ld.get());
        TAEL_PRINTF(&log, TAEL_INFO, "Client file open and logging for new client... ID assigned is %d", client_id);
    }

    typedef boost::transform_iterator<rsp_to_yaml, std::deque<typed::response>::iterator> it_yaml;

    status receive ( reqdeque &reqs );
    status send    ( rspdeque &rsps, bool trimIrrelevantRsps, int *numOfRelRsps, int *relRspsSent, std::vector<bool>& sentToSomeClient) {
    	std::deque<typed::response> relRsps;
    	*numOfRelRsps = 0;
    	int i = 0;
        for (rspdeque::const_iterator r = rsps.begin(); r != rsps.end(); ++r, i++) {
        	std::pair<bool, int> relevant = typed::isRelevant(this->client_id, this->listenToBcast, *r);
        	if (trimIrrelevantRsps && !relevant.first) {
        		std::string temp = typed::show(*r);
        		TAEL_PRINTF(&log, TAEL_INFO, " Pruned response %s. It was directed to clientId %d and not %d", temp.c_str(), relevant.second, this->client_id);
        		continue;
        	}
        	relRsps.push_back(*r);
        	sentToSomeClient[i] = true;
        	std::string temp = typed::show(*r);
            TAEL_PRINTF(&log, TAEL_INFO, " -> %s", temp.c_str());
            (*numOfRelRsps)++;
        }

        *relRspsSent = ye.send(it_yaml(relRsps.begin()), it_yaml(relRsps.end()));
        if (*relRspsSent != *numOfRelRsps) {
            if (wtr->error())
                status_ = error;
            else 
                status_ = blocked;
        } else {
            status_ = open;
        }
        return status_;
    }

    ~ClientInfo ( ) 
    {
        TAEL_PRINTF(&log, TAEL_INFO, "Client connection closing.");
    }
};

class ServerThread : public Thread, public Configurable {

    static const int errbufLen = 80;
    char errbuf[errbufLen];

    mxdeque<typed::request> &reqx;
    mxdeque<typed::response> &rspx;


    reqdeque my_reqs;
    rspdeque my_rsps;

    Select *sel;
    TCPServerSocket *srv;

    int port, seltimeout;
    std::string sdebugfile, tdebugfile;
    std::string account;
    std::string password;

    typedef std::map<Socket *, boost::shared_ptr<ClientInfo> > cmap;
    cmap clients;
    
    bool stopping;

    std::string elogfile, clogpfx;
    boost::shared_ptr<tael::LoggerDestination> ld;
    tael::Logger log;

    std::vector<std::string> client_str;
    std::vector<struct in_addr> client_ip;
    bool restrict_ip;


    protected:

        virtual void *run ();
        virtual void *onKill ();
        void closeout ();
        void handleInteract ();
    bool allow_ip(const struct sockaddr_in *sin);

    public: 

        ServerThread ( mxdeque<typed::request> &reqx,
                mxdeque<typed::response> &rspx);
        virtual ~ServerThread ( );
        void setLoggerDestination(boost::shared_ptr<tael::LoggerDestination> ld);
        virtual void stop ( ) { stopping = true; }
};
}
}

#endif
