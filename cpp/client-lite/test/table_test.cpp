#include <cl-util/factory.h>
#include <cl-util/table.h>

using namespace clite::util;
using namespace std;

struct input {
    typedef const char * key_type;
    vector<string> ss;
    const char * get_key() const { return ss[0].c_str(); }
    input ( const vector<string> &h, const vector<string> &c ) : ss(c) {
        if (!h.empty() && h.size() == c.size()) {
            for (unsigned i = 0; i < h.size(); ++i) {
                ss[i] = h[i] + string("=") + c[i];
            }
        }
    }
};

int main (int argc, char **argv) {

    file_table_config::get_config()->paths().push_back(".");

    for (int i = 1; i < argc; ++i) {
        cout << "--- " << argv[i] << " ---" << endl;
        factory<file_table<input> >::pointer fp = factory<file_table<input> >::get((argv[i]));
        for (file_table<input>::iterator i = fp->begin(); i != fp->end(); ++i) {
            cout << i->first << " | ";
            for (vector<string>::const_iterator s = i->second.ss.begin(); s != i->second.ss.end(); ++s) {
                cout << "\"" << *s << "\" , ";
            }
            cout << endl;
        }
        cout << endl;
    }
}

