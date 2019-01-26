#include <guillotine/typed_message.h>

#include <typeinfo> 
#include <boost/iterator/transform_iterator.hpp>
#include <boost/lexical_cast.hpp>

using std::string;
using std::list;

namespace guillotine {
    namespace typed {

        template <typename T>
        bool fillf ( T &dest, const yaml::message &m, 
                const string &field, bool opt = false) {
            parse_error pe;
            if (m[field].empty()) {
                if (opt) { 
                    return false;
                } else {
                    pe.msg.reason = error::bad_field;
                    pe.msg.field_name = field;
                    pe.msg.info = string("required field missing in message ") + m.name();
                    throw pe;
                }
            }
            try {
                dest = boost::lexical_cast<T> ( 
                        boost::any_cast<string> (
                            m[field].value()));
            } catch ( std::exception &e ) {
                pe.msg.reason = error::bad_field;
                pe.msg.field_name = field;
                pe.msg.info = string("incorrect field type (expected ") + typeid(T()).name() +
                    string(") in field ") + field + string(" of message ") + m.name();
                throw pe;
            }
            return true;
        }

        template <typename T>
        bool fillf ( list<T> &dest, const yaml::message &m, 
                const string &field, bool opt = false ) {
            const string *s;
            const list<yaml::node> *ss;
            parse_error pe;
            if (m[field].empty()) {
                if (opt) { 
                    return false;
                } else {
                    pe.msg.reason = error::bad_field;
                    pe.msg.field_name = field;
                    pe.msg.info = string("required field missing in message ") + m.name();
                    throw pe;
                }
            }
            try {
                if ( (s = boost::any_cast<T>(&(m[field].value()))) ) {
                    dest.push_back(boost::lexical_cast<T>(*s));
                } else if ( (ss = boost::any_cast<list<yaml::node> >(&(m[field].value()))) ) {
                    for (list<yaml::node>::const_iterator i = ss->begin(); i != ss->end(); ++i)
                        dest.push_back(boost::lexical_cast<T>(i->as<string>()));
                }
            }
            catch ( std::exception &e ) {
                pe.msg.reason = error::bad_field;
                pe.msg.field_name = field;
                pe.msg.info = string("incorrect field type in message ") + m.name();
                throw pe;
            }
            return true;
        }

        message get_message ( const yaml::message &raw, int clientId ) {
            parse_error pe;

            if (raw.name().empty()) {
                pe.msg.reason = error::bad_message;
                pe.msg.info = "No message type specified";
                throw pe;
            }

            if (raw.name() == "connect") {
                connect c;
                fillf(c.account, raw, "account");
                fillf(c.name, raw, "name", true);
                fillf(c.password, raw, "password", true);
                fillf(c.listenToBcast, raw, "listenToBcast", true)? NULL : c.listenToBcast = 0;
                c.clientId = clientId;
                return message(c);
            }

            if (raw.name() == "trade") {
                trade t;
                fillf(t.symbol, raw, "symbol");
                fillf(t.aggr, raw, "aggr");
                if (!fillf(t.orderID, raw, "orderID", true)) {
                  t.orderID = -1;
                }
                fillf(t.qty, raw, "qty");
                if (!fillf(t.short_mark, raw, "short-mark", true)) {
                  t.short_mark = trade::unknown;
                }
                t.clientId = clientId;
                return message(t);
            }

            if (raw.name() == "stop") {
                stop s;
                s.all = !fillf(s.symbols, raw, "symbol", true);
                s.clientId = clientId;
                return message(s);
            }

            if (raw.name() == "halt") {
                halt s;
                s.all = !fillf(s.symbols, raw, "symbol", true);
                s.clientId = clientId;
                return message(s);
            }
            
            if (raw.name() == "resume") {
                resume s;
                s.all = !fillf(s.symbols, raw, "symbol", true);
                s.clientId = clientId;
                return message(s);
            }

            if (raw.name() == "status") {
                status s;
                s.all = !fillf(s.symbols, raw, "symbol", true);
                s.clientId = clientId;
                return message(s);
            }

            if (raw.name() == "server") {
                server s;
                fillf(s.name, raw, "name");
                fillf(s.symbols, raw, "symbol");
                s.clientId = clientId;
                return message(s);
            }

            if (raw.name() == "fill") {
                fill s;

                string tmstr, liqstr;
                fillf(tmstr, raw, "time");
                fillf(liqstr, raw, "liquidity", true);
                size_t dot = tmstr.find('.'); 
                try {
                    int sec = 0, usec = 0;
                    sec = boost::lexical_cast<int>(tmstr.substr(0, dot));
                    if (dot != string::npos)
                        usec = boost::lexical_cast<int>(tmstr.substr(dot+1, tmstr.size()));
                    s.time_sec = sec;
                    s.time_usec = usec;
                } catch (...) {
                    parse_error pe;
                    pe.msg.reason = error::bad_field;
                    pe.msg.field_name = "time";
                    pe.msg.info = string("required field missing in message fill");
                    throw pe;
                }

                if      (liqstr == "A") s.liquidity = fill::add;
                else if (liqstr == "R") s.liquidity = fill::remove;
                     else s.liquidity = fill::other;

                long orderID = -1;
                fillf(orderID, raw, "orderID", true);
                s.orderID = orderID;
                string strat = "U";
                fillf(strat, raw, "strat", true);
                s.strat = strat;
                fillf(s.symbol, raw, "symbol");
                fillf(s.exchange, raw, "exchange");
                fillf(s.qtyLeft, raw, "qtyLeft");
                fillf(s.fill_size, raw, "fill-size");
                fillf(s.fill_price, raw, "fill-price");
                s.clientId = clientId;

                return message(s);
            }

            if (raw.name() == "info") {
                info s;
                string tmstr;
                fillf(tmstr, raw, "time");
                size_t dot = tmstr.find('.'); 
                try {
                    int sec = 0, usec = 0;
                    sec = boost::lexical_cast<int>(tmstr.substr(0, dot));
                    if (dot != string::npos)
                        usec = boost::lexical_cast<int>(tmstr.substr(dot+1, tmstr.size()));
                    s.time_sec = sec;
                    s.time_usec = usec;
                } catch (...) {
                    parse_error pe;
                    pe.msg.reason = error::bad_field;
                    pe.msg.field_name = "time";
                    pe.msg.info = string("required field missing in message info");
                    throw pe;
                }
                fillf(s.symbol, raw, "symbol");
                fillf(s.position, raw, "position");
                fillf(s.qtyLeft, raw, "qtyLeft");
                fillf(s.locates, raw, "locates");
                fillf(s.aggr, raw, "aggr");
                fillf(s.halt, raw, "halt");
                if (!fillf(s.bid, raw, "bid", true)) s.bid = 0.0;
                if (!fillf(s.ask, raw, "ask", true)) s.ask = 0.0;
                if (!fillf(s.bidsz, raw, "bid-size", true)) s.bidsz = 0;
                if (!fillf(s.asksz, raw, "ask-size", true)) s.asksz = 0;
                s.clientId = clientId;

                return message(s);
            }

            if (raw.name() == "error") {
                error s;
                fillf(s.info, raw, "info", true);
                
                string e;
                fillf(e, raw, "error");
                if (e == "bad-field") { 
                    s.reason = error::bad_field;
                    fillf(s.field_name, raw, "field");
                }
                if (e == "unknown-symbol") {
                    s.reason = error::unknown_symbol;
                    fillf(s.symbol, raw, "symbol");
                }
                if (e == "limit-reached") {
                    s.reason = error::limit;
                    fillf(s.symbol, raw, "symbol");
                }
                if (e == "exec-halted") {
                    s.reason = error::halt;
                    fillf(s.symbol, raw, "symbol");
                }
                if (e == "server-down") {
                    s.reason = error::server;
                }
                if (e == "bad-message") {
                    s.reason = error::bad_message;
                }

                s.clientId = clientId;

                return message(s);
            }

            pe.msg.reason = error::bad_message;
            pe.msg.info = "Unknown message type " + raw.name();
            throw pe;
        }

        template <typename T>
        struct any_fn : public std::unary_function<const T &, yaml::node> {
            yaml::node operator()(const T &t) const { 
                return yaml::node(boost::lexical_cast<string>(t), false); 
            }
        };
        template <typename iter>
        struct any_iterator {
            typedef boost::transform_iterator<any_fn<typename iter::value_type>, iter> type;
        };
        template <typename iter>
        typename any_iterator<iter>::type
        make_any_iter ( const iter &it ) {
                return typename any_iterator<iter>::type(it);
        }

        struct to_raw : public boost::static_visitor<yaml::message> {
            void fillf ( yaml::message &m, const string &f, const char * t ) const {
                fillf(m, f, string(t));
            }
            template <typename T>
            void fillf( yaml::message &m, const string &f, const list<T> &ts ) const {
                m.insert(std::make_pair(f, 
                           yaml::node(yaml::node_list(make_any_iter(ts.begin()), make_any_iter(ts.end())), false)));
            }
            template <typename T>
            void fillf( yaml::message &m, const string &f, const T &t ) const {
                m.insert(std::make_pair(f, yaml::node(boost::lexical_cast<string>(t), false)));
            }
            void fillf( yaml::message &m, const string &f, const double &d ) const {
                char buf[20];
                snprintf(buf, 19, "%.3f", d);
                buf[19] = 0;
                m.insert(std::make_pair(f, yaml::node(string(buf), false)));
            }

            yaml::message operator() ( const connect &s ) const {
                yaml::message m("connect");
                fillf(m, "account", s.account);
                fillf(m, "name", s.name);
                fillf(m, "password", s.password);
                return m;
            }
            yaml::message operator() ( const trade &s ) const {
                yaml::message m("trade");
                fillf(m, "symbol", s.symbol);
                fillf(m, "aggr", s.aggr);
                fillf(m, "orderID", s.orderID);
                fillf(m, "qty", s.qty);
                fillf(m, "short-mark", s.short_mark);
                return m;
            }
            yaml::message operator() ( const halt &s ) const {
                yaml::message m("halt");
                if (!s.all) {
                    fillf(m, "symbol", s.symbols);
                }
                return m;
            }
            yaml::message operator() ( const stop &s ) const {
                yaml::message m("stop");
                if (!s.all) {
                    fillf(m, "symbol", s.symbols);
                }
                return m;
            }
            yaml::message operator() ( const resume &s ) const {
                yaml::message m("resume");
                if (!s.all) {
                    fillf(m, "symbol", s.symbols);
                }
                return m;
            }
            yaml::message operator() ( const status &s ) const {
                yaml::message m("status");
                if (!s.all) {
                    fillf(m, "symbol", s.symbols);
                }
                return m;
            }
            yaml::message operator() ( const server &s ) const {
                yaml::message m("server");
                fillf(m, "name", s.name);
                fillf(m, "symbol", s.symbols);
                return m;
            }
            yaml::message operator() ( const fill &s ) const {
                char tmbuf[19];
                yaml::message m("fill");

                snprintf(tmbuf, 19, "%d.%06d", s.time_sec, s.time_usec);
                fillf(m, "symbol", s.symbol);
                fillf(m, "exchange", s.exchange);
                fillf(m, "time", string(tmbuf));
                fillf(m, "qtyLeft", s.qtyLeft);
                fillf(m, "orderID", s.orderID);
                fillf(m, "strat", s.strat);
                fillf(m, "fill-size", s.fill_size);
                fillf(m, "fill-price", s.fill_price);
                switch(s.liquidity) {
                    case fill::add:    fillf(m, "liquidity", "A"); break;
                    case fill::remove: fillf(m, "liquidity", "R"); break;
                    case fill::other:  fillf(m, "liquidity", "O"); break;
                }
                return m;
            }
            yaml::message operator() ( const info &s ) const {
                char tmbuf[19];
                yaml::message m("info");

                snprintf(tmbuf, 19, "%d.%06d", s.time_sec, s.time_usec);
                fillf(m, "symbol", s.symbol);
                fillf(m, "time", string(tmbuf));
                fillf(m, "position", s.position);
                fillf(m, "qtyLeft", s.qtyLeft);
                fillf(m, "locates", s.locates);
                fillf(m, "aggr", s.aggr);
                fillf(m, "halt", s.halt);
                fillf(m, "bid-size", s.bidsz);
                fillf(m, "ask-size", s.asksz);
                fillf(m, "bid", s.bid);
                fillf(m, "ask", s.ask);
                return m;
            }

            yaml::message operator() ( const error &s ) const {
                yaml::message m("error");
                switch (s.reason) {
                    case error::bad_field:
                        fillf(m, "error", "bad-field");
                        fillf(m, "field", s.field_name);
                        break;
                    case error::unknown_symbol:
                        fillf(m, "error", "unknown-symbol");
                        fillf(m, "symbol", s.symbol);
                        break;
                    case error::limit:
                        fillf(m, "error", "limit-reached");
                        fillf(m, "symbol", s.symbol);
                        break;
                    case error::halt:
                        fillf(m, "error", "exec-halted");
                        fillf(m, "symbol", s.symbol);
                        break;
                    case error::bad_message:
                        fillf(m, "error", "bad-message");
                        fillf(m, "message-name", s.message_name);
                        break;
                    case error::server:
                        fillf(m, "error", "server-down");
                        break;
                    case error::unshortable:
                        fillf(m, "error", "unshortable");
                        fillf(m, "symbol", s.symbol);
                }
                fillf(m, "reason", s.info);
                return m;
            }
        };

        struct to_string : public boost::static_visitor<std::string> {
            mutable std::ostringstream ss;
            to_string ( const std::string &pfx ) : ss(pfx) { }
            to_string ( )  { }
            to_string ( const to_string &to ) { } //XXX

            std::string operator() ( const connect &s ) const {
                ss << "Connect  " << s.account;
                if (s.name.empty()) ss << " anonymous client.";
                else ss << " \"" << s.name << "\"";
                if (s.password.empty()) ss << " no password";
                else ss << " password supplied.";
                if (s.listenToBcast > 0) ss << "Client listens to broadcast.";
                else ss << "Client does not listen to broadcast.";

                return ss.str();
            }
                
            std::string operator() ( const trade &s ) const {
              	ss << "Trade    " << "qty " << s.qty << " " << s.symbol << " at aggr " << s.aggr;
                if (s.short_mark != trade::unknown) {
                    ss << " marked ";
                    switch (s.short_mark) {
                        case trade::long_sell:  ss << "long "; break;
                        case trade::short_sell: ss << "short "; break;
                    }
                }
                ss << " (orderID = " << s.orderID << ").";
                return ss.str();
            }

            std::string operator() ( const stop &s ) const {
                ss << "Stop     ";
                if (s.all) ss << "(all symbols).";
                else {
                    ss << " [";
                    std::copy(s.symbols.begin(), s.symbols.end(), std::ostream_iterator<string>(ss, " "));
                    ss << "].";
                }
                return ss.str();
            }
                     
            std::string operator() ( const halt &s ) const {
                ss << "Halt     ";
                if (s.all) ss << "(all symbols).";
                else {
                    ss << " [";
                    std::copy(s.symbols.begin(), s.symbols.end(), std::ostream_iterator<string>(ss, " "));
                    ss << "].";
                }
                return ss.str();
            }
            std::string operator() ( const resume &s ) const {
                ss << "Resume   ";
                if (s.all) ss << "(all symbols).";
                else {
                    ss << " [";
                    std::copy(s.symbols.begin(), s.symbols.end(), std::ostream_iterator<string>(ss, " "));
                    ss << "].";
                }
                return ss.str();
            }
            std::string operator() ( const status &s ) const {
                ss << "Status   ";
                if (s.all) ss << "(all symbols).";
                else {
                    ss << " [";
                    std::copy(s.symbols.begin(), s.symbols.end(), std::ostream_iterator<string>(ss, " "));
                    ss << "].";
                }
                return ss.str();
            }
            std::string operator() ( const server &s ) const {
                ss << "Server   " << s.name << ".";
                return ss.str();
            }

            std::string operator() ( const fill &s ) const {
                ss << "Fill     " << s.symbol << " ";
                switch (s.liquidity) {
                    case fill::add:    ss << "A "; break;
                    case fill::remove: ss << "R "; break;
                    case fill::other:  ss << "O "; break;
                }
                ss << s.fill_size << " @" << s.fill_price
                   << " on " << s.exchange
                   << " qtyLeft: " << s.qtyLeft
                   << " (orderID = " << s.orderID << ")"
                   << " strat: " << s.strat;
                return ss.str();
            }
            std::string operator() ( const info &s ) const { 
                ss << "Info     " << s.symbol << " "
                   << s.position << " [l:" << s.locates << "] "
                   << " aggr: " << s.aggr << " qtyLeft: " << s.qtyLeft
                   << ", halt: " << s.halt
                   << " [" << s.bidsz << "x" << s.bid << " | " 
                   << s.ask << "x" << s.asksz << "]";
                return ss.str();
            }
            std::string operator() ( const error &s ) const {
                ss << "Error    ";
                switch (s.reason) {
                    case error::bad_field: ss << "bad field " << s.field_name; break;
                    case error::unknown_symbol: ss << "unknown symbol " << s.symbol; break;
                    case error::limit: ss << "limit " << s.symbol; break;
                    case error::halt: ss << "halt " << s.symbol; break;
                    case error::bad_message: ss << "bad message " << s.message_name; break;
                    case error::server: ss << "server"; break;
                    case error::unshortable: ss<< "unshortable "; break;
                };
                ss << s.info;
                return ss.str();
            }
        };
            
        struct to_req : public boost::static_visitor<request> {
            request operator() ( const connect &s ) const { return request(s); }
            request operator() ( const trade &s ) const { return request(s); }
            request operator() ( const stop &s ) const { return request(s); }
            request operator() ( const halt &s ) const { return request(s); }
            request operator() ( const resume &s ) const { return request(s); }
            request operator() ( const status &s ) const { return request(s); }
            request operator() ( const server &s ) const {
                parse_error pe;
                pe.msg.reason = error::bad_message;
                pe.msg.message_name = "server";
                pe.msg.info = std::string("message type 'server' not expected here.");
                throw pe;
            }
            request operator() ( const fill &s ) const {
                parse_error pe;
                pe.msg.reason = error::bad_message;
                pe.msg.message_name = "fill";
                pe.msg.info = std::string("message type 'fill' not expected here.");
                throw pe;
            }
            request operator() ( const info &s ) const {
                parse_error pe;
                pe.msg.reason = error::bad_message;
                pe.msg.message_name = "info";
                pe.msg.info = std::string("message type 'info' not expected here.");
                throw pe;
            }
            request operator() ( const error &s ) const {
                parse_error pe;
                pe.msg.reason = error::bad_message;
                pe.msg.message_name = "error";
                pe.msg.info = std::string("message type 'error' not expected here.");
                throw pe;
            }
        };
        struct to_rsp : public boost::static_visitor<response> {
            response operator() ( const server &s ) const { return response(s); }
            response operator() ( const fill &s ) const { return response(s); }
            response operator() ( const info &s ) const { return response(s); }
            response operator() ( const error &s ) const { return response(s); }
            response operator() ( const connect &s ) const {
                parse_error pe;
                pe.msg.reason = error::bad_message;
                pe.msg.message_name = "connect";
                pe.msg.info = std::string("message type 'connect' not expected here.");
                throw pe;
            }
            response operator() ( const trade &s ) const {
                parse_error pe;
                pe.msg.reason = error::bad_message;
                pe.msg.message_name = "trade";
                pe.msg.info = std::string("message type 'trade' not expected here.");
                throw pe;
            }
            response operator() ( const stop &s ) const {
                parse_error pe;
                pe.msg.reason = error::bad_message;
                pe.msg.message_name = "stop";
                pe.msg.info = std::string("message type 'stop' not expected here.");
                throw pe;
            }
            response operator() ( const halt &s ) const {
                parse_error pe;
                pe.msg.reason = error::bad_message;
                pe.msg.message_name = "halt";
                pe.msg.info = std::string("message type 'halt' not expected here.");
                throw pe;
            }
            response operator() ( const resume &s ) const {
                parse_error pe;
                pe.msg.reason = error::bad_message;
                pe.msg.message_name = "resume";
                pe.msg.info = std::string("message type 'resume' not expected here.");
                throw pe;
            }
            response operator() ( const status &s ) const {
                parse_error pe;
                pe.msg.reason = error::bad_message;
                pe.msg.message_name = "status";
                pe.msg.info = std::string("message type 'status' not expected here.");
                throw pe;
            }
        };

        struct relevant : public boost::static_visitor<std::pair<bool, int> > {
        protected:
        	int targetClientId;
        	bool targetListenToBcast;
        public:
        	relevant(int targetClientId, bool targetListenToBcast): targetClientId(targetClientId), targetListenToBcast(targetListenToBcast) {
        	}

        	std::pair<bool, int> operator() ( const server &s ) const { return std::make_pair(targetClientId == s.clientId, s.clientId); }
        	std::pair<bool, int> operator() ( const fill &s ) const { return std::make_pair(targetClientId == s.clientId || targetListenToBcast, s.clientId); }
        	std::pair<bool, int> operator() ( const info &s ) const { return std::make_pair(targetClientId == s.clientId, s.clientId); }
        	std::pair<bool, int> operator() ( const error &s ) const { return std::make_pair(targetClientId == s.clientId, s.clientId); }
        };

        request get_request  ( const message &m ) { return boost::apply_visitor(to_req(), m); }
        response get_response ( const message &m ) { return boost::apply_visitor(to_rsp(), m); }
        yaml::message put_message ( const message &m ) { return boost::apply_visitor(to_raw(), m); }
// GVNOTE: Removing the following put_message functions, because they are essentially the same as
// the above put_message(message) function, and message encompasses both request and response
//        yaml::message put_message ( const request &m ) { return boost::apply_visitor(to_raw(), m); }
//        yaml::message put_message ( const response &m ) { return boost::apply_visitor(to_raw(), m); }
        std::string show ( const message &m ) { return boost::apply_visitor(to_string(), m); }
        std::string show ( const request &m ) { return boost::apply_visitor(to_string(), m); }
        std::string show ( const response &m ) { return boost::apply_visitor(to_string(), m); }
        std::pair<bool, int> isRelevant(int targetClientId, bool targetListenToBcast, const response &m) { return boost::apply_visitor(relevant(targetClientId, targetListenToBcast), m); }
    }
}
