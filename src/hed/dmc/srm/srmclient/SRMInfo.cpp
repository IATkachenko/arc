#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <vector>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef WIN32
#include <arc/win32.h>
#endif

#include <glibmm.h>

#include <arc/FileLock.h>
#include <arc/FileUtils.h>
#include <arc/StringConv.h>
#include <arc/Logger.h>
#include <arc/Utils.h>

#include "SRMInfo.h"

Arc::Logger SRMInfo::logger(Arc::Logger::getRootLogger(), "SRMInfo");

bool SRMFileInfo::operator ==(SRMURL srm_url) {
  if (host == srm_url.Host() &&
      (!srm_url.PortDefined() || port == srm_url.Port()) &&
      version == srm_url.SRMVersion())
    return true;
  return false;
}

std::string SRMFileInfo::versionString() const {
  switch (version) {
    case SRMURL::SRM_URL_VERSION_1: {
      return "1";
    }; break;
    case SRMURL::SRM_URL_VERSION_2_2: {
      return "2.2";
    }; break;
    default: {
      return "";
    }
  }
  return "";
}

SRMInfo::SRMInfo(std::string dir) {
  srm_info_filename = dir + G_DIR_SEPARATOR_S + "srms.conf";
}

bool SRMInfo::getSRMFileInfo(SRMFileInfo& srm_file_info) {
  
  std::list<std::string> filedata;
  Arc::FileLock filelock(srm_info_filename);
  bool acquired = false;
  for (int tries = 10; (tries > 0 && !acquired); --tries) {
    acquired = filelock.acquire();
    usleep(500000);
  }
  if (!acquired) {
    logger.msg(Arc::WARNING, "Failed to acquire lock on file %s", srm_info_filename);
    return false;
  }
  if (!Arc::FileRead(srm_info_filename, filedata)) {
    if (errno != ENOENT)
      logger.msg(Arc::WARNING, "Error reading info from file %s:%s", srm_info_filename, Arc::StrError(errno));
    filelock.release();
    return false;
  }

  for (std::list<std::string>::iterator line = filedata.begin(); line != filedata.end(); ++line) {
    if (line->empty() || (*line)[0] == '#')
      continue;
    // split line
    std::vector<std::string> fields;
    Arc::tokenize(*line, fields);
    if (fields.size() != 3) {
      logger.msg(Arc::WARNING, "Bad or old format detected in file %s, in line %s", srm_info_filename, *line);
      continue;
    }
    // look for our combination of host and version
    if (fields.at(0) == srm_file_info.host && fields.at(2) == srm_file_info.versionString()) {
      int port_i = Arc::stringtoi(fields.at(1));
      if (port_i == 0) {
        logger.msg(Arc::WARNING, "Can't convert string %s to int in file %s, line %s", fields.at(1), srm_info_filename, *line);
        continue;
      }        
      srm_file_info.port = port_i;
      filelock.release();
      return true;
    }
  }
  filelock.release();
  return false;
}

void SRMInfo::putSRMFileInfo(const SRMFileInfo& srm_file_info) {
   
  std::string header("# This file was automatically generated by ARC for caching SRM information.\n");
  header += "# Its format is lines with 3 entries separated by spaces:\n";
  header += "# hostname port version\n#\n";
  header += "# This file can be freely edited, but it is not advisable while there\n";
  header += "# are on-going transfers. Comments begin with #\n#";

  std::list<std::string> filedata;
  Arc::FileLock filelock(srm_info_filename);
  bool acquired = false;
  for (int tries = 10; (tries > 0 && !acquired); --tries) {
    acquired = filelock.acquire();
    usleep(500000);
  }
  if (!acquired) {
    logger.msg(Arc::WARNING, "Failed to acquire lock on file %s", srm_info_filename);
    return;
  }
  if (!Arc::FileRead(srm_info_filename, filedata)) {
    // write new file
    filedata.push_back(header);
  }

  std::string lines;
  for (std::list<std::string>::iterator line = filedata.begin(); line != filedata.end(); ++line) {
    if (line->empty()) continue;
    if ((*line)[0] == '#') {
      // check for old-style file - if so re-write whole file
      if (line->find("# Its format is lines with 4 entries separated by spaces:") == 0) {
        lines = header+'\n';
        break;
      }
      lines += *line+'\n';
      continue;
    }
    // split line
    std::vector<std::string> fields;
    Arc::tokenize(*line, fields);
    if (fields.size() != 3) {
      if (line->length() > 0) {
        logger.msg(Arc::WARNING, "Bad or old format detected in file %s, in line %s", srm_info_filename, *line);
      }
      continue;
    }
    // if any line contains our combination of host and version, ignore it
    if (fields.at(0) == srm_file_info.host &&
        fields.at(2) == srm_file_info.versionString()) {
      continue;
    }
    lines += *line+'\n';
  }
  // add new info
  lines += srm_file_info.host + ' ' + Arc::tostring(srm_file_info.port) + ' ' + srm_file_info.versionString() + '\n';

  // write everything back to the file
  if (!Arc::FileCreate(srm_info_filename, lines)) {
    logger.msg(Arc::WARNING, "Error writing srm info file %s", srm_info_filename);
  };
  filelock.release();
}

