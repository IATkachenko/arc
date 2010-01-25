#ifndef __ARC_SEC_SIMPLEPDP_H__
#define __ARC_SEC_SIMPLEPDP_H__

#include <stdlib.h>

#include <arc/ArcConfig.h>
#include <arc/security/PDP.h>

namespace ArcSec {

/// Tests X509 subject against list of subjects in file
/** This class implements PDP interface. It's isPermitted() method
  compares X590 subject of requestor obtained from TLS layer (TLS:PEERDN)
  to list of subjects (ne per line) in external file. Locations of 
  file is defined by 'location' attribute of PDP caonfiguration.
  Returns true if subject is present in list, otherwise false. */
class SimpleListPDP : public PDP {
 public:
  static Arc::Plugin* get_simplelist_pdp(Arc::PluginArgument *arg);
  SimpleListPDP(Arc::Config* cfg);
  virtual ~SimpleListPDP() {};
  virtual bool isPermitted(Arc::Message *msg) const;
 private:
  std::string location;
  std::list<std::string> dns;
 protected:
  static Arc::Logger logger;
};

} // namespace ArcSec

#endif /* __ARC_SEC_SIMPLELISTPDP_H__ */
