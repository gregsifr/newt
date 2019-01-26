#ifndef __GT_MESSAGE_H__
#define __GT_MESSAGE_H__

#include <boost/variant.hpp>
#include "yaml_message.h"
#include <list>
#include <string>

namespace guillotine {
    namespace typed {

        struct connect {
            std::string account;
            std::string name;
            std::string password;
            int clientId;
            int listenToBcast;
        };

        struct trade {
            enum mark_type {
                long_sell, short_sell, unknown };
            //std::list<std::string> symbols;
            std::string symbol;
            double aggr;
            long orderID;
            int qty;
            int short_mark;
            int clientId;
        };

        // Whenever we receive a list of trades, ack them with the number of
        // trades received by us
        struct trade_ack {
        	int numTrades;
        	int clientId;
        };

        struct stop {
            std::list<std::string> symbols;
            bool all;
            int clientId;
        };

        struct halt {
            std::list<std::string> symbols;
            bool all;
            int clientId;
        };

        struct resume {
            std::list<std::string> symbols;
            bool all;
            int clientId;
        };

        struct status {
            std::list<std::string> symbols;
            bool all;
            int clientId;
        };

        struct server {
            std::string name;
            std::list<std::string> symbols;
            int clientId;
        };

        struct fill {
            enum liq_type {
                add, remove, other };
            liq_type liquidity;
            std::string symbol;
            std::string exchange;
            std::string strat;
            int time_sec;
            int time_usec;
            int qtyLeft;
            long orderID;
            int fill_size;
            double fill_price;
            int clientId;
        };

        struct info {
            std::string symbol;
            int time_sec;
            int time_usec;
            int position;
            int qtyLeft;
            int locates;
            double aggr;
            bool halt;
            double bid, ask;
            size_t bidsz, asksz;
            int clientId;
        };

        struct error {
            enum error_type {
                bad_field,
                unshortable,
                unknown_symbol,
                limit,
                halt,
                bad_message,
                server
            };
            error_type reason;
            std::string info;
            std::string symbol;
            std::string message_name;
            std::string field_name;
            int clientId;
        };

        class parse_error : public std::exception { 
            public:

            error msg;

            virtual const char *what ( ) { return msg.info.c_str(); }
            virtual ~parse_error ( ) throw ( ) { }
        };

        typedef boost::variant<connect, trade, stop, halt, resume, status> request;
        typedef boost::variant<server, fill, info, error> response;
        typedef boost::variant<connect, trade, stop, halt, resume, status,
                server, fill, info, error> message;

        //NSARKAS ammended so that a client id is embeded in the resulting message. Other option would be to stop treating messages as immutable later on.
        message get_message ( const yaml::message &raw, int clientId );
        yaml::message put_message ( const message &msg );
        /*
        yaml::message put_message ( const request &msg );
        yaml::message put_message ( const response &msg );
        */
        request get_request ( const message &m );
        response get_response ( const message &m );

        std::string show ( const message &m );
        std::pair<bool, int> isRelevant(int targetClientId, bool targetListenToBcast, const response &m);
        /*
        std::string show ( const request &m );
        std::string show ( const response &m );
        */

    }
}

#endif
