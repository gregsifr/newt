#ifndef __GT_MSG_YAML__
#define __GT_MSG_YAML__

#include <string>
#include <list>

#include <yaml.h>

#include <boost/program_options/variables_map.hpp>

#include <iostream>

class Socket;

namespace guillotine {
    namespace yaml {

    typedef boost::program_options::variable_value node;

    class message : public boost::program_options::variables_map {
        public:

        typedef boost::program_options::variables_map map;

        private:
            std::string name_;

        public:
        message ( ) { }
        message ( const std::string &n ) : name_(n) { }
        message ( const message &m ) : map(m), name_(m.name_) { }
        const std::string &name() const { return name_; }
        std::string &name() { return name_; }
    };

    typedef std::list<node> node_list;

    class base_reader {
        size_t lastmark;

        protected:
        std::string buf;
        public:

        bool doc_ready ( );
        bool get_docs ( const char *&c, int &n );
        void clear_docs ( );

        virtual bool read_bytes ( ) = 0;
        virtual bool ready ( ) = 0;
        virtual bool eof ( ) = 0;
        virtual bool error ( ) = 0;
        virtual bool wait ( ) { return true; }
        
        base_reader ( ) : lastmark(0) { }
        virtual ~base_reader ( ) { }
    };

    class socket_reader : public base_reader {
        Socket *s;
        char cbuf[1024];
        bool is_eof;

        public:
        virtual bool read_bytes ( );
        virtual bool ready ( );
        virtual bool wait ( );
        virtual bool eof ( );
        virtual bool error ( );
        socket_reader ( Socket *s ) : s(s), is_eof(false) { }
        ~socket_reader ( ) { }
    };

    class file_reader : public base_reader {
        std::istream &in;
        char cbuf[1024];

        public:
        virtual bool read_bytes ( );
        virtual bool ready ( );
        virtual bool eof ( );
        virtual bool error ( );
        file_reader ( std::istream &in ) : in(in) { };
        ~file_reader ( ) { }
    };

    class base_writer {

        protected:
        std::string buf;

        public:

        bool take_docs ( const char *c, int n);
        void clear_docs ( );

        virtual bool write_bytes ( ) = 0;
        virtual bool ready ( ) = 0;
        virtual bool eof ( ) = 0;
        virtual bool error ( ) = 0;
        virtual bool wait ( ) { return true; }

        base_writer ( ) { }
        virtual ~base_writer ( ) { }
    };

    class file_writer : public base_writer {

        std::ostream &out;

        public:
        virtual bool write_bytes ( );
        virtual bool ready ( );
        virtual bool eof ( );
        virtual bool error ( );

        file_writer ( std::ostream &o ) : out(o) { }
        ~file_writer ( ) { }
    };

    class socket_writer : public base_writer {
        
        Socket *s;

        public:
        virtual bool write_bytes ( );
        virtual bool ready ( );
        virtual bool eof ( );
        virtual bool error ( );

        socket_writer ( Socket *s ) : s(s) { }
        ~socket_writer ( ) { }

    };

    struct libyaml { 
        static int ystrcmp ( const yaml_char_t *a, const yaml_char_t *b );
        static yaml_char_t *maptag;
        static yaml_char_t *seqtag;
        static yaml_char_t *strtag;
        static yaml_char_t *inttag;
        static yaml_char_t *flttag;
        static yaml_char_t *booltag;
    };


    class parser {

        boost::shared_ptr<base_reader> rdr;

        std::list<message> ms;

        void init ( );
        bool get_messages ( );
        void yaml_to_msg ( message &m, yaml_document_t *ydoc, 
                yaml_node_pair_t *start, yaml_node_pair_t *end );

        public:

        parser ( boost::shared_ptr<base_reader> r ) : rdr(r) { init(); }
        parser ( base_reader *r ) : rdr(r) { init(); }

        template <typename F>
        int receive ( F &f ) {
            int i = 0;
            if (rdr->read_bytes() && rdr->doc_ready()) {
                get_messages();
                i = ms.size();
                std::for_each(ms.begin(), ms.end(), f);
                ms.clear();
            }
            return i;
        }
    };

    class emitter {

        static int write_handler ( void *thisptr, unsigned char *buf, size_t size );
        boost::shared_ptr<base_writer> wtr;

        void msg_to_yaml ( yaml_document_t *ydoc, int seqix, message &m );
        bool write_messages ( );

        std::list<message> ms;


        public:

        emitter ( boost::shared_ptr<base_writer> r ) : wtr(r) {  }
        emitter ( base_writer *r ) : wtr(r) {  }

        template <typename It>
        int send ( It begin, It end ) {
            typedef std::insert_iterator<std::list<message> > insit;
            std::copy(begin, end, insit(ms, ms.end()));
            int n = ms.size();
            write_messages();
            ms.clear();
            if (wtr->write_bytes()) {
                return n;
            } else return 0;
        }
        int send ( const message &m ) {
            ms.push_back(m);
            write_messages();
            ms.clear();
            if (wtr->write_bytes()) {
                return 1;
            } else return 0;
        }
    };

}
}

#endif
