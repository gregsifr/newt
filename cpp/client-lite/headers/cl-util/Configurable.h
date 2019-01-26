#ifndef _CONFIGURABLE_H_
#define _CONFIGURABLE_H_
#include <boost/program_options.hpp>
#include <iostream>
#include <fstream>

namespace popt = boost::program_options;
class Configurable;

class ConfigurationSource {

    public:

    popt::options_description optdesc_c_;
    popt::variables_map vmap_c_;
    
    popt::options_description &get_optdesc_ ( Configurable &c );
    void set_vmap_ ( Configurable &c );
        
    public:
    friend std::ostream &operator << ( std::ostream &os, const ConfigurationSource &cs );
        
    virtual void add ( Configurable &c );

    virtual bool configure ( ) = 0;
    virtual ~ConfigurationSource ( ) { }
};

/** A simple configuration system for objects.
  *
  * Inherit from this class to make objects of your class easily configurable.
  * Call this constructor with a name that identifies this object, then in your
  * constructor, or somewhere else early on, call defOption with a name, address
  * of configureable variable, and description to create the configuration
  * option.

  * Example:
    class FooBar : public Configurable {
        int foo;
        string bar;
        FooBar (const string &name) : Configurable(name) {
            defOption("foo", &foo, "set foo for # of foos 1-10");
            defOption("bar", &bar, "set name of drinking establishment");
        }
        bool doThings ( ) {
            if (!configured) return false;   // we don't know what we are!
            else { 
                fooAtTheBar(foo, bar);
                return true;
            }
        }
    };

  * In this case, the options will be known outside the program as "<name>.foo"
  * and "<name>.bar".  See ConfigureSource documentation for info on how these
  * can be set.
  */

// GVNOTE: We should probably keep this class even if we get rid of client-lite. This
// class makes it very easy and convenient to define config options.
class Configurable {
    friend class ConfigurationSource;
    
    std::string confpfx_;

    protected:
    popt::options_description optdesc_;
    popt::variables_map *vmap_;

        void defSwitch ( const std::string &name, bool *param, const std::string &desc) {
            defOption(name, param, desc);
        }
    
        void defOption ( const std::string &name, bool *param, const std::string &desc ) {
            optdesc_.add_options()((confpfx_ + name).c_str(), popt::bool_switch(param), desc.c_str());
        }

        template <class T>
        void defOption ( const std::string &name, T *param, const std::string &desc ) {
            optdesc_.add_options()((confpfx_ + name).c_str(), popt::value<T>(param), desc.c_str());
        }

        template <class T>
        void defOption ( const std::string &name, std::vector<T> *param, const std::string &desc, const T &def) {
            std::ostringstream ss;
            ss << def;
            optdesc_.add_options()((confpfx_ + name).c_str(), 
                    popt::value<std::vector<T> >(param)->default_value(std::vector<T>(1, def), ss.str()), 
                    desc.c_str());
        }

        template <class T>
        void defOption ( const std::string &name, T *param, const std::string &desc, const T &def) {
            optdesc_.add_options()((confpfx_ + name).c_str(), 
                    popt::value<T>(param)->default_value(def), 
                    desc.c_str());
        }

        void defOption ( const std::string &name, std::string *param, const std::string &desc, const char *def) {
            defOption(name, param, desc, std::string(def));
        }
        

        bool configured (  ) { return vmap_ != 0; }
        bool configured ( const std::string &param ) 
        { return vmap_ != 0 && vmap_->count(confpfx_ + param) != 0; }
    public:

        Configurable ( const std::string &prefix ) 
            : confpfx_ (prefix == ""? "" : prefix + "."), optdesc_(prefix),
              vmap_(0) {  }
        Configurable ( ) : vmap_(0) { }
        virtual ~Configurable ( ) { }
};

class CmdLineConfig : public ConfigurationSource, public Configurable {

    int argc_;
    char **argv_;

    public:
    CmdLineConfig ( int argc, char **argv ) : 
        Configurable(), argc_(argc), argv_(argv) { }

    bool configure ( );
    Configurable::defSwitch;
    Configurable::defOption;
    Configurable::configured;
};

class FileConfig : public ConfigurationSource, public Configurable {

    std::string file_;
    
    public:
    FileConfig ( std::string filename ) : file_(filename) { }

    bool configure ( );
};

class CmdLineFileConfig : public CmdLineConfig {

    std::vector<std::string> filenames_;
    public:
    CmdLineFileConfig ( int argc, char **argv, std::string fileopt ) 
        : CmdLineConfig(argc, argv)
    {
        defOption(fileopt, &filenames_, "Read configuration options from file(s).");
    }
    bool configure ( );
};

#endif
