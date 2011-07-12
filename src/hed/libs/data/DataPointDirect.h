// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_DATAPOINTDIRECT_H__
#define __ARC_DATAPOINTDIRECT_H__

#include <list>
#include <string>

#include <arc/data/DataPoint.h>

#define MAX_PARALLEL_STREAMS 20
#define MAX_BLOCK_SIZE (1024 * 1024)

namespace Arc {

  class DataBuffer;
  class DataCallback;

  /// This is a kind of generalized file handle.
  /** Differently from file handle it does not support operations
      read() and write(). Instead it initiates operation and uses
      object of class DataBuffer to pass actual data. It also
      provides other operations like querying parameters of remote
      object. It is used by higher-level classes DataMove and
      DataMovePar to provide data transfer service for application. */

  class DataPointDirect
    : public DataPoint {
  public:
    virtual ~DataPointDirect();

    virtual bool IsIndex() const;
    virtual bool IsStageable() const;

    virtual long long int BufSize() const;
    virtual int BufNum() const;

    virtual bool Local() const;
    virtual bool ReadOnly() const;

    virtual void ReadOutOfOrder(bool v);
    virtual bool WriteOutOfOrder();

    virtual void SetAdditionalChecks(bool v);
    virtual bool GetAdditionalChecks() const;

    virtual void SetSecure(bool v);
    virtual bool GetSecure() const;

    virtual void Passive(bool v);

    virtual void Range(unsigned long long int start = 0,
                       unsigned long long int end = 0);

    virtual int AddCheckSumObject(CheckSum *cksum);

    virtual const CheckSum* GetCheckSumObject(int index) const;

    // Not supported for direct data points:
    virtual DataStatus Resolve(bool source);
    virtual bool Registered() const;
    virtual DataStatus PreRegister(bool replication, bool force = false);
    virtual DataStatus PostRegister(bool replication);
    virtual DataStatus PreUnregister(bool replication);
    virtual DataStatus Unregister(bool all);
    virtual bool AcceptsMeta() const;
    virtual bool ProvidesMeta() const;
    virtual const URL& CurrentLocation() const;
    virtual const std::string& CurrentLocationMetadata() const;
    virtual DataStatus CompareLocationMetadata() const;
    virtual bool NextLocation();
    virtual bool LocationValid() const;
    virtual bool HaveLocations() const;
    virtual bool LastLocation();
    virtual DataStatus AddLocation(const URL& url, const std::string& meta);
    virtual DataStatus RemoveLocation();
    virtual DataStatus RemoveLocations(const DataPoint& p);
    virtual DataStatus ClearLocations();
    virtual void SortLocations(const std::string& /* pattern */,
                               const URLMap& /* url_map */) {};

  protected:
    DataBuffer *buffer;
    long long int bufsize;
    int bufnum;
    bool local;
    bool readonly;
    bool linkable;
    bool is_secure;
    bool force_secure;
    bool force_passive;
    bool additional_checks;
    bool allow_out_of_order;
    unsigned long long int range_start;
    unsigned long long int range_end;
    std::list<CheckSum*> checksums;
    DataPointDirect(const URL& url, const UserConfig& usercfg);
  };

} // namespace Arc

#endif // __ARC_DATAPOINTDIRECT_H__
