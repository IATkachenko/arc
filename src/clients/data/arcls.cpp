// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <string>
#include <list>
#include <signal.h>

#include <arc/ArcLocation.h>
#include <arc/Logger.h>
#include <arc/StringConv.h>
#include <arc/URL.h>
#include <arc/XMLNode.h>
#include <arc/client/UserConfig.h>
#include <arc/data/DataHandle.h>
#include <arc/OptionParser.h>

static Arc::Logger logger(Arc::Logger::getRootLogger(), "arcls");

void arcls(const Arc::URL& dir_url,
           Arc::XMLNode credentials,
           bool show_details,
           bool show_urls,
           bool show_meta,
           int recursion,
           int timeout) {
  if (dir_url.Protocol() == "urllist") {
    std::list<Arc::URL> dirs = Arc::ReadURLList(dir_url);
    if (dirs.size() == 0) {
      logger.msg(Arc::ERROR, "Can't read list of locations from file %s",
                 dir_url.Path());
      return;
    }
    for (std::list<Arc::URL>::iterator dir = dirs.begin();
         dir != dirs.end(); dir++)
      arcls(*dir, credentials, show_details, show_urls, show_meta, recursion, timeout);
    return;
  }

  Arc::DataHandle url(dir_url);
  if (!url) {
    logger.msg(Arc::ERROR, "Unsupported url given");
    return;
  }
  url->AssignCredentials(credentials);
  std::list<Arc::FileInfo> files;
  url->SetSecure(false);
  if (!url->ListFiles(files, show_details, show_urls, show_meta)) {
    if (files.size() == 0) {
      logger.msg(Arc::ERROR, "Failed listing metafiles");
      return;
    }
    logger.msg(Arc::INFO, "Warning: "
               "Failed listing metafiles but some information is obtained");
  }
  for (std::list<Arc::FileInfo>::iterator i = files.begin();
       i != files.end(); i++) {
    if(!show_meta || show_details || show_urls) std::cout << i->GetName();
    if (show_details) {
      switch (i->GetType()) {
      case Arc::FileInfo::file_type_file:
        std::cout << " file";
        break;

      case Arc::FileInfo::file_type_dir:
        std::cout << " dir";
        break;

      default:
        std::cout << " unknown";
        break;
      }
      if (i->CheckSize())
        std::cout << " " << i->GetSize();
      else
        std::cout << " *";
      if (i->CheckCreated())
        std::cout << " " << i->GetCreated();
      else
        std::cout << " *";
      if (i->CheckValid())
        std::cout << " " << i->GetValid();
      else
        std::cout << " *";
      if (i->CheckCheckSum())
        std::cout << " " << i->GetCheckSum();
      else
        std::cout << " *";
      if (i->CheckLatency())
        std::cout << " " << i->GetLatency();
    }
    if(!show_meta || show_details || show_urls) std::cout << std::endl;
    if (show_urls)
      for (std::list<Arc::URL>::const_iterator u = i->GetURLs().begin();
           u != i->GetURLs().end(); u++)
        std::cout << "\t" << *u << std::endl;
    if (show_meta) {
      std::map<std::string, std::string> md = i->GetMetaData();
      for (std::map<std::string, std::string>::iterator mi = md.begin(); mi != md.end(); ++mi)
        std::cout<<mi->first<<":"<<mi->second<<std::endl;
    }
    // Do recursion
    else if (recursion > 0)
      if (i->GetType() == Arc::FileInfo::file_type_dir) {
        Arc::URL suburl = dir_url;
        if (suburl.Path()[suburl.Path().length() - 1] != '/')
          suburl.ChangePath(suburl.Path() + "/" + i->GetName());
        else
          suburl.ChangePath(suburl.Path() + i->GetName());
        std::cout << suburl.str() << ":" << std::endl;
        arcls(suburl, credentials, show_details, show_urls, show_meta, recursion - 1, timeout);
        std::cout << std::endl;
      }
  }
  return;
}

int main(int argc, char **argv) {

  setlocale(LC_ALL, "");
signal(SIGTTOU,SIG_IGN);

  Arc::LogStream logcerr(std::cerr);
  Arc::Logger::getRootLogger().addDestination(logcerr);
  Arc::Logger::getRootLogger().setThreshold(Arc::WARNING);

  Arc::ArcLocation::Init(argv[0]);

  Arc::OptionParser options(istring("url"),
                            istring("The arcls command is used for listing "
                                    "files in grid storage elements "
                                    "and file\nindex catalogues."));

  bool longlist = false;
  options.AddOption('l', "long", istring("long format (more information)"),
                    longlist);

  bool locations = false;
  options.AddOption('L', "locations", istring("show URLs of file locations"),
                    locations);

  bool metadata = false;
  options.AddOption('m', "metadata", istring("display all available metadata"),
        metadata);

  int recursion = 0;
  options.AddOption('r', "recursive",
                    istring("operate recursively up to specified level"),
                    istring("level"), recursion);

  int timeout = 20;
  options.AddOption('t', "timeout", istring("timeout in seconds (default 20)"),
                    istring("seconds"), timeout);

  std::string conffile;
  options.AddOption('z', "conffile",
                    istring("configuration file (default ~/.arc/client.xml)"),
                    istring("filename"), conffile);

  std::string debug;
  options.AddOption('d', "debug",
                    istring("FATAL, ERROR, WARNING, INFO, DEBUG or VERBOSE"),
                    istring("debuglevel"), debug);

  bool version = false;
  options.AddOption('v', "version", istring("print version information"),
                    version);

  std::list<std::string> params = options.Parse(argc, argv);

  Arc::UserConfig usercfg(conffile);
  if (!usercfg) {
    logger.msg(Arc::ERROR, "Failed configuration initialization");
    return 1;
  }

  if (!debug.empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(debug));
  else if (debug.empty() && usercfg.ConfTree()["Debug"]) {
    debug = (std::string)usercfg.ConfTree()["Debug"];
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(debug));
  }

  if (version) {
    std::cout << Arc::IString("%s version %s", "arcls", VERSION) << std::endl;
    return 0;
  }

  // Proxy check
  //if (!usercfg.CheckProxy())
  //  return 1;

  if (params.size() != 1) {
    logger.msg(Arc::ERROR, "Wrong number of parameters specified");
    return 1;
  }

  std::list<std::string>::iterator it = params.begin();

  Arc::NS ns;
  Arc::XMLNode cred(ns, "cred");
  usercfg.ApplyToConfig(cred);

  arcls(*it, cred, longlist, locations, metadata, recursion, timeout);

  return 0;
}
