#ifndef __CL_UTIL_LOGGER__
#define __CL_UTIL_LOGGER__

#include <tael/Log.h>
#include <tael/FdLogger.h>
#include <cl-util/Configurable.h>

#define MAX_BINARY_BUFFER_FILE_SIZE (1 << 26)
using namespace trc;

namespace clite { namespace util {

    class debug_stream_config {
        protected:
            debug_stream_config () { }
            debug_stream_config ( const debug_stream_config & ) { }
        public:
            struct config : public Configurable {
                friend class debug_stream_config;
                private:
                    config ( ) : Configurable("debug-stream") {
                        defOption("path", &path_, "path to write debug streams", getenv("EXEC_LOG_DIR"));
                        defOption("ext", &ext_, "extension for debug files", ".log");
                    }
                    std::string path_, ext_;
                public:
                    const std::string &path() const { return path_; }
                    const std::string &ext() const { return ext_; }
                    operator bool() const {
                        return !path_.empty();
                    }
            };

            static config *get_config () {
                static config cfg;
                return &cfg;
            }
    };

    // GVNOTE: Make sure all the logger objects are being deleted properly (or are being
    // called using a boost smart pointer. Also, make sure that the logger object is deleted
    // _before_ the FdLogger object is deleted.
    
    class debug_stream : public tael::Logger {

        private:
            std::string name;
            std::string prefix;

        public:
            debug_stream ( std::string const & name );
            // GVNOTE: This is a hack. Adding the printf function to keep the code consistent
            // with the interface provided by the OLD logger. Should remove this in the future
            // and probably switch to calling TAEL_* directly everywhere in the code.
            void printf(const char *format, ...);
            virtual ~debug_stream ( ) ;
    };
}}
#endif
