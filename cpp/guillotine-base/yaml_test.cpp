#include "yaml_message.h"
#include <list>

#include <string>
#include <sstream>
#include <fstream>

#include <Util/Socket.h>
using namespace std;
using namespace guillotine;

int main ( int argc, char **argv ) {

    /*
       ostringstream ss;
       ss << "---" << endl;
       ss << "hello: world" << endl;
       ss << "foo: bar" << endl;
       ss << "..." << endl;
       */

    TCPSocket *s = new TCPSocket();
    if (!s->Connect("localhost", 21093)) {
        return 2;
    }

    typedef boost::shared_ptr<yaml::base_reader> reader_sp;
    typedef boost::shared_ptr<yaml::base_writer> writer_sp;
    reader_sp rdr = reader_sp(new yaml::socket_reader(s));
    writer_sp wtr = writer_sp(new yaml::file_writer(cout));

    list<yaml::raw_message> ms;
    yaml::parser yp(rdr);
    yaml::emitter ye(wtr);

    while (rdr->wait()) {
        if (yp.receive(insert_iterator<list<yaml::raw_message> >(ms, ms.end())) > 0) {
            for (list<yaml::raw_message>::iterator it = ms.begin(); it != ms.end(); ++it) {
                cout << "vvv " << it->name() << endl;
                for (yaml::raw_message::iterator mit = it->begin(); mit != it->end(); ++mit) {
                    if (boost::any_cast<string>(&(mit->second.value())))
                        cout << mit->first << ": " << mit->second.as<string>() << endl;
                    if (boost::any_cast<yaml::raw_list>(&(mit->second.value()))) {
                        yaml::raw_list &l = mit->second.as<yaml::raw_list>();
                        cout << mit->first << ": ";
                        for (yaml::raw_list::const_iterator i = l.begin(); i != l.end(); ++i) {
                            cout << i->as<string>() << ", ";
                        }
                        cout << endl;
                    }
                }
                cout << "^^^ " << endl;
            }
            ye.send(ms.begin(), ms.end());
        }
        ms.clear();
    }

    delete s;
}

