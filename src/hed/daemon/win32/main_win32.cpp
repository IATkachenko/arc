#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include <arc/win32.h>
#endif

#include <fstream>
#include <signal.h>
#include <arc/ArcConfig.h>
#include <arc/message/MCCLoader.h>
#include <arc/XMLNode.h>
#include <arc/Logger.h>
#include "../options.h"

Arc::Config config;
Arc::MCCLoader *loader;
Arc::Logger& logger=Arc::Logger::rootLogger;

static void shutdown(int)
{
    logger.msg(Arc::VERBOSE, "shutdown");
    delete loader;
    _exit(0);
}

static void merge_options_and_config(Arc::Config& cfg, Arc::ServerOptions& opt)
{   
    Arc::XMLNode srv = cfg["Server"];
    if (!(bool)srv) {
      logger.msg(Arc::ERROR, "No server config part of config file");
      return;
    }
    if (opt.pid_file != "") {
        if (!(bool)srv["PidFile"]) {
           srv.NewChild("PidFile")=opt.pid_file;
        } else {
            srv["PidFile"] = opt.pid_file;
        }
    }
    if (opt.foreground == true) {
        if (!(bool)srv["Foreground"]) {
            srv.NewChild("Foreground");
        }
    }
}

static std::string init_logger(Arc::Config& cfg)
{   
    /* setup root logger */
    Arc::XMLNode log = cfg["Server"]["Logger"];
    Arc::LogStream* sd = NULL;
    std::string log_file = (std::string)log;
    std::string str = (std::string)log.Attribute("level");
    if(!str.empty()) {
      Arc::LogLevel level = Arc::string_to_level(str);
      Arc::Logger::rootLogger.setThreshold(level); 
    }


    Arc::Logger::rootLogger.addDestination(*sd);
    if(!log_file.empty()) {
      std::fstream *dest = new std::fstream(log_file.c_str(), std::fstream::out | std::fstream::app);
      if(!(*dest)) {
        logger.msg(Arc::ERROR,"Failed to open log file: %s",log_file);
        _exit(1);
      }
      sd = new Arc::LogStream(*dest);
    }
    Arc::Logger::rootLogger.removeDestinations();
    if(sd) Arc::Logger::rootLogger.addDestination(*sd);
    if ((bool)cfg["Server"]["Foreground"]) {
      logger.msg(Arc::INFO, "Start foreground");
      Arc::LogStream *err = new Arc::LogStream(std::cerr);
      Arc::Logger::rootLogger.addDestination(*err);
    }
    return log_file;
}

int main(int argc, char **argv)
{
    signal(SIGTTOU, SIG_IGN);
    // Temporary stderr destination for error messages
    Arc::LogStream logcerr(std::cerr);
    Arc::Logger::getRootLogger().addDestination(logcerr);
    /* Create options parser */
    Arc::ServerOptions options;
      
    try {
        std::list<std::string> params = options.Parse(argc, argv);
        if (params.size() == 0) {
            /* Load and parse config file */
            config.parse(options.xml_config_file.c_str());
            if(!config) {
                logger.msg(Arc::ERROR, "Failed to load service configuration form file %s",options.xml_config_file);
                exit(1);
            };
            if(!MatchXMLName(config,"ArcConfig")) {
              logger.msg(Arc::ERROR, "Configuration root element is not <ArcConfig>");
              exit(1);
            }

            /* overwrite config variables by cmdline options */
            merge_options_and_config(config, options);
            std::string pid_file = (std::string)config["Server"]["PidFile"];
            /* initalize logger infrastucture */
            std::string root_log_file = init_logger(config);
            
            logger.msg(Arc::VERBOSE, "ARC_PLUGIN_PATH=%s", getenv("ARC_PLUGIN_PATH"));
            
            // set signal handlers 
            signal(SIGTERM, shutdown);
            signal(SIGINT, shutdown);

            // bootstrap
            loader = new Arc::MCCLoader(config);
            logger.msg(Arc::INFO, "Service side MCCs are loaded");
            // sleep forever
            for (;;) {
                sleep(INT_MAX/1000);
            }
        }
    } catch (const Glib::Error& error) {
      logger.msg(Arc::ERROR, error.what());
    }
    
    return 0;
}
