// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <list>

#include <unistd.h>
#include <sys/types.h>

#include <arc/ArcLocation.h>
#include <arc/GUID.h>
#include <arc/Logger.h>
#include <arc/StringConv.h>
#include <arc/URL.h>
#include <arc/User.h>
#include <arc/UserConfig.h>
#include <arc/credential/Credential.h>
#include <arc/data/FileCache.h>
#include <arc/data/DataHandle.h>
#include <arc/data/DataMover.h>
#include <arc/data/URLMap.h>
#include <arc/loader/FinderLoader.h>
#include <arc/OptionParser.h>

static Arc::Logger logger(Arc::Logger::getRootLogger(), "arccp");

static void progress(FILE *o, const char*, unsigned int,
                     unsigned long long int all, unsigned long long int max,
                     double, double) {
  static int rs = 0;
  const char rs_[4] = {
    '|', '/', '-', '\\'
  };
  if (max) {
    fprintf(o, "\r|");
    unsigned int l = (74 * all + 37) / max;
    if (l > 74)
      l = 74;
    unsigned int i = 0;
    for (; i < l; i++)
      fprintf(o, "=");
    fprintf(o, "%c", rs_[rs++]);
    if (rs > 3)
      rs = 0;
    for (; i < 74; i++)
      fprintf(o, " ");
    fprintf(o, "|\r");
    fflush(o);
    return;
  }
  fprintf(o, "\r%llu kB                    \r", all / 1024);
}

bool arcregister(const Arc::URL& source_url,
                 const Arc::URL& destination_url,
                 const std::list<std::string>& locations,
                 Arc::UserConfig& usercfg,
                 bool secure,
                 bool passive,
                 bool force_meta,
                 int timeout) {
  if (!source_url) {
    logger.msg(Arc::ERROR, "Invalid URL: %s", source_url.str());
    return false;
  }
  if (!destination_url) {
    logger.msg(Arc::ERROR, "Invalid URL: %s", destination_url.str());
    return false;
  }
  if (source_url.Protocol() == "urllist" &&
      destination_url.Protocol() == "urllist") {
    std::list<Arc::URL> sources = Arc::ReadURLList(source_url);
    std::list<Arc::URL> destinations = Arc::ReadURLList(destination_url);
    if (sources.empty()) {
      logger.msg(Arc::ERROR, "Can't read list of sources from file %s",
                 source_url.Path());
      return false;
    }
    if (destinations.empty()) {
      logger.msg(Arc::ERROR, "Can't read list of destinations from file %s",
                 destination_url.Path());
      return false;
    }
    if (sources.size() != destinations.size()) {
      logger.msg(Arc::ERROR,
                 "Numbers of sources and destinations do not match");
      return false;
    }
    bool r = true;
    for (std::list<Arc::URL>::iterator source = sources.begin(),
                                       destination = destinations.begin();
         (source != sources.end()) && (destination != destinations.end());
         source++, destination++)
      if(!arcregister(*source, *destination, locations, usercfg, secure, passive,
                      force_meta, timeout)) r = false;
    return r;
  }
  if (source_url.Protocol() == "urllist") {
    std::list<Arc::URL> sources = Arc::ReadURLList(source_url);
    if (sources.empty()) {
      logger.msg(Arc::ERROR, "Can't read list of sources from file %s",
                 source_url.Path());
      return false;
    }
    bool r = true;
    for (std::list<Arc::URL>::iterator source = sources.begin();
         source != sources.end(); source++)
      if(!arcregister(*source, destination_url, locations, usercfg, secure, passive,
                      force_meta, timeout)) r = false;
    return r;
  }
  if (destination_url.Protocol() == "urllist") {
    std::list<Arc::URL> destinations = Arc::ReadURLList(destination_url);
    if (destinations.empty()) {
      logger.msg(Arc::ERROR, "Can't read list of destinations from file %s",
                 destination_url.Path());
      return false;
    }
    bool r = true;
    for (std::list<Arc::URL>::iterator destination = destinations.begin();
         destination != destinations.end(); destination++)
      if(!arcregister(source_url, *destination, locations, usercfg, secure, passive,
                      force_meta, timeout)) r = false;
    return r;
  }

  if (destination_url.Path()[destination_url.Path().length() - 1] == '/') {
    logger.msg(Arc::ERROR, "Fileset registration is not supported yet");
    return false;
  }
  if (source_url.IsSecureProtocol() || destination_url.IsSecureProtocol()) {
    usercfg.InitializeCredentials(Arc::initializeCredentialsType::RequireCredentials);
    if (!Arc::Credential::IsCredentialsValid(usercfg)) {
      logger.msg(Arc::ERROR, "Unable to register file %s: No valid credentials found", source_url.str());
      return false;
    }
  }
  Arc::DataHandle source(source_url, usercfg);
  Arc::DataHandle destination(destination_url, usercfg);
  if (!source) {
    logger.msg(Arc::ERROR, "Unsupported source url: %s", source_url.str());
    return false;
  }
  if (!destination) {
    logger.msg(Arc::ERROR, "Unsupported destination url: %s",
               destination_url.str());
    return false;
  }
  if (source->IsIndex() || !destination->IsIndex()) {
    logger.msg(Arc::ERROR, "For registration source must be ordinary URL"
               " and destination must be indexing service");
    return false;
  }
  if (!locations.empty()) {
    std::string meta(destination->GetURL().Protocol()+"://"+destination->GetURL().Host());
    for (std::list<std::string>::const_iterator i = locations.begin(); i != locations.end(); ++i)
      destination->AddLocation(*i, meta);
  }
  // Obtain meta-information about source
  Arc::FileInfo fileinfo;
  Arc::DataPoint::DataPointInfoType verb = (Arc::DataPoint::DataPointInfoType)Arc::DataPoint::INFO_TYPE_CONTENT;
  if (!source->Stat(fileinfo, verb)) {
    logger.msg(Arc::ERROR, "Source probably does not exist");
    return false;
  }
  // add new location
  if (!destination->Resolve(false)) {
    logger.msg(Arc::ERROR, "Problems resolving destination");
    return false;
  }
  bool replication = destination->Registered();
  destination->SetMeta(*source); // pass metadata
  std::string metaname;
  // look for similar destination
  for (destination->SetTries(1); destination->LocationValid();
       destination->NextLocation()) {
    const Arc::URL& loc_url = destination->CurrentLocation();
    if (loc_url == source_url) {
      metaname = destination->CurrentLocationMetadata();
      break;
    }
  }
  // remove locations if exist
  for (destination->SetTries(1); destination->RemoveLocation();) {}
  // add new location
  if (metaname.empty())
    metaname = source_url.ConnectionURL();
  if (!destination->AddLocation(source_url, metaname)) {
    destination->PreUnregister(replication);
    logger.msg(Arc::ERROR, "Failed to accept new file/destination");
    return false;
  }
  destination->SetTries(1);
  if (!destination->PreRegister(replication, force_meta)) {
    logger.msg(Arc::ERROR, "Failed to register new file/destination");
    return false;
  }
  if (!destination->PostRegister(replication)) {
    destination->PreUnregister(replication);
    logger.msg(Arc::ERROR, "Failed to register new file/destination");
    return false;
  }
  return true;
}

bool arccp(const Arc::URL& source_url_,
           const Arc::URL& destination_url_,
           const std::list<std::string>& locations,
           const std::string& cache_dir,
           Arc::UserConfig& usercfg,
           bool secure,
           bool passive,
           bool force_meta,
           int recursion,
           int tries,
           bool verbose,
           int timeout) {
  Arc::URL source_url(source_url_);
  if (!source_url) {
    logger.msg(Arc::ERROR, "Invalid URL: %s", source_url.str());
    return false;
  }
  Arc::URL destination_url(destination_url_);
  if (!destination_url) {
    logger.msg(Arc::ERROR, "Invalid URL: %s", destination_url.str());
    return false;
  }

  if (timeout <= 0)
    timeout = 300; // 5 minute default
  if (tries < 0)
    tries = 0;
  if (source_url.Protocol() == "urllist" &&
      destination_url.Protocol() == "urllist") {
    std::list<Arc::URL> sources = Arc::ReadURLList(source_url);
    std::list<Arc::URL> destinations = Arc::ReadURLList(destination_url);
    if (sources.empty()) {
      logger.msg(Arc::ERROR, "Can't read list of sources from file %s",
                 source_url.Path());
      return false;
    }
    if (destinations.empty()) {
      logger.msg(Arc::ERROR, "Can't read list of destinations from file %s",
                 destination_url.Path());
      return false;
    }
    if (sources.size() != destinations.size()) {
      logger.msg(Arc::ERROR,
                 "Numbers of sources and destinations do not match");
      return false;
    }
    bool r = true;
    for (std::list<Arc::URL>::iterator source = sources.begin(),
                                       destination = destinations.begin();
         (source != sources.end()) && (destination != destinations.end());
         source++, destination++)
      if(!arccp(*source, *destination, locations, cache_dir, usercfg, secure, passive,
                force_meta, recursion, tries, verbose, timeout)) r = false;
    return r;
  }
  if (source_url.Protocol() == "urllist") {
    std::list<Arc::URL> sources = Arc::ReadURLList(source_url);
    if (sources.empty()) {
      logger.msg(Arc::ERROR, "Can't read list of sources from file %s",
                 source_url.Path());
      return false;
    }
    bool r = true;
    for (std::list<Arc::URL>::iterator source = sources.begin();
         source != sources.end(); source++)
      if(!arccp(*source, destination_url, locations, cache_dir, usercfg, secure,
                passive, force_meta, recursion, tries, verbose, timeout))
        r = false;
    return r;
  }
  if (destination_url.Protocol() == "urllist") {
    std::list<Arc::URL> destinations = Arc::ReadURLList(destination_url);
    if (destinations.empty()) {
      logger.msg(Arc::ERROR, "Can't read list of destinations from file %s",
                 destination_url.Path());
      return false;
    }
    bool r = true;
    for (std::list<Arc::URL>::iterator destination = destinations.begin();
         destination != destinations.end(); destination++)
      if(!arccp(source_url, *destination, locations, cache_dir, usercfg, secure,
                passive, force_meta, recursion, tries, verbose, timeout))
        r = false;
    return r;
  }

  if (destination_url.Path()[destination_url.Path().length() - 1] != '/') {
    if (source_url.Path()[source_url.Path().length() - 1] == '/') {
      logger.msg(Arc::ERROR,
                 "Fileset copy to single object is not supported yet");
      return false;
    }
  }
  else {
    // Copy TO fileset/directory
    if (source_url.Path()[source_url.Path().length() - 1] != '/') {
      // Copy FROM single object
      std::string::size_type p = source_url.Path().rfind('/');
      if (p == std::string::npos) {
        logger.msg(Arc::ERROR, "Can't extract object's name from source url");
        return false;
      }
      destination_url.ChangePath(destination_url.Path() +
                                 source_url.Path().substr(p + 1));
    }
    else {
      // Fileset copy
      // Find out if source can be listed (TODO - through datapoint)
      if ((source_url.Protocol() != "rc") &&
          (source_url.Protocol() != "rls") &&
          (source_url.Protocol() != "fireman") &&
          (source_url.Protocol() != "file") &&
          (source_url.Protocol() != "se") &&
          (source_url.Protocol() != "srm") &&
          (source_url.Protocol() != "lfc") &&
          (source_url.Protocol() != "gsiftp") &&
          (source_url.Protocol() != "ftp")) {
        logger.msg(Arc::ERROR,
                   "Fileset copy for this kind of source is not supported");
        return false;
      }
      if (source_url.IsSecureProtocol() || destination_url.IsSecureProtocol()) {
        usercfg.InitializeCredentials(Arc::initializeCredentialsType::RequireCredentials);
        if (!Arc::Credential::IsCredentialsValid(usercfg)) {
          logger.msg(Arc::ERROR, "Unable to copy file %s: No valid credentials found", source_url.str());
          return false;
        }
      }
      Arc::DataHandle source(source_url, usercfg);
      if (!source) {
        logger.msg(Arc::ERROR, "Unsupported source url: %s", source_url.str());
        return false;
      }
      std::list<Arc::FileInfo> files;
      if (source->IsIndex()) {
        if (!source->List(files, (Arc::DataPoint::DataPointInfoType)
                  (Arc::DataPoint::INFO_TYPE_NAME | Arc::DataPoint::INFO_TYPE_TYPE))) {
          logger.msg(Arc::ERROR, "Failed listing metafiles");
          return false;
        }
      }
      else
        if (!source->List(files, (Arc::DataPoint::DataPointInfoType)
                  (Arc::DataPoint::INFO_TYPE_NAME | Arc::DataPoint::INFO_TYPE_TYPE))) {
          logger.msg(Arc::ERROR, "Failed listing files");
          return false;
        }
      bool failures = false;
      // Handle transfer of files first (treat unknown like files)
      for (std::list<Arc::FileInfo>::iterator i = files.begin();
           i != files.end(); i++) {
        if ((i->GetType() != Arc::FileInfo::file_type_unknown) &&
            (i->GetType() != Arc::FileInfo::file_type_file))
          continue;
        logger.msg(Arc::INFO, "Name: %s", i->GetName());
        Arc::URL s_url(std::string(source_url.str() + i->GetName()));
        Arc::URL d_url(std::string(destination_url.str() + i->GetName()));
        logger.msg(Arc::INFO, "Source: %s", s_url.str());
        logger.msg(Arc::INFO, "Destination: %s", d_url.str());
        Arc::DataHandle source(s_url, usercfg);
        Arc::DataHandle destination(d_url, usercfg);
        if (!source) {
          logger.msg(Arc::INFO, "Unsupported source url: %s", s_url.str());
          continue;
        }
        if (!destination) {
          logger.msg(Arc::INFO, "Unsupported destination url: %s", d_url.str());
          continue;
        }
        if (!locations.empty() && destination->IsIndex()) {
          std::string meta(destination->GetURL().Protocol()+"://"+destination->GetURL().Host());
          for (std::list<std::string>::const_iterator i = locations.begin(); i != locations.end(); ++i)
            destination->AddLocation(*i, meta);
        }
        Arc::DataMover mover;
        mover.secure(secure);
        mover.passive(passive);
        mover.verbose(verbose);
        mover.force_to_meta(force_meta);
        if (tries) {
          mover.retry(true); // go through all locations
          source->SetTries(tries); // try all locations "tries" times
          destination->SetTries(tries);
        }
        Arc::User cache_user;
        Arc::FileCache cache;
        if (!cache_dir.empty()) cache = Arc::FileCache(cache_dir+" .", "", cache_user.get_uid(), cache_user.get_gid());
        Arc::DataStatus res = mover.Transfer(*source, *destination, cache, Arc::URLMap(),
                                             0, 0, 0, timeout);
        if (!res.Passed()) {
          if (!res.GetDesc().empty())
            logger.msg(Arc::INFO, "Current transfer FAILED: %s - %s", std::string(res), res.GetDesc());
          else
            logger.msg(Arc::INFO, "Current transfer FAILED: %s", std::string(res));
          if (res.Retryable())
            logger.msg(Arc::ERROR, "This seems like a temporary error, please try again later");
          failures = true;
        }
        else
          logger.msg(Arc::INFO, "Current transfer complete");
      }
      if (failures) {
        logger.msg(Arc::ERROR, "Some transfers failed");
        return false;
      }
      // Go deeper if allowed
      bool r = true;
      if (recursion > 0)
        // Handle directories recursively
        for (std::list<Arc::FileInfo>::iterator i = files.begin();
             i != files.end(); i++) {
          if (i->GetType() != Arc::FileInfo::file_type_dir)
            continue;
          if (verbose)
            logger.msg(Arc::INFO, "Directory: %s", i->GetName());
          std::string s_url(source_url.str());
          std::string d_url(destination_url.str());
          s_url += i->GetName();
          d_url += i->GetName();
          s_url += "/";
          d_url += "/";
          if(!arccp(s_url, d_url, locations, cache_dir, usercfg, secure, passive,
                    force_meta, recursion - 1, tries, verbose, timeout))
            r = false;
        }
      return r;
    }
  }
  if (source_url.IsSecureProtocol() || destination_url.IsSecureProtocol()) {
    usercfg.InitializeCredentials(Arc::initializeCredentialsType::RequireCredentials);
    if (!Arc::Credential::IsCredentialsValid(usercfg)) {
      logger.msg(Arc::ERROR, "Unable to copy file %s: No valid credentials found", source_url.str());
      return false;
    }
  }
  Arc::DataHandle source(source_url, usercfg);
  Arc::DataHandle destination(destination_url, usercfg);
  if (!source) {
    logger.msg(Arc::ERROR, "Unsupported source url: %s", source_url.str());
    return false;
  }
  if (!destination) {
    logger.msg(Arc::ERROR, "Unsupported destination url: %s",
               destination_url.str());
    return false;
  }
  if (!locations.empty() && destination->IsIndex()) {
    std::string meta(destination->GetURL().Protocol()+"://"+destination->GetURL().Host());
    for (std::list<std::string>::const_iterator i = locations.begin(); i != locations.end(); ++i)
      destination->AddLocation(*i, meta);
  }
  Arc::DataMover mover;
  mover.secure(secure);
  mover.passive(passive);
  mover.verbose(verbose);
  mover.force_to_meta(force_meta);
  if (tries) { // 0 means default behavior
    mover.retry(true); // go through all locations
    source->SetTries(tries); // try all locations "tries" times
    destination->SetTries(tries);
  }
  Arc::FileCache cache;
  Arc::User cache_user;
  std::string job_id(Arc::UUID());
  // always copy to destination rather than link
  if (!cache_dir.empty()) cache = Arc::FileCache(cache_dir+" .", job_id, cache_user.get_uid(), cache_user.get_gid());
  if (verbose)
    mover.set_progress_indicator(&progress);
  Arc::DataStatus res = mover.Transfer(*source, *destination, cache, Arc::URLMap(),
                                       0, 0, 0, timeout);
  if (verbose) std::cerr<<std::endl;
  // clean up joblinks created during cache procedure
  if (cache)
    cache.Release();
  if (!res.Passed()) {
    logger.msg(Arc::ERROR, "Transfer FAILED: %s", std::string(res));
    if (res.Retryable())
      logger.msg(Arc::ERROR, "This seems like a temporary error, please try again later");
    return false;
  }
  logger.msg(Arc::INFO, "Transfer complete");
  return true;
}

int main(int argc, char **argv) {

  setlocale(LC_ALL, "");

  Arc::LogStream logcerr(std::cerr);
  logcerr.setFormat(Arc::ShortFormat);
  Arc::Logger::getRootLogger().addDestination(logcerr);
  Arc::Logger::getRootLogger().setThreshold(Arc::WARNING);

  Arc::ArcLocation::Init(argv[0]);

  Arc::OptionParser options(istring("source destination"),
                            istring("The arccp command copies files to, from "
                                    "and between grid storage elements."));

  bool passive = false;
  options.AddOption('p', "passive",
                    istring("use passive transfer (does not work if secure "
                            "is on, default if secure is not requested)"),
                    passive);

  bool notpassive = false;
  options.AddOption('n', "nopassive",
                    istring("do not try to force passive transfer"),
                    notpassive);

  bool force = false;
  options.AddOption('f', "force",
                    istring("if the destination is an indexing service "
                            "and not the same as the source and the "
                            "destination is already registered, then "
                            "the copy is normally not done. However, if "
                            "this option is specified the source is "
                            "assumed to be a replica of the destination "
                            "created in an uncontrolled way and the "
                            "copy is done like in case of replication. "
                            "Using this option also skips validation of "
                            "completed transfers."),
                    force);

  bool verbose = false;
  options.AddOption('i', "indicate", istring("show progress indicator"),
                    verbose);

  bool nocopy = false;
  options.AddOption('T', "notransfer",
                    istring("do not transfer file, just register it - "
                            "destination must be non-existing meta-url"),
                    nocopy);

  bool secure = false;
  options.AddOption('u', "secure",
                    istring("use secure transfer (insecure by default)"),
                    secure);

  std::string cache_path;
  options.AddOption('y', "cache",
                    istring("path to local cache (use to put file into cache)"),
                    istring("path"), cache_path);

  int recursion = 0;
  options.AddOption('r', "recursive",
                    istring("operate recursively up to specified level"),
                    istring("level"), recursion);

  int retries = 0;
  options.AddOption('R', "retries",
                    istring("number of retries before failing file transfer"),
                    istring("number"), retries);

  std::list<std::string> locations;
  options.AddOption('L', "location",
                    istring("physical location to write to when destination is an indexing service."
                            " Must be specified for indexing services which do not automatically"
                            " generate physical locations. Can be specified multiple times -"
                            " locations will be tried in order until one succeeds."),
                    istring("URL"), locations);

  bool show_plugins = false;
  options.AddOption('P', "listplugins",
                    istring("list the available plugins (protocols supported)"),
                    show_plugins);

  int timeout = 20;
  options.AddOption('t', "timeout", istring("timeout in seconds (default 20)"),
                    istring("seconds"), timeout);

  std::string conffile;
  options.AddOption('z', "conffile",
                    istring("configuration file (default ~/.arc/client.conf)"),
                    istring("filename"), conffile);

  std::string debug;
  options.AddOption('d', "debug",
                    istring("FATAL, ERROR, WARNING, INFO, VERBOSE or DEBUG"),
                    istring("debuglevel"), debug);

  bool version = false;
  options.AddOption('v', "version", istring("print version information"),
                    version);

  std::list<std::string> params = options.Parse(argc, argv);

  if (version) {
    std::cout << Arc::IString("%s version %s", "arccp", VERSION) << std::endl;
    return 0;
  }

  // If debug is specified as argument, it should be set before loading the configuration.
  if (!debug.empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(debug));

  if (show_plugins) {
    std::list<Arc::ModuleDesc> modules;
    Arc::PluginsFactory pf(Arc::BaseConfig().MakeConfig(Arc::Config()).Parent());
    pf.scan(Arc::FinderLoader::GetLibrariesList(), modules);
    Arc::PluginsFactory::FilterByKind("HED:DMC", modules);

    std::cout << Arc::IString("Protocol plugins available:") << std::endl;
    for (std::list<Arc::ModuleDesc>::iterator itMod = modules.begin();
         itMod != modules.end(); itMod++) {
      for (std::list<Arc::PluginDesc>::iterator itPlug = itMod->plugins.begin();
           itPlug != itMod->plugins.end(); itPlug++) {
        std::cout << "  " << itPlug->name << " - " << itPlug->description << std::endl;
      }
    }
    return 0;
  }

  // credentials will be initialised later if necessary
  Arc::UserConfig usercfg(conffile, Arc::initializeCredentialsType::SkipCredentials);
  if (!usercfg) {
    logger.msg(Arc::ERROR, "Failed configuration initialization");
    return 1;
  }
  usercfg.UtilsDirPath(Arc::UserConfig::ARCUSERDIRECTORY);

  if (debug.empty() && !usercfg.Verbosity().empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(usercfg.Verbosity()));

  if (params.size() != 2) {
    logger.msg(Arc::ERROR, "Wrong number of parameters specified");
    return 1;
  }

  if (passive && notpassive) {
    logger.msg(Arc::ERROR, "Options 'p' and 'n' can't be used simultaneously");
    return 1;
  }

  if ((!secure) && (!notpassive))
    passive = true;

  std::list<std::string>::iterator it = params.begin();
  std::string source = *it;
  ++it;
  std::string destination = *it;

  if (source == "-") source = "stdio:///stdin";
  if (destination == "-") destination = "stdio:///stdout";
  if (nocopy) {
    if(!arcregister(source, destination, locations, usercfg, secure, passive, force, timeout))
      return 1;
  } else {
    if(!arccp(source, destination, locations, cache_path, usercfg, secure, passive, force,
          recursion, retries + 1, verbose, timeout))
      return 1;
  }

  return 0;
}
