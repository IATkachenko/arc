// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_FILEINFO_H__
#define __ARC_FILEINFO_H__

#include <list>
#include <string>

#include <arc/DateTime.h>
#include <arc/URL.h>
#include <arc/StringConv.h>

namespace Arc {

  /// FileInfo stores information about files (metadata).
  class FileInfo {

  public:

    enum Type {
      file_type_unknown = 0,
      file_type_file = 1,
      file_type_dir = 2
    };

    FileInfo(const std::string& name = "")
      : name(name),
        size((unsigned long long int)(-1)),
        modified((time_t)(-1)),
        valid((time_t)(-1)),
        type(file_type_unknown),
        latency("") {}

    ~FileInfo() {}

    const std::string& GetName() const {
      return name;
    }

    std::string GetLastName() const {
      std::string::size_type pos = name.rfind('/');
      if (pos != std::string::npos)
        return name.substr(pos + 1);
      else
        return name;
    }

    void SetName(const std::string& n) {
      name = n;
    }

    const std::list<URL>& GetURLs() const {
      return urls;
    }

    void AddURL(const URL& u) {
      urls.push_back(u);
    }

    bool CheckSize() const {
      return (size != (unsigned long long int)(-1));
    }

    unsigned long long int GetSize() const {
      return size;
    }

    void SetSize(const unsigned long long int s) {
      size = s;
    }

    bool CheckCheckSum() const {
      return (!checksum.empty());
    }

    const std::string& GetCheckSum() const {
      return checksum;
    }

    void SetCheckSum(const std::string& c) {
      checksum = c;
    }

    bool CheckModified() const {
      return (modified != -1);
    }

    Time GetModified() const {
      return modified;
    }

    void SetModified(const Time& t) {
      modified = t;
    }

    bool CheckValid() const {
      return (valid != -1);
    }

    Time GetValid() const {
      return valid;
    }

    void SetValid(const Time& t) {
      valid = t;
    }

    bool CheckType() const {
      return (type != file_type_unknown);
    }

    Type GetType() const {
      return type;
    }

    void SetType(const Type t) {
      type = t;
    }

    bool CheckLatency() const {
      return (!latency.empty());
    }

    std::string GetLatency() const {
      return latency;
    }

    void SetLatency(const std::string l) {
      latency = l;
    }

    std::map<std::string, std::string> GetMetaData() const {
      return metadata;
    }
    
    void SetMetaData(const std::string att, const std::string val) {
      metadata[att] = val;
    }
    
    bool operator<(const FileInfo& f) const {
      return (lower(this->name).compare(lower(f.name)) < 0);
    }

    operator bool() const {
      return !name.empty();
    }

    bool operator!() const {
      return name.empty();
    }

  private:

    std::string name;
    std::list<URL> urls;         // Physical enpoints/URLs.
    unsigned long long int size; // Size of file in bytes.
    std::string checksum;        // Checksum of file.
    Time modified;               // Creation/modification time.
    Time valid;                  // Valid till time.
    Type type;                   // File type - usually file_type_file
    std::string latency;         // Access latenct of file (applies to SRM only)
    std::map<std::string, std::string> metadata; // Generic metadata attribute-value pairs
  };

} // namespace Arc

#endif // __ARC_FILEINFO_H__
