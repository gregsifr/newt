#include <cl-util/Configurable.h>

void ConfigurationSource::add ( Configurable &c ) {
    optdesc_c_.add(get_optdesc_(c));
    set_vmap_(c);
}

popt::options_description &ConfigurationSource::get_optdesc_ ( Configurable &c ) { return c.optdesc_; }
void ConfigurationSource::set_vmap_ ( Configurable &c ) { c.vmap_ = &vmap_c_; }

std::ostream &operator << ( std::ostream &os, const ConfigurationSource &cs ) {
    return os << cs.optdesc_c_;
}

bool CmdLineConfig::configure ( ) {
    optdesc_c_.add(optdesc_);
    set_vmap_(*this);
    //popt::store(popt::command_line_parser(argc_, argv_).options(optdesc_c_).allow_unregistered().run());
    popt::store(popt::parse_command_line(argc_, argv_, optdesc_c_), vmap_c_);
    popt::notify(vmap_c_);
    return true;
}

bool FileConfig::configure ( ) {
    std::ifstream ifs(file_.c_str(), std::ifstream::in);
    if (ifs.good()) {
        popt::store(popt::parse_config_file(ifs, optdesc_c_), vmap_c_);
        popt::notify(vmap_c_);
        ifs.close();
        return true;
    }
    return false;
}

bool CmdLineFileConfig::configure ( ) {
    CmdLineConfig::configure();

    for (std::vector<std::string>::const_iterator it = filenames_.begin(); 
            it != filenames_.end(); ++it) 
    {
        std::ifstream ifs(it->c_str(), std::ifstream::in);
        if (ifs.good()) {
            popt::store(popt::parse_config_file(ifs, optdesc_c_), vmap_c_);
            ifs.close();
        } else {
            std::cerr << "Couldn't read specified file " << *it << std::endl;
            return false;
        }
    }
    popt::notify(vmap_c_);
    return true;
}
