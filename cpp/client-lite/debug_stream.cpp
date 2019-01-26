
#include <cl-util/debug_stream.h>

using namespace clite::util;
using std::string;

debug_stream::debug_stream ( string const & name ) : tael::Logger(*(new tael::LoggerConfiguration((size_t) MAX_BINARY_BUFFER_FILE_SIZE))) {

    debug_stream_config::config *cfg = debug_stream_config::get_config();
    if (!(*cfg)) {
        prefix = name + string(": ");
    }
    else {
        string fullname = cfg->path() +string("/") + name + cfg->ext();
        int fd = open(fullname.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);

        if (fd == -1) {
            throw std::runtime_error(string("Cannot open file ") + fullname + string(" for logging"));
        }

        tael::FdLogger *dest = new tael::FdLogger(fd);
        this->addDestination(dest);
    }
}

void debug_stream::printf(const char *format, ... ) {
	// For variable number of arguments, need to use a va_list
	va_list ap;
	va_start(ap, format);
	std::string formatStr = std::string(format);
	if(prefix != "") {
		formatStr = prefix + formatStr;
	}
	TAEL_PRINTF(this, this->configuration().threshold(), formatStr.c_str(), ap);
	va_end(ap);
}

debug_stream::~debug_stream ( ) { }
