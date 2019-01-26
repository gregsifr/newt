
#include <cl-util/Configurable.h>
#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

using namespace clite::util;
using namespace std;


struct input {
    typedef int key_type;
    vector<string> ss;
    key_type get_key() const { return atoi(ss[0].c_str()); }
    input ( const vector<string> &h, const vector<string> &c ) : ss(c) {
        if (!h.empty() && h.size() == c.size()) {
            for (unsigned i = 1; i < h.size(); ++i) {
                ss[i] = h[i] + string("=") + c[i];
            }
        }
    }
};

int main (int argc, char **argv) {
    CmdLineConfig cfg(argc, argv);
    cfg.add(*file_table_config::get_config());
    cfg.add(*debug_stream_config::get_config());

    cout << cfg << endl;

    cfg.configure();

    file_table<input> ft("table1.tsv");
    factory<debug_stream>::pointer dp = factory<debug_stream>::get(string("output"));

    for (file_table<input>::iterator i = ft.begin(); i != ft.end(); ++i) {
    	TAEL_PRINTF(dp, TAEL_ERROR, "%d -> ", i->second.get_key());
        for (vector<string>::const_iterator s = i->second.ss.begin(); s != i->second.ss.end(); ++s) {
        	TAEL_PRINTF(dp, TAEL_ERROR, "\"%s\", ", s->c_str());
        }
        TAEL_PRINTF(dp, TAEL_ERROR, "\n");
    }

};
