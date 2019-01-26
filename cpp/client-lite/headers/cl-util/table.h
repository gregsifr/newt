#ifndef __CLUTIL__TABLE_H__
#define __CLUTIL__TABLE_H__

#include <cl-util/Configurable.h>
#include <tdec/UnorderedMap.h>

#include <ext/hash_map>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

// GVNOTE: Defining this to be able to use the gnu hash_map for a string. This would go
// away if we switched to using StdStringUnorderedMap.h (see note below).
namespace trc { namespace tdec {
	template <> struct hash<std::string> {
		size_t operator() (const std::string& str) const {
			return __gnu_cxx::hash<const char*>()(str.c_str());
		}
	};
} //namespace tdec
} //namespace trc

namespace clite { namespace util {


    class file_table_config {
        protected:
            file_table_config ( ) { }
            file_table_config ( const file_table_config &o ) { }

        public:
            struct config : public Configurable {
                friend class file_table_config;
                private:
                config ( ) : Configurable("file-table") {
                    defOption("path", &paths_, "path(s) to search for file names.");
                }
                config ( config &o ) : paths_(o.paths_) { };
                std::vector<std::string> paths_;
                public:
                std::vector<std::string> & paths() { return paths_; }
                operator bool () const {
                    return !paths_.empty();
                }
            };

        static config *get_config ( ) { 
            static config cfg;
            return &cfg;
        }
    };

    class table_error : public std::runtime_error {
        public:
            table_error ( const std::string &s ) : std::runtime_error(s) {
                // GVNOTE: Should perhaps throw the exception? Also, see note below.
            }
    };

    class table_file_error : public table_error {
        public:
            table_file_error ( const std::string &s ) : table_error(s) {
                // GVNOTE: Should log the error. Figure out which debug_stream / logger to use.
                // Perhaps just print to stdout and use cronwrapper?
            }
    };
    class table_data_error : public table_error {
        public:
            table_data_error ( const std::string &s ) : table_error(s) {
                // GVNOTE: See note above
            }
    };

    // GVNOTE: This is unnecessarily generic. We should probably switch to using StdStringUnorderedMap in
    // tdec (infra repo), since we only use this hash function with the String key_type
    template <typename value, class parent_map = trc::tdec::UnorderedMap<typename value::key_type, value> >
    class file_table : public parent_map, public file_table_config {

        private:
            static void process_line(std::vector<std::string> &outvec, const std::string &in) {
                boost::algorithm::split(outvec, in, boost::algorithm::is_any_of("\t,"));
                for (std::vector<std::string>::iterator si = outvec.begin(); si != outvec.end(); ++si)
                    boost::algorithm::trim(*si);
            }

        public:
        file_table ( const std::string &name );

    };


    template<typename value, class parent_map>
    file_table<value, parent_map>::file_table ( const std::string &name ) {
        using namespace std;
        char errbuf[80];

        if (!(*get_config())) {
            throw table_file_error("No paths configured to read files.");
        }

        for (vector<string>::const_iterator pi = get_config()->paths().begin();
                pi != get_config()->paths().end(); ++pi)
        {
            string filename = *pi + string("/") + name;
            ifstream ifs(filename.c_str());
            // Note that although we might have multiple paths, as soon as we get
            // one place where the file exists, we will read that, and ignore any
            // of the subsequent locations in the path
            if (ifs) {
                string buf;
                vector<string> contents;
                vector<string> header;
                std::getline(ifs, buf);
                if (buf[0] == '#') {
                    buf.erase(0, 1);
                    process_line(header, buf);
                } else if (!buf.empty()) {
                    process_line(contents, buf);
                    value v(header, contents);
                    // Ignoring the return value (pair<iterator,bool>) below. Since this is
                    // the first entry that is read, it can't be a duplicate (unlike below)
                    parent_map::insert(std::make_pair(v.get_key(), v));
                }

                int ln = 0;
                while (ifs.good()) {
                    std::getline(ifs, buf);
                    ln += 1;
                    size_t comment = buf.find('#');
                    if (comment != string::npos) {
                        buf.erase(comment, buf.size());
                    }

                    if (!buf.empty()) {
                        process_line(contents, buf);
                        value v(header, contents);
                        std::pair<typename parent_map::iterator, bool> res = 
                            parent_map::insert(std::make_pair(v.get_key(), v));
                        if (!res.second) {
                            snprintf(errbuf, 79, "Duplicate key on line %d, file %s", ln, filename.c_str());
                            errbuf[79] = 0;
                            throw table_data_error(errbuf);
                        }
                    }
                }

                ifs.close();
                return;
            }
        }
        snprintf(errbuf, 79, "File name %s not found", name.c_str());
        errbuf[79] = 0;
        throw table_file_error(errbuf);
    }
}}

#endif // __CLUTIL__TABLE_H__
