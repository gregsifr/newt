#include <guillotine/yaml_message.h>
#include <guillotine/typed_message.h>

#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <Util/Socket.h>

#include <iostream>

using namespace guillotine;
using namespace std;

template<typename T>
boost::shared_ptr<T> shared ( T *t ) {
    return boost::shared_ptr<T>(t);
};

template<typename C, typename T>
struct insert_fn : public unary_function<T,void> { 
    C &c;
    insert_fn ( C &c ) : c(c) { }
    void operator()(const T &t) { c.push_back(t); }
};

int main ( int argc, char **argv ) {

    typedef boost::shared_ptr<yaml::base_reader> read_ptr;
    typedef boost::shared_ptr<yaml::base_writer> write_ptr;

    read_ptr reader;
    write_ptr writer;
    auto_ptr<TCPSocket> s;

    if (argc == 1) {
        reader = shared(new yaml::file_reader(cin));
        writer = shared(new yaml::file_writer(cout));
    } else if (argc == 3) {
        s = auto_ptr<TCPSocket>(new TCPSocket());
        if (!s->Connect(argv[1], boost::lexical_cast<int>(argv[2])))
            return 1;
        reader = shared(new yaml::socket_reader(s.get()));
        writer = shared(new yaml::socket_writer(s.get()));
    }

    yaml::parser yparse(reader);
    yaml::emitter yemit(writer);

    typedef list<yaml::message> ymsg_list;
    ymsg_list ins;
    ymsg_list outs;


    while (!reader->eof() && !writer->eof()) {
        insert_fn<ymsg_list, yaml::message> in_f(ins);
        if (yparse.receive(in_f) > 0) {
            for (ymsg_list::const_iterator i = ins.begin(); i != ins.end(); ++i) {
                try {
                    outs.push_back(typed::put_message(typed::get_message(*i)));
                } catch (typed::parse_error &pe) {
                    cerr << "Error: " << pe.what() << endl;
                    outs.push_back(typed::put_message(typed::message(pe.msg)));
                }
            }
            yemit.send(outs.begin(), outs.end());
            ins.clear();
            outs.clear();
        }
        reader->wait();
    }
};
