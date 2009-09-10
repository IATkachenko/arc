// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstdlib>
// NOTE: On Solaris errno is not working properly if cerrno is included first
#include <cerrno>

#include <sys/types.h>
#include <unistd.h>

#include <glibmm/random.h>

#include <arc/DateTime.h>
#include <arc/Logger.h>
#include <arc/Thread.h>
#include <arc/URL.h>
#include <arc/UserConfig.h>
#include <arc/Utils.h>
#include <arc/credential/Credential.h>
#include <arc/data/DataBuffer.h>
#include <arc/data/CheckSum.h>
#include <arc/data/DataMover.h>
#include <arc/data/DataPoint.h>
#include <arc/data/DataHandle.h>
#include <arc/data/FileCache.h>
#include <arc/data/MkDirRecursive.h>
#include <arc/data/URLMap.h>

#ifdef WIN32
#include <arc/win32.h>
#endif

namespace Arc {

  Logger DataMover::logger(Logger::getRootLogger(), "DataMover");

  DataMover::DataMover()
    : be_verbose(false),
      force_secure(false),
      force_passive(false),
      force_registration(false),
      do_checks(true),
      do_retries(true),
      default_min_speed(0),
      default_min_speed_time(0),
      default_min_average_speed(0),
      default_max_inactivity_time(300),
      show_progress(NULL) {}

  DataMover::~DataMover() {}

  bool DataMover::verbose() {
    return be_verbose;
  }

  void DataMover::verbose(bool val) {
    be_verbose = val;
  }

  void DataMover::verbose(const std::string& prefix) {
    be_verbose = true;
    verbose_prefix = prefix;
  }

  bool DataMover::retry() {
    return do_retries;
  }

  void DataMover::retry(bool val) {
    do_retries = val;
  }

  bool DataMover::checks() {
    return do_checks;
  }

  void DataMover::checks(bool val) {
    do_checks = val;
  }

  typedef struct {
    DataPoint *source;
    DataPoint *destination;
    FileCache *cache;
    const URLMap *map;
    unsigned long long int min_speed;
    time_t min_speed_time;
    unsigned long long int min_average_speed;
    time_t max_inactivity_time;
    DataMover::callback cb;
    DataMover *it;
    void *arg;
    const char *prefix;
  } transfer_struct;

  DataStatus DataMover::Delete(DataPoint& url, bool errcont) {
    bool remove_lfn = !url.HaveLocations(); // pfn or plain url
    if (!url.Resolve(true).Passed())
      // TODO: Check if error is real or "not exist".
      if (remove_lfn)
        logger.msg(INFO,
                   "No locations found - probably no more physical instances");
    std::list<URL> removed_urls;
    if (url.HaveLocations())
      for (; url.LocationValid();) {
        logger.msg(INFO, "Removing %s", url.CurrentLocation().str());
        // It can happen that after resolving list contains duplicated
        // physical locations obtained from different meta-data-services.
        // Because not all locations can reliably say if files does not exist
        // or access is not allowed, avoid duplicated delete attempts.
        bool url_was_deleted = false;
        for (std::list<URL>::iterator u = removed_urls.begin(); u
             != removed_urls.end(); ++u)
          if (url.CurrentLocation() == (*u)) {
            url_was_deleted = true;
            break;
          }
        if (url_was_deleted) {
          logger.msg(VERBOSE, "This instance was already deleted");
        }
        else {
          url.SetSecure(false);
          if (!url.Remove()) {
            logger.msg(INFO, "Failed to delete physical file");
            if (!errcont) {
              url.NextLocation();
              continue;
            }
          }
          else
            removed_urls.push_back(url.CurrentLocation());
        }
        if (url.IsIndex()) {
          logger.msg(INFO, "Removing metadata in %s",
                     url.CurrentLocationMetadata());
          DataStatus err = url.Unregister(false);
          if (!err) {
            logger.msg(ERROR, "Failed to delete meta-information");
            url.NextLocation();
          }
          else
            url.RemoveLocation();
        }
        else {
          // Leave immediately in case of direct URL
          break;
        }
      }
    if (url.IsIndex()) {
      if (url.HaveLocations()) {
        logger.msg(ERROR, "Failed to remove all physical instances");
        return DataStatus::DeleteError;
      }
      if (remove_lfn) {
        logger.msg(INFO, "Removing logical file from metadata %s", url.str());
        DataStatus err = url.Unregister(true);
        if (!err) {
          logger.msg(ERROR, "Failed to delete logical file");
          return err;
        }
      }
    }
    else {
      if (!url.LocationValid()) {
        logger.msg(ERROR, "Failed to remove instance");
        return DataStatus::DeleteError;
      }
    }
    return DataStatus::Success;
  }

  void transfer_func(void *arg) {
    transfer_struct *param = (transfer_struct*)arg;
    DataStatus res = param->it->Transfer(*(param->source),
                                         *(param->destination), *(param->cache), *(param->map),
                                         param->min_speed, param->min_speed_time,
                                         param->min_average_speed, param->max_inactivity_time,
                                         NULL, NULL, param->prefix);
    (*(param->cb))(param->it, res, param->arg);
    if (param->prefix)
      free((void*)(param->prefix));
    delete param->cache;
    free(param);
  }

  /* transfer data from source to destination */
  DataStatus DataMover::Transfer(DataPoint& source, DataPoint& destination,
                                 FileCache& cache, const URLMap& map,
                                 DataMover::callback cb, void *arg, const char *prefix) {
    return Transfer(source, destination, cache, map, default_min_speed,
                    default_min_speed_time, default_min_average_speed,
                    default_max_inactivity_time, cb, arg, prefix);
  }

  DataStatus DataMover::Transfer(DataPoint& source, DataPoint& destination,
                                 FileCache& cache, const URLMap& map, unsigned long long int min_speed,
                                 time_t min_speed_time, unsigned long long int
                                 min_average_speed, time_t max_inactivity_time,
                                 DataMover::callback cb, void *arg,
                                 const char *prefix) {

    UserConfig usercfg(true); // need to get this from somewhere...

    if (cb != NULL) {
      logger.msg(DEBUG, "DataMover::Transfer : starting new thread");
      transfer_struct *param = (transfer_struct*)malloc(sizeof(transfer_struct));
      if (param == NULL)
        return DataStatus::TransferError;
      param->source = &source;
      param->destination = &destination;
      param->cache = new FileCache(cache);
      param->map = &map;
      param->min_speed = min_speed;
      param->min_speed_time = min_speed_time;
      param->min_average_speed = min_average_speed;
      param->max_inactivity_time = max_inactivity_time;
      param->cb = cb;
      param->it = this;
      param->arg = arg;
      param->prefix = NULL;
      if (prefix)
        param->prefix = strdup(prefix);
      if (param->prefix == NULL)
        param->prefix = strdup(verbose_prefix.c_str());
      if (!CreateThreadFunction(&transfer_func, param)) {
        free(param);
        return DataStatus::TransferError;
      }
      return DataStatus::Success;
    }
    logger.msg(INFO, "Transfer from %s to %s", source.str(), destination.str());
    if (!source) {
      logger.msg(ERROR, "Not valid source");
      source.NextTry();
      return DataStatus::ReadAcquireError;
    }
    if (!destination) {
      logger.msg(ERROR, "Not valid destination");
      destination.NextTry();
      return DataStatus::WriteAcquireError;
    }
    for (;;) {
      // if(source.Resolve(true, map)) {
      DataStatus dres = source.Resolve(true);
      if (dres.Passed()) {
        if (source.HaveLocations())
          break;
        logger.msg(ERROR, "No locations for source found: %s", source.str());
      }
      else
        logger.msg(ERROR, "Failed to resolve source: %s", source.str());
      source.NextTry(); /* try again */
      if (!do_retries)
        return dres;
      if (!source.LocationValid())
        return dres;
    }
    for (;;) {
      // if(destination.Resolve(false, URLMap())) {
      DataStatus dres = destination.Resolve(false);
      if (dres.Passed()) {
        if (destination.HaveLocations())
          break;
        logger.msg(ERROR, "No locations for destination found: %s",
                   destination.str());
      }
      else
        logger.msg(ERROR, "Failed to resolve destination: %s", destination.str());
      destination.NextTry(); /* try again */
      if (!do_retries)
        return dres;
      if (!destination.LocationValid())
        return dres;
    }
    bool replication = false;
    if (source.IsIndex() && destination.IsIndex())
      // check for possible replication
      if (source.GetURL() == destination.GetURL()) {
        replication = true;
        // we do not want to replicate to same site
        destination.RemoveLocations(source);
        if (!destination.HaveLocations()) {
          logger.msg(ERROR, "No locations for destination different from source "
                     "found: %s", destination.str());
          return DataStatus::WriteResolveError;
        }
      }
    // Try to avoid any additional checks meant to provide
    // meta-information whenever possible
    bool checks_required = destination.AcceptsMeta() && (!replication);
    bool destination_meta_initially_stored = destination.Registered();
    bool destination_overwrite = false;
    if (!replication) { // overwriting has no sense in case of replication
      std::string value = destination.GetURL().Option("overwrite", "no");
      if (strcasecmp(value.c_str(), "no") != 0)
        destination_overwrite = true;
    }
    if (destination_overwrite) {
      if ((destination.IsIndex() && destination_meta_initially_stored)
          || (!destination.IsIndex())) {
        URL del_url = destination.GetURL();
        logger.msg(DEBUG, "DataMover::Transfer: trying to destroy/overwrite "
                   "destination: %s", del_url.str());
        int try_num = destination.GetTries();
        for (;;) {
          DataHandle del(del_url, usercfg);
          del->SetTries(1);
          DataStatus res = Delete(*del);
          if (res == DataStatus::Success)
            break;
          if (!destination.IsIndex()) {
            // pfn has chance to be overwritten directly
            logger.msg(INFO, "Failed to delete %s but will still try to upload", del_url.str());
            break;
          }
          logger.msg(INFO, "Failed to delete %s", del_url.str());
          destination.NextTry(); /* try again */
          if (!do_retries)
            return res;
          if ((--try_num) <= 0)
            return res;
        }
        if (destination.IsIndex()) {
          for (;;) {
            DataStatus dres = destination.Resolve(false);
            if (dres.Passed()) {
              if (destination.HaveLocations())
                break;
              logger.msg(ERROR, "No locations for destination found: %s",
                         destination.str());
            }
            else
              logger.msg(ERROR, "Failed to resolve destination: %s",
                         destination.str());
            destination.NextTry(); /* try again */
            if (!do_retries)
              return dres;
            if (!destination.LocationValid())
              return dres;
          }
          destination_meta_initially_stored = destination.Registered();
          if (destination_meta_initially_stored) {
            logger.msg(INFO, "Deleted but still have locations at %s",
                       destination.str());
            return DataStatus::WriteResolveError;
          }
        }
      }
    }
    DataStatus res = DataStatus::TransferError;
    int try_num;
    for (try_num = 0;; try_num++) { /* cycle for retries */
      logger.msg(DEBUG, "DataMover: cycle");
      if ((try_num != 0) && (!do_retries)) {
        logger.msg(DEBUG, "DataMover: no retries requested - exit");
        return res;
      }
      if ((!source.LocationValid()) || (!destination.LocationValid())) {
        if (!source.LocationValid())
          logger.msg(DEBUG, "DataMover: source out of tries - exit");
        if (!destination.LocationValid())
          logger.msg(DEBUG, "DataMover: destination out of tries - exit");
        /* out of tries */
        return res;
      }
      // By putting DataBuffer here, one makes sure it will be always
      // destroyed AFTER all DataHandle. This allows for not bothering
      // to call stop_reading/stop_writing because they are called in
      // destructor of DataHandle.
      DataBuffer buffer;
      logger.msg(INFO, "Real transfer from %s to %s", source.CurrentLocation().str(), destination.CurrentLocation().str());
      /* creating handler for transfer */
      source.SetSecure(force_secure);
      source.Passive(force_passive);
      destination.SetSecure(force_secure);
      destination.Passive(force_passive);
      destination.SetAdditionalChecks(do_checks);
      /* take suggestion from DataHandle about buffer, etc. */
      bool cacheable = false;
      /* is file executable */
      bool executable = false;
      std::string exec_option = source.GetURL().Option("exec");
      if (exec_option == "yes")
        executable = true;
      bool cache_copy = false;
      std::string cache_option = source.GetURL().Option("cache");
      if (cache_option == "copy")
        cache_copy = true;
      long long int bufsize;
      int bufnum;
      if (source.Cache() && destination.Local() && cache)
        cacheable = true;
      /* tune buffers */
      bufsize = 65536; /* have reasonable buffer size */
      bool seekable = destination.WriteOutOfOrder();
      source.ReadOutOfOrder(seekable);
      bufnum = 1;
      if (source.BufSize() > bufsize)
        bufsize = source.BufSize();
      if (destination.BufSize() > bufsize)
        bufsize = destination.BufSize();
      if (seekable) {
        if (source.BufNum() > bufnum)
          bufnum = source.BufNum();
        if (destination.BufNum() > bufnum)
          bufnum = destination.BufNum();
      }
      bufnum = bufnum * 2;
      logger.msg(DEBUG, "Creating buffer: %lli x %i", bufsize, bufnum);
      /* prepare crc */
      CheckSumAny crc;
      // Shold we trust indexing service or always compute checksum ?
      // Let's trust.
      if (destination.AcceptsMeta()) { // may need to compute crc
        // Let it be CRC32 by default.
        std::string crc_type = destination.GetURL().Option("checksum", "cksum");
        logger.msg(DEBUG, "DataMover::Transfer: checksum type is %s", crc_type);
        if (!source.CheckCheckSum()) {
          crc = crc_type.c_str();
          logger.msg(DEBUG, "DataMover::Transfer: will try to compute crc");
        }
        else if (CheckSumAny::Type(crc_type.c_str())
                 != CheckSumAny::Type(source.GetCheckSum().c_str())) {
          crc = crc_type.c_str();
          logger.msg(DEBUG, "DataMover::Transfer: will try to compute crc");
        }
      }
      /* create buffer and tune speed control */
      buffer.set(&crc, bufsize, bufnum);
      if (!buffer)
        logger.msg(INFO, "Buffer creation failed !");
      buffer.speed.set_min_speed(min_speed, min_speed_time);
      buffer.speed.set_min_average_speed(min_average_speed);
      buffer.speed.set_max_inactivity_time(max_inactivity_time);
      buffer.speed.verbose(be_verbose);
      if (be_verbose) {
        if (prefix)
          buffer.speed.verbose(std::string(prefix));
        else
          buffer.speed.verbose(verbose_prefix);
        buffer.speed.set_progress_indicator(show_progress);
      }
      /* checking if current source should be mapped to different location */
      /* TODO: make mapped url to be handled by source handle directly */
      bool mapped = false;
      URL mapped_url;
      if (destination.Local()) {
        mapped_url = source.CurrentLocation();
        mapped = map.map(mapped_url);
        /* TODO: copy options to mapped_url */
        if (!mapped)
          mapped_url = URL();
        else {
          logger.msg(DEBUG, "Url is mapped to: %s", mapped_url.str());
          if ((mapped_url.Protocol() == "link") || (mapped_url.Protocol()
                                                    == "file"))
            /* can't cache links */
            cacheable = false;
        }
      }
      // Do not link if user asks. Replace link:// with file://
      if (source.ReadOnly() && mapped)
        if (mapped_url.Protocol() == "link")
          mapped_url.ChangeProtocol("file");
      DataHandle mapped_h(mapped_url, usercfg);
      DataPoint& mapped_p(*mapped_h);
      if (mapped_h) {
        mapped_p.SetSecure(force_secure);
        mapped_p.Passive(force_passive);
      }
      /* Try to initiate cache (if needed) */
      std::string canonic_url = source.str();
#ifndef WIN32
      if (cacheable) {
        res = DataStatus::Success;
        for (;;) { /* cycle for outdated cache files */
          bool is_in_cache = false;
          bool is_locked = false;
          if (!cache.Start(canonic_url, is_in_cache, is_locked)) {
            if (is_locked) {
              logger.msg(DEBUG, "Cached file is locked - should retry");
              source.NextLocation(); /* to decrease retry counter */
              return DataStatus::CacheErrorRetryable;
            }
            cacheable = false;
            logger.msg(INFO, "Failed to initiate cache");
            break;
          }
          if (is_in_cache) {
            // check for forced re-download option
            std::string cache_option = source.GetURL().Option("cache");
            if (cache_option == "renew") {
              logger.msg(DEBUG, "Forcing re-download of file %s", canonic_url);
              cache.StopAndDelete(canonic_url);
              continue;
            }
            /* just need to check permissions */
            logger.msg(INFO, "File %s is cached (%s) - checking permissions",
                       canonic_url, cache.File(canonic_url));
            // check the list of cached DNs
            bool have_permission = false;
            std::string dn;
            Time exp_time(0);
            try {
              // TODO (important) load credential in unified way or 
              // use already loaded one
              Credential ci(GetEnv("X509_USER_PROXY"), GetEnv("X509_USER_PROXY"), GetEnv("X509_CERT_DIR"), "");
              dn = ci.GetIdentityName();
              if (cache.CheckDN(canonic_url, dn))
                have_permission = true;
              exp_time = ci.GetEndTime();
            } catch (CredentialError e) {
              logger.msg(WARNING, "Couldn't handle certificate: %s", e.what());
            }
            if (!have_permission) {
              DataStatus cres = source.Check();
              if (!cres.Passed()) {
                logger.msg(ERROR, "Permission checking failed: %s", source.str());
                cache.Stop(canonic_url);
                source.NextLocation(); /* try another source */
                logger.msg(DEBUG, "source.next_location");
                res = cres;
                break;
              }
              cache.AddDN(canonic_url, dn, exp_time);
            }
            logger.msg(DEBUG, "Permission checking passed");
            /* check if file is fresh enough */
            bool outdated = true;
            if (have_permission)
              outdated = false; // cached DN means don't check creation date
            if (source.CheckCreated() && cache.CheckCreated(canonic_url)) {
              Time sourcetime = source.GetCreated();
              Time cachetime = cache.GetCreated(canonic_url);
              logger.msg(DEBUG, "Source creation date: %s", sourcetime.str());
              logger.msg(DEBUG, "Cache creation date: %s", cachetime.str());
              if (sourcetime <= cachetime)
                outdated = false;
            }
            if (cache.CheckValid(canonic_url)) {
              Time validtime = cache.GetValid(canonic_url);
              logger.msg(DEBUG, "Cache file valid until: %s", validtime.str());
              if (validtime > Time())
                outdated = false;
              else
                outdated = true;
            }
            if (outdated) {
              cache.StopAndDelete(canonic_url);
              logger.msg(INFO, "Cached file is outdated, will re-download");
              continue;
            }
            logger.msg(DEBUG, "Cached copy is still valid");
            if (source.ReadOnly() && !executable && !cache_copy) {
              logger.msg(DEBUG, "Linking/copying cached file");
              if (!cache.Link(destination.CurrentLocation().Path(), canonic_url)) {
                /* failed cache link is unhandable */
                cache.Stop(canonic_url);
                source.NextLocation(); /* to decrease retry counter */
                return DataStatus::CacheError;
              }
            }
            else {
              logger.msg(DEBUG, "Copying cached file");
              if (!cache.Copy(destination.CurrentLocation().Path(), canonic_url, executable)) {
                /* failed cache copy is unhandable */
                cache.Stop(canonic_url);
                source.NextLocation(); /* to decrease retry counter */
                return DataStatus::CacheError;
              }
            }
            cache.Stop(canonic_url);
            return DataStatus::Success;
            // Leave here. Rest of code below is for transfer.
          }
          break;
        }
        if (cacheable && !res.Passed())
          continue;
      }
#endif /*WIN32*/
      if (mapped) {
        if ((mapped_url.Protocol() == "link")
            || (mapped_url.Protocol() == "file")) {
          /* check permissions first */
          logger.msg(INFO, "URL is mapped to local access - "
                     "checking permissions on original URL");
          DataStatus cres =  source.Check();
          if (!cres.Passed()) {
            logger.msg(ERROR, "Permission checking on original URL failed: %s",
                       source.str());
            source.NextLocation(); /* try another source */
            logger.msg(DEBUG, "source.next_location");
            res = cres;
#ifndef WIN32
            if (cacheable)
              cache.StopAndDelete(canonic_url);
#endif
            continue;
          }
          logger.msg(DEBUG, "Permission checking passed");
          if (mapped_url.Protocol() == "link") {
            logger.msg(DEBUG, "Linking local file");
            const std::string& file_name = mapped_url.Path();
            const std::string& link_name = destination.CurrentLocation().Path();
            // create directory structure for link_name
            {
              User user;
              std::string dirpath = link_name;
              std::string::size_type n = dirpath.rfind('/');
              if (n == std::string::npos)
                n = 0;
              if (n == 0)
                dirpath = "/";
              else
                dirpath.resize(n);
              if (mkdir_recursive(NULL, dirpath.c_str(), S_IRWXU, user) != 0) {
                if (errno != EEXIST) {
                  logger.msg(ERROR, "Failed to create/find directory %s : %s",
                             dirpath, StrError());
                  source.NextLocation(); /* try another source */
                  logger.msg(DEBUG, "source.next_location");
                  res = DataStatus::ReadStartError;
#ifndef WIN32
                  if (cacheable)
                    cache.StopAndDelete(canonic_url);
#endif
                  continue;
                }
              }
            }
            // make link
            if (symlink(file_name.c_str(), link_name.c_str()) == -1) {
              logger.msg(ERROR, "Failed to make symbolic link %s to %s : %s",
                         link_name, file_name, StrError());
              source.NextLocation(); /* try another source */
              logger.msg(DEBUG, "source.next_location");
              res = DataStatus::ReadStartError;
#ifndef WIN32
              if (cacheable)
                cache.StopAndDelete(canonic_url);
#endif
              continue;
            }
            User user;
            (lchown(link_name.c_str(), user.get_uid(), user.get_gid()) != 0);
#ifndef WIN32
            if (cacheable)
              cache.Stop(canonic_url);
#endif
            return DataStatus::Success;
            // Leave after making a link. Rest moves data.
          }
        }
      }
      URL churl;
#ifndef WIN32
      if (cacheable) {
        /* create new destination for cache file */
        churl = cache.File(canonic_url);
        logger.msg(INFO, "cache file: %s", churl.Path());
      }
#endif
      DataHandle chdest_h(churl, usercfg);
      DataPoint& chdest(*chdest_h);
      if (chdest_h) {
        chdest.SetSecure(force_secure);
        chdest.Passive(force_passive);
        chdest.SetAdditionalChecks(do_checks);
        chdest.SetMeta(destination); // share metadata
      }
      DataPoint& source_url = mapped ? mapped_p : source;
      DataPoint& destination_url = cacheable ? chdest : destination;
      // Disable checks meant to provide meta-information if not needed
      source_url.SetAdditionalChecks(do_checks & (checks_required | cacheable));
      DataStatus datares = source_url.StartReading(buffer);
      if (!datares.Passed()) {
        logger.msg(ERROR, "Failed to start reading from source: %s",
                   source_url.str());
        res = datares;
        if (source.GetFailureReason() != DataStatus::UnknownError)
          res = source.GetFailureReason();
        /* try another source */
        if (source.NextLocation())
          logger.msg(DEBUG, "(Re)Trying next source");
#ifndef WIN32
        if (cacheable)
          cache.StopAndDelete(canonic_url);
#endif
        continue;
      }
      if (mapped)
        destination.SetMeta(mapped_p);
      if (force_registration && destination.IsIndex()) {
        // at least compare metadata
        if (!destination.CompareMeta(source)) {
          logger.msg(ERROR, "Metadata of source and destination are different");
          source.NextLocation(); /* not exactly sure if this would help */
          res = DataStatus::PreRegisterError;
#ifndef WIN32
          if (cacheable)
            cache.StopAndDelete(canonic_url);
#endif
          continue;
        }
      }
      destination.SetMeta(source);
      // pass metadata gathered during start_reading()
      // from source to destination
      if (destination.CheckSize())
        buffer.speed.set_max_data(destination.GetSize());
      datares = destination.PreRegister(replication, force_registration);
      if (!datares.Passed()) {
        logger.msg(ERROR, "Failed to preregister destination: %s",
                   destination.str());
        destination.NextLocation(); /* not exactly sure if this would help */
        logger.msg(DEBUG, "destination.next_location");
        res = datares;
        // Normally remote destination is not cached. But who knows.
#ifndef WIN32
        if (cacheable)
          cache.StopAndDelete(canonic_url);
#endif
        continue;
      }
      buffer.speed.reset();
      DataStatus read_failure = DataStatus::Success;
      DataStatus write_failure = DataStatus::Success;
      if (!cacheable) {
        datares = destination.StartWriting(buffer);
        if (!datares.Passed()) {
          logger.msg(ERROR, "Failed to start writing to destination: %s",
                     destination.str());
          source_url.StopReading();
          if (!destination.PreUnregister(replication ||
                                         destination_meta_initially_stored).Passed())
            logger.msg(ERROR, "Failed to unregister preregistered lfn. "
                       "You may need to unregister it manually: %s", destination.str());
          if (destination.NextLocation())
            logger.msg(DEBUG, "(Re)Trying next destination");
          res = datares;
          if(destination.GetFailureReason() != DataStatus::UnknownError)
            res = destination.GetFailureReason();
          continue;
        }
      }
      else {
#ifndef WIN32
        datares = chdest.StartWriting(buffer);
        if (!datares.Passed()) {
          // TODO: put callback to clean cache into FileCache
          logger.msg(ERROR, "Failed to start writing to cache");
          source_url.StopReading();
          // hope there will be more space next time
          cache.StopAndDelete(canonic_url);
          if (!destination.PreUnregister(replication ||
                                         destination_meta_initially_stored).Passed())
            logger.msg(ERROR, "Failed to unregister preregistered lfn. "
                       "You may need to unregister it manually");
          return DataStatus::CacheError; // repeating won't help here
        }
#endif
      }
      logger.msg(DEBUG, "Waiting for buffer");
      for (; (!buffer.eof_read() || !buffer.eof_write()) && !buffer.error();)
        buffer.wait_any();
      logger.msg(INFO, "buffer: read eof : %i", (int)buffer.eof_read());
      logger.msg(INFO, "buffer: write eof: %i", (int)buffer.eof_write());
      logger.msg(INFO, "buffer: error    : %i", (int)buffer.error());
      logger.msg(DEBUG, "Closing read channel");
      read_failure = source_url.StopReading();
      if (cacheable && mapped)
        source.SetMeta(mapped_p); // pass more metadata (checksum)
      logger.msg(DEBUG, "Closing write channel");
      if (!destination_url.StopWriting().Passed())
        buffer.error_write(true);

      if (buffer.error()) {
#ifndef WIN32
        if (cacheable) 
          cache.StopAndDelete(canonic_url);
#endif        
        if (!destination.PreUnregister(replication ||
                                       destination_meta_initially_stored).Passed())
          logger.msg(ERROR, "Failed to unregister preregistered lfn. "
                     "You may need to unregister it manually");
        // Analyze errors
        // Easy part first - if either read or write part report error
        // go to next endpoint.
        if (buffer.error_read()) {
          if (source.NextLocation())
            logger.msg(DEBUG, "(Re)Trying next source");
          // check for error from callbacks etc
          if(source.GetFailureReason() != DataStatus::UnknownError)
            res=source.GetFailureReason();
          else
            res=DataStatus::ReadError;
        }
        else if (buffer.error_write()) {
          if (destination.NextLocation())
            logger.msg(DEBUG, "(Re)Trying next destination");
          // check for error from callbacks etc
          if(destination.GetFailureReason() != DataStatus::UnknownError)
            res=destination.GetFailureReason();
          else
            res=DataStatus::WriteError;
        }
        else if (buffer.error_transfer()) {
          // Here is more complicated case - operation timeout
          // Let's first check if buffer was full
          res = DataStatus::TransferError;
          if (!buffer.for_read()) {
            // No free buffers for 'read' side. Buffer must be full.
            res.SetDesc(destination.GetFailureReason().GetDesc());
            if (destination.NextLocation())
              logger.msg(DEBUG, "(Re)Trying next destination");
          }
          else if (!buffer.for_write()) {
            // Buffer is empty
            res.SetDesc(source.GetFailureReason().GetDesc());
            if (source.NextLocation())
              logger.msg(DEBUG, "(Re)Trying next source");
          }
          else {
            // Both endpoints were very slow? Choose randomly.
            logger.msg(DEBUG, "Cause of failure unclear - choosing randomly");
            Glib::Rand r;
            if (r.get_int() < (RAND_MAX / 2)) {
              res.SetDesc(source.GetFailureReason().GetDesc());
              if (source.NextLocation())
                logger.msg(DEBUG, "(Re)Trying next source");
            }
            else {
              res.SetDesc(destination.GetFailureReason().GetDesc());
              if (destination.NextLocation())
                logger.msg(DEBUG, "(Re)Trying next destination");
            }
          }
        }
        continue;
      }
      // check if checksum is specified as a metadata attribute
      if (!destination.GetURL().MetaDataOption("checksumtype").empty() &&
          !destination.GetURL().MetaDataOption("checksumvalue").empty()) {
        std::string csum = destination.GetURL().MetaDataOption("checksumtype") + ":" + destination.GetURL().MetaDataOption("checksumvalue");
        source.SetCheckSum(csum.c_str());
        logger.msg(DEBUG, "DataMove::Transfer: using supplied checksum %s", csum);
      }
      else if (crc)
        if (buffer.checksum_valid()) {
          // source.meta_checksum(crc.end());
          char buf[100];
          crc.print(buf, 100);
          source.SetCheckSum(buf);
          logger.msg(DEBUG, "DataMover::Transfer: have valid checksum");
        }
      destination.SetMeta(source); // pass more metadata (checksum)
      datares = destination.PostRegister(replication);
      if (!datares.Passed()) {
        logger.msg(ERROR, "Failed to postregister destination %s",
                   destination.str());
        if (!destination.PreUnregister(replication ||
                                       destination_meta_initially_stored).Passed())
          logger.msg(ERROR, "Failed to unregister preregistered lfn. "
                     "You may need to unregister it manually: %s", destination.str());
        destination.NextLocation(); /* not sure if this can help */
        logger.msg(DEBUG, "destination.next_location");
#ifndef WIN32
        if(cacheable) 
          cache.Stop(canonic_url);
#endif
        res = datares;
        continue;
      }
#ifndef WIN32
      if (cacheable) {
        if (source.CheckValid())
          cache.SetValid(canonic_url, source.GetValid());
        try {
          // TODO (important) load credential in unified way or
          // use already loaded one
          Credential ci(GetEnv("X509_USER_PROXY"), GetEnv("X509_USER_PROXY"), GetEnv("X509_CERT_DIR"), "");
          cache.AddDN(canonic_url, ci.GetIdentityName(), ci.GetEndTime());
        } catch (CredentialError e) {
          logger.msg(WARNING, "Couldn't handle certificate: %s", e.what());
        }
        bool cache_link_result;
        if (executable || cache_copy) {
          logger.msg(DEBUG, "Copying cached file");
          cache_link_result = cache.Copy(destination.CurrentLocation().Path(), canonic_url, executable);
        }
        else {
          logger.msg(DEBUG, "Linking/copying cached file");
          cache_link_result = cache.Link(destination.CurrentLocation().Path(), canonic_url);
        }
        cache.Stop(canonic_url);
        if (!cache_link_result) {
          if (!destination.PreUnregister(replication ||
                                         destination_meta_initially_stored).Passed())
            logger.msg(ERROR, "Failed to unregister preregistered lfn. "
                       "You may need to unregister it manually");
          return DataStatus::CacheError; /* retry won't help */
        }
      }
#endif
      if (buffer.error())
        continue; // should never happen - keep just in case
      break;
    }
    return DataStatus::Success;
  }

  void DataMover::secure(bool val) {
    force_secure = val;
  }

  void DataMover::passive(bool val) {
    force_passive = val;
  }

  void DataMover::force_to_meta(bool val) {
    force_registration = val;
  }

} // namespace Arc
