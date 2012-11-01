#ifndef __ARC_MESSAGEAUTH_H__
#define __ARC_MESSAGEAUTH_H__

#include <string>
#include <map>

#include <arc/message/SecAttr.h>

namespace Arc {

/// Contains authencity information, authorization tokens and decisions.
/** This class only supports string keys and SecAttr values. */
class MessageAuth {
  private:
    std::map<std::string,SecAttr*> attrs_;
    bool attrs_created_;
    MessageAuth(const MessageAuth&);
  public:
    MessageAuth(void);
    ~MessageAuth(void);
    /// Adds/overwrites security attribute stored under specified key
    void set(const std::string& key, SecAttr* value);
    /// Deletes security attribute stored under specified key
    void remove(const std::string& key);
    /// Retrieves reference to security attribute stored under specified key
    SecAttr* get(const std::string& key);
    /// Same as MessageAuth::get
    SecAttr* operator[](const std::string& key) { return get(key); };
    /// Returns properly catenated attributes in specified format
    /** Content of XML node at @val is replaced with generated information
       if XML tree is empty. If tree at @val is not empty then Export() tries
       to merge generated information to already existing like everything would
       be generated inside same Export() method. If @val does not
       represent valid node then new XML tree is created. */
    bool Export(SecAttrFormat format,XMLNode &val) const;
    /// Creates new instance of MessageAuth with attributes filtered
    /** In new instance all attributes with keys listed in @rejected_keys are
      removed. If @selected_keys is not empty only corresponding attributes
      are transferred to new instance. Created instance does not own refered
      attributes. Hence parent instance must not be deleted as long as
      this one is in use. */
    MessageAuth* Filter(const std::list<std::string>& selected_keys,const std::list<std::string>& rejected_keys);
};

}

#endif

