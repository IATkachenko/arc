// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include <arc/win32.h>
#endif

#include <cstdlib>

#include <globus_io.h>
#include <glibmm/fileutils.h>

#include <arc/Thread.h>
#include <arc/StringConv.h>
#include <arc/Logger.h>
#include <arc/URL.h>
#include <arc/data/DataBuffer.h>
#include <arc/data/DataCallback.h>
#include <arc/globusutils/GlobusWorkarounds.h>


#include "DataPointSRM.h"

namespace Arc {

  Logger DataPointSRM::logger(DataPoint::logger, "SRM");

  static bool proxy_initialized = false;

  DataPointSRM::DataPointSRM(const URL& url)
    : DataPointDirect(url),
      srm_request(NULL),
      r_handle(NULL),
      reading(false),
      writing(false) {
    globus_module_activate(GLOBUS_GSI_GSSAPI_MODULE);
    globus_module_activate(GLOBUS_IO_MODULE);
    if (!proxy_initialized)
      proxy_initialized = GlobusRecoverProxyOpenSSL();
  }

  DataPointSRM::~DataPointSRM() {
    globus_module_deactivate(GLOBUS_GSI_GSSAPI_MODULE);
    globus_module_deactivate(GLOBUS_IO_MODULE);
  }

  DataStatus DataPointSRM::Check() {

    SRMClient *client = SRMClient::getInstance(url.fullstr());
    if (!client)
      return DataStatus::CheckError;

    if (url.HTTPOption("SFN", "") == "")
      srm_request = new SRMClientRequest(url.str());
    else
      srm_request = new SRMClientRequest(url.Protocol() + "://" + url.Host() + "/" + url.HTTPOption("SFN"));
    
    if (!srm_request)
      return DataStatus::CheckError;

    logger.msg(DEBUG, "Check: looking for metadata: %s", CurrentLocation().str());
    std::list<struct SRMFileMetaData> metadata;

    if (!client->info(*srm_request, metadata))
      return DataStatus::CheckError;

    if (metadata.empty())
      return DataStatus::CheckError;
    logger.msg(INFO, "Check: obtained size: %lli", metadata.front().size);
    if (metadata.front().size > 0)
      SetSize(metadata.front().size);
    logger.msg(INFO, "Check: obtained checksum: %s", metadata.front().checkSumValue);
    if (metadata.front().checkSumValue.length() > 0 &&
        metadata.front().checkSumType.length() > 0) {
      std::string csum(metadata.front().checkSumType + ":" + metadata.front().checkSumValue);
      SetCheckSum(csum);
    }
    if (metadata.front().createdAtTime > 0) {
      logger.msg(INFO, "Check: obtained creation date: %s", Time(metadata.front().createdAtTime).str());
      SetCreated(Time(metadata.front().createdAtTime));
    }
    return DataStatus::Success;
  }

  DataStatus DataPointSRM::Remove() {

    SRMClient *client = SRMClient::getInstance(url.fullstr());
    if (!client)
      return DataStatus::DeleteError;

    // take out options in srm url
    std::string canonic_url;
    if (!url.HTTPOption("SFN").empty())
      canonic_url = url.Protocol() + "://" + url.Host() + "/" + url.HTTPOption("SFN");
    else
      canonic_url = url.Protocol() + "://" + url.Host() + "/" + url.Path();

    srm_request = new SRMClientRequest(canonic_url);

    if (!srm_request) {
      delete client;
      client = NULL;
      return DataStatus::DeleteError;
    }
    logger.msg(DEBUG, "remove_srm: deleting: %s", CurrentLocation().str());

    if (!client->remove(*srm_request)) {
      delete client;
      client = NULL;
      return DataStatus::DeleteError;
    }
    if (client) {
      delete client;
      client = NULL;
    }

    return DataStatus::Success;
  }

  DataStatus DataPointSRM::StartReading(DataBuffer& buf) {

    logger.msg(DEBUG, "StartReading");
    if (reading)
      return DataStatus::IsReadingError;
    if (writing)
      return DataStatus::IsWritingError;

    reading = true;
    buffer = &buf;

    SRMClient *client = SRMClient::getInstance(url.fullstr());
    if (!client) {
      reading = false;
      return DataStatus::ReadError;
    }

    // take out options in srm url
    std::string canonic_url;
    if (!url.HTTPOption("SFN").empty())
      canonic_url = url.Protocol() + "://" + url.Host() + "/" + url.HTTPOption("SFN");
    else
      canonic_url = url.Protocol() + "://" + url.Host() + "/" + url.Path();

    srm_request = new SRMClientRequest(canonic_url);
    if (!srm_request) {
      delete client;
      client = NULL;
      return DataStatus::ReadError;
    }

    if (additional_checks) {
      logger.msg(DEBUG, "StartReading: looking for metadata: %s", CurrentLocation().str());
      std::list<struct SRMFileMetaData> metadata;
      if (!client->info(*srm_request, metadata)) {
        reading = false;
        return DataStatus::ReadError;
      }
      // provide some metadata
      if (!metadata.empty()) {
        logger.msg(INFO, "StartReading: obtained size: %lli", metadata.front().size);
        if (metadata.front().size > 0)
          SetSize(metadata.front().size);
        logger.msg(INFO, "StartReading: obtained checksum: %s:%s", metadata.front().checkSumType, metadata.front().checkSumValue);
        if (metadata.front().checkSumValue.length() > 0 &&
            metadata.front().checkSumType.length() > 0) {
          std::string csum(metadata.front().checkSumType + ":" + metadata.front().checkSumValue);
          SetCheckSum(csum);
        }
      }
    }

    std::list<std::string> turls;
    if (!client->getTURLs(*srm_request, turls)) {
      delete client;
      client = NULL;
      return DataStatus::ReadError;
    }
    client->disconnect();

    std::srand(time(NULL));

    // Choose handled URL randomly
    for (;;) {
      if (turls.size() <= 0)
        break;
      int n = (int)((std::rand() * ((double)(turls.size() - 1))) / RAND_MAX + 0.25);
      std::list<std::string>::iterator i = turls.begin();
      for (; n; ++i, ++n) {}
      if (i == turls.end())
        continue;
      // Avoid redirection to SRM
      logger.msg(DEBUG, "Checking URL returned by SRM: %s", *i);
      if (strncasecmp(i->c_str(), "srm://", 6) == 0) {
        turls.erase(i);
        continue;
      }
      // Try to use this TURL + old options
      URL u(*i);
      {
        std::map<std::string, std::string> options = url.Options();
        if (!options.empty())
          for (std::map<std::string, std::string>::iterator oi = options.begin(); oi != options.end(); oi++)
            u.AddOption((*oi).first, (*oi).second);
      }
      r_handle = new DataHandle(u);
      // check if url can be handled
      if (!r_handle) {
        turls.erase(i);
        continue;
      }
      if ((*r_handle)->ProvidesMeta()) {
        delete r_handle;
        r_handle = NULL;
        turls.erase(i);
        continue;
      }
      break;
    }

    if (r_handle == NULL) {
      logger.msg(INFO, "SRM returned no useful Transfer URLs: %s", url.str());
      delete client;
      client = NULL;
      return DataStatus::ReadError;
    }

    (*r_handle)->SetAdditionalChecks(additional_checks);
    (*r_handle)->SetSecure(force_secure);
    (*r_handle)->Passive(force_passive);

    logger.msg(INFO, "Redirecting to new URL: %s", (*r_handle)->CurrentLocation().str());
    if (!(*r_handle)->StartReading(buf)) {
      if (r_handle) {
        delete r_handle;
        r_handle = NULL;
      }
      if (srm_request) {
        delete srm_request;
        srm_request = NULL;
      }
      if (client) {
        delete client;
        client = NULL;
      }
      reading = false;
      return DataStatus::ReadError;
    }
    delete client;
    client = NULL;
    return DataStatus::Success;
  }

  DataStatus DataPointSRM::StopReading() {

    if (!reading)
      return DataStatus::ReadStopError;
    reading = false;

    if (!r_handle)
      return DataStatus::Success;

    DataStatus r = (*r_handle)->StopReading();
    delete r_handle;
    if (srm_request) {
      SRMClient *client = SRMClient::getInstance(url.fullstr());
      if (client)
        client->releaseGet(*srm_request);
      delete srm_request;
      if (client) {
        delete client;
        client = NULL;
      }
    }
    r_handle = NULL;
    srm_request = NULL;
    return r;
  }

  DataStatus DataPointSRM::StartWriting(DataBuffer& buf,
                                        DataCallback *space_cb) {

    logger.msg(DEBUG, "StartWriting");
    if (reading)
      return DataStatus::IsReadingError;
    if (writing)
      return DataStatus::IsWritingError;

    writing = true;
    buffer = &buf;

    SRMClient *client = SRMClient::getInstance(url.fullstr());
    if (!client) {
      writing = false;
      return DataStatus::WriteError;
    }

    // take out options in srm url
    std::string canonic_url;
    if (!url.HTTPOption("SFN").empty())
      canonic_url = url.Protocol() + "://" + url.Host() + "/" + url.HTTPOption("SFN");
    else
      canonic_url = url.Protocol() + "://" + url.Host() + "/" + url.Path();

    srm_request = new SRMClientRequest(canonic_url);
    if (!srm_request) {
      delete client;
      client = NULL;
      return DataStatus::WriteError;
    }

    // set space token
    std::string space_token = url.Option("spacetoken");
    if (space_token.empty()) {
      if (client->getVersion().compare("v2.2") == 0)
        // only print message if using v2.2
        logger.msg(DEBUG, "No space token specified");
    }
    else {
      if (client->getVersion().compare("v2.2") != 0)
        // print warning if not using srm2.2
        logger.msg(WARNING, "Warning: Using SRM protocol v1 which does not support space tokens");
      else {
        logger.msg(DEBUG, "Using space token description %s", space_token);
        // get token from SRM that matches description
        std::list<std::string> tokens;
        if (client->getSpaceTokens(tokens, space_token) != SRM_OK)
          // not critical so log a warning
          logger.msg(WARNING, "Warning: Error looking up space tokens matching description %s. Will copy without using token", space_token);
        else if (tokens.empty())
          // not critical so log a warning
          logger.msg(WARNING, "Warning: No space tokens found matching description! Will copy without using token");
        else {
          // take the first one in the list
          logger.msg(DEBUG, "Using space token %s", tokens.front());
          srm_request->space_token(tokens.front());
        }
      }
    }

    std::list<std::string> turls;
    if (!client->putTURLs(*srm_request, turls)) {
      delete client;
      client = NULL;
      return DataStatus::WriteError;
    }
    client->disconnect();
 
    std::srand(time(NULL));

    // Choose handled URL randomly
    for (;;) {
      if (turls.size() <= 0)
        break;
      int n = (int)((std::rand() * ((double)(turls.size() - 1))) / RAND_MAX + 0.25);
      std::list<std::string>::iterator i = turls.begin();
      for (; n; ++i, ++n) {}
      if (i == turls.end())
        continue;
      // Avoid redirection to SRM
      logger.msg(DEBUG, "Checking URL returned by SRM: %s", *i);
      if (strncasecmp(i->c_str(), "srm://", 6) == 0) {
        turls.erase(i);
        continue;
      }
      // Try to use this TURL + old options
      URL u(*i);
      {
        std::map<std::string, std::string> options = url.Options();
        if (!options.empty())
          for (std::map<std::string, std::string>::iterator oi = options.begin(); oi != options.end(); oi++)
            u.AddOption((*oi).first, (*oi).second);
      }
      r_handle = new DataHandle(u);
      // check if url can be handled
      if (!r_handle) {
        turls.erase(i);
        continue;
      }
      if ((*r_handle)->ProvidesMeta()) {
        delete r_handle;
        r_handle = NULL;
        turls.erase(i);
        continue;
      }
      break;
    }

    if (r_handle == NULL) {
      logger.msg(INFO, "SRM returned no useful Transfer URLs: %s", url.str());
      delete client;
      client = NULL;
      return DataStatus::WriteError;
    }

    logger.msg(INFO, "Redirecting to new URL: %s", (*r_handle)->CurrentLocation().str());
    if (!(*r_handle)->StartWriting(buf)) {
      if (r_handle) {
        delete r_handle;
        r_handle = NULL;
      }
      if (srm_request) {
        delete srm_request;
        srm_request = NULL;
      }
      if (client) {
        delete client;
        client = NULL;
      }
      reading = false;
      return DataStatus::WriteError;
    }
    delete client;
    client = NULL;
    return DataStatus::Success;
  }

  DataStatus DataPointSRM::StopWriting() {

    if (!writing)
      return DataStatus::WriteStopError;
    writing = false;

    if (!r_handle)
      return DataStatus::Success;

    DataStatus r = (*r_handle)->StopWriting();
    delete r_handle;
    if (srm_request) {
      SRMClient *client = SRMClient::getInstance(url.fullstr());
      if (client) {
        if (buffer->error())
          client->abort(*srm_request);
        else
          client->releasePut(*srm_request);
      }
      delete srm_request;
      if (client) {
        delete client;
        client = NULL;
      }
    }
    r_handle = NULL;
    srm_request = NULL;
    return r;
  }

  DataStatus DataPointSRM::ListFiles(std::list<FileInfo>& files,
                                     bool long_list,
                                     bool resolve,
                                     bool metadata) {

    SRMClient * client = SRMClient::getInstance(url.fullstr());
    if(!client) 
      return DataStatus::CheckError;
    
    std::string canonic_url;
    if (!url.HTTPOption("SFN").empty())
      canonic_url = url.Protocol() + "://" + url.Host() + "/" + url.HTTPOption("SFN");
    else
      canonic_url = url.Protocol() + "://" + url.Host() + "/" + url.Path();

    srm_request = new SRMClientRequest(canonic_url);
    if (!srm_request) {
      delete client;
      client = NULL;
      return DataStatus::ListError;
    }

    logger.msg(DEBUG, "ListFiles: looking for metadata: %s", CurrentLocation().str());
    std::list<struct SRMFileMetaData> srm_metadata;

    // get info from SRM
    if (!client->info(*srm_request, srm_metadata)) {
      delete client;
      client = NULL;
      return DataStatus::ListError;
    }

    if (srm_metadata.empty()) {
      delete client;
      client = NULL;
      return DataStatus::Success;
    }
    // set URL attributes for surl requested (file or dir)
    if(srm_metadata.front().size > 0)
      SetSize(srm_metadata.front().size);
    if(srm_metadata.front().checkSumType.length() > 0 &&
       srm_metadata.front().checkSumValue.length() > 0) {
      std::string csum(srm_metadata.front().checkSumType+":"+srm_metadata.front().checkSumValue);
      SetCheckSum(csum);
    }
    if(srm_metadata.front().createdAtTime > 0) 
      SetCreated(Time(srm_metadata.front().createdAtTime));

    // set FileInfo attributes for surl requested and any files within a dir
    for (std::list<struct SRMFileMetaData>::iterator i = srm_metadata.begin();
         i != srm_metadata.end();
         ++i) {

      std::list<FileInfo>::iterator f =
        files.insert(files.end(), FileInfo((*i).path));
      f->SetMetaData("path", i->path);
      
      if ((*i).fileType == SRM_FILE) {
        f->SetType(FileInfo::file_type_file);
        f->SetMetaData("type", "file");
      }
      else if ((*i).fileType == SRM_DIRECTORY) {
        f->SetType(FileInfo::file_type_dir);
        f->SetMetaData("type", "dir");
      }

      if ((*i).size >= 0) {
        f->SetSize((*i).size);
        f->SetMetaData("size", tostring(i->size));
      }
      if ((*i).createdAtTime > 0) {
        f->SetCreated(Time((*i).createdAtTime));
        f->SetMetaData("ctime", (Time((*i).createdAtTime)).str());
      }
      if ((*i).checkSumType.length() > 0 &&
          (*i).checkSumValue.length() > 0) {
        std::string csum((*i).checkSumType + ":" + (*i).checkSumValue);
        f->SetCheckSum(csum);
        f->SetMetaData("checksum", csum);
      }
      if ((*i).fileLocality == SRM_ONLINE) {
        f->SetLatency("ONLINE");
        f->SetMetaData("latency", "ONLINE");
      }
      else if ((*i).fileLocality == SRM_NEARLINE) {
        f->SetLatency("NEARLINE");
        f->SetMetaData("latency", "NEARLINE");
      }
      if(!(*i).arrayOfSpaceTokens.empty()) f->SetMetaData("spacetokens", (*i).arrayOfSpaceTokens);
      if(!(*i).owner.empty()) f->SetMetaData("owner", (*i).owner);
      if(!(*i).group.empty()) f->SetMetaData("group", (*i).group);
      if(!(*i).permission.empty()) f->SetMetaData("accessperm", (*i).permission);
      if((*i).lastModificationTime > 0)
        f->SetMetaData("mtime", (Time((*i).lastModificationTime)).str());
      if((*i).lifetimeLeft != 0) f->SetMetaData("lifetimeleft", tostring((*i).lifetimeLeft));
      if((*i).lifetimeAssigned != 0) f->SetMetaData("lifetimeassigned", tostring((*i).lifetimeAssigned));
  
      if ((*i).retentionPolicy == SRM_REPLICA) f->SetMetaData("retentionpolicy", "REPLICA");
      else if ((*i).retentionPolicy == SRM_OUTPUT) f->SetMetaData("retentionpolicy", "OUTPUT");
      else if ((*i).retentionPolicy == SRM_CUSTODIAL)  f->SetMetaData("retentionpolicy", "CUSTODIAL");
    }
    delete client;
    client = NULL;
    return DataStatus::Success;
  }

} // namespace Arc
