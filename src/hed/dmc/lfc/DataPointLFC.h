// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_DATAPOINTLFC_H__
#define __ARC_DATAPOINTLFC_H__

#include <list>

#include <arc/data/DataPointIndex.h>

namespace Arc {
  class Logger;
  class URL;

  /**
   * The LCG File Catalog (LFC) is a replica catalog developed by CERN. It
   * consists of a hierarchical namespace of grid files and each filename
   * can be associated with one or more physical locations.
   *
   * This class is a loadable module and cannot be used directly. The DataHandle
   * class loads modules at runtime and should be used instead of this.
   */
  class DataPointLFC
    : public DataPointIndex {
  public:
    DataPointLFC(const URL& url, const UserConfig& usercfg);
    ~DataPointLFC();
    static Plugin* Instance(PluginArgument *arg);
    virtual DataStatus Resolve(bool source);
    virtual DataStatus Check();
    virtual DataStatus PreRegister(bool replication, bool force = false);
    virtual DataStatus PostRegister(bool replication);
    virtual DataStatus PreUnregister(bool replication);
    virtual DataStatus Unregister(bool all);
    virtual DataStatus Stat(FileInfo& file, DataPointInfoType verb = INFO_TYPE_ALL);
    virtual DataStatus List(std::list<FileInfo>& files, DataPointInfoType verb = INFO_TYPE_ALL);
    virtual DataStatus CreateDirectory(bool with_parents=false);
    //virtual DataStatus ListFiles(std::list<FileInfo>& files, bool long_list = false, bool resolve = false, bool metadata = false);
    virtual const std::string DefaultCheckSum() const;
    virtual std::string str() const;
  protected:
    static Logger logger;
    std::string guid;
    std::string path_for_guid;
  private:
    std::string ResolveGUIDToLFN();
    /// Returns true if serrno is a temporary error and can potentially be retried
    bool IsTempError() const;
    DataStatus ListFiles(std::list<FileInfo>& files, DataPointInfoType verb, bool listdir);

  };

} // namespace Arc

#endif // __ARC_DATAPOINTLFC_H__
