#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/StringConv.h>

#include "LegacySecAttr.h"
#include "ConfigParser.h"
#include "auth.h"

#include "LegacyPDP.h"

namespace ArcSHCLegacy {

Arc::Plugin* LegacyPDP::get_pdp(Arc::PluginArgument *arg) {
    ArcSec::PDPPluginArgument* pdparg =
            arg?dynamic_cast<ArcSec::PDPPluginArgument*>(arg):NULL;
    if(!pdparg) return NULL;
    return new LegacyPDP((Arc::Config*)(*pdparg),arg);
}

static bool match_lists(const std::list<std::string>& list1, const std::list<std::string>& list2, Arc::Logger& logger) {
  for(std::list<std::string>::const_iterator l1 = list1.begin(); l1 != list1.end(); ++l1) {
    for(std::list<std::string>::const_iterator l2 = list2.begin(); l2 != list2.end(); ++l2) {
      if((*l1) == (*l2)) return true;
    };
  };
  return false;
}

class LegacyPDPCP: public ConfigParser {
 public:
  LegacyPDPCP(LegacyPDP::cfgfile& file, Arc::Logger& logger):ConfigParser(file.filename,logger),file_(file) {
  };

  virtual ~LegacyPDPCP(void) {
  };

 protected:
  virtual bool BlockStart(const std::string& id, const std::string& name) {
    std::string bname = id;
    if(!name.empty()) bname = bname+"/"+name;
    for(std::list<LegacyPDP::cfgblock>::iterator block = file_.blocks.begin();
                                 block != file_.blocks.end();++block) {
      if(block->name == bname) {
        block->exists = true;
      };
    };
    return true;
  };

  virtual bool BlockEnd(const std::string& id, const std::string& name) {
    return true;
  };

  virtual bool ConfigLine(const std::string& id, const std::string& name, const std::string& cmd, const std::string& line) {
    //if(group_matched_) return true;
    if(cmd != "groupcfg") return true;
    std::string bname = id;
    if(!name.empty()) bname = bname+"/"+name;
    for(std::list<LegacyPDP::cfgblock>::iterator block = file_.blocks.begin();
                                 block != file_.blocks.end();++block) {
      if(block->name == bname) {
        block->limited = true;
        std::list<std::string> groups;
        Arc::tokenize(line,groups," \t","\"","\"");
        block->groups.insert(block->groups.end(),groups.begin(),groups.end());
      };
    };
    return true;
  };

 private:
  LegacyPDP::cfgfile& file_;
};

LegacyPDP::LegacyPDP(Arc::Config* cfg,Arc::PluginArgument* parg):PDP(cfg,parg) {
  any_ = false;
  Arc::XMLNode group = (*cfg)["Group"];
  while((bool)group) {
    groups_.push_back((std::string)group);
    ++group;
  };
  Arc::XMLNode vo = (*cfg)["VO"];
  while((bool)vo) {
    vos_.push_back((std::string)vo);
    ++vo;
  };
  Arc::XMLNode block = (*cfg)["ConfigBlock"];
  while((bool)block) {
    std::string filename = (std::string)(block["ConfigFile"]);
    if(filename.empty()) {
      logger.msg(Arc::ERROR, "Configuration file not specified in ConfigBlock");
      //blocks_.clear();
      return;
    };
    cfgfile file(filename);
    Arc::XMLNode name = block["BlockName"];
    while((bool)name) {
      std::string blockname = (std::string)name;
      if(blockname.empty()) {
        logger.msg(Arc::ERROR, "BlockName is empty");
        //blocks_.clear();
        return;
      };
      //file.blocknames.push_back(blockname);
      file.blocks.push_back(blockname);
      ++name;
    };
    LegacyPDPCP parser(file,logger);
    if((!parser) || (!parser.Parse())) {
      logger.msg(Arc::ERROR, "Failed to parse configuration file %s",filename);
      return;
    };
    for(std::list<cfgblock>::const_iterator b = file.blocks.begin();
                         b != file.blocks.end(); ++b) {
      if(!(b->exists)) {
        logger.msg(Arc::ERROR, "Block %s not found in configuration file %s",b->name,filename);
        return;
      };
      if(!(b->limited)) {
        any_ = true;
      } else {
        groups_.insert(groups_.end(),b->groups.begin(),b->groups.end());
      };
    };
    //blocks_.push_back(file);
    ++block;
  };
}

LegacyPDP::~LegacyPDP() {
}

class LegacyPDPAttr: public Arc::SecAttr {
 public:
  LegacyPDPAttr(bool decision):decision_(decision) { };
  virtual ~LegacyPDPAttr(void);

  // Common interface
  virtual operator bool(void) const;
  virtual bool Export(Arc::SecAttrFormat format,Arc::XMLNode &val) const;
  virtual std::string get(const std::string& id) const;
  virtual std::list<std::string> getAll(const std::string& id) const;

  // Specific interface
  bool GetDecision(void) const { return decision_; };

 protected:
  bool decision_;
  virtual bool equal(const SecAttr &b) const;
};

LegacyPDPAttr::~LegacyPDPAttr(void) {
}

LegacyPDPAttr::operator bool(void) const {
  return true;
}

bool LegacyPDPAttr::Export(Arc::SecAttrFormat format,Arc::XMLNode &val) const {
  return true;
}

std::string LegacyPDPAttr::get(const std::string& id) const {
  return "";
}

std::list<std::string> LegacyPDPAttr::getAll(const std::string& id) const {
  return std::list<std::string>();
}

bool LegacyPDPAttr::equal(const SecAttr &b) const {
  const LegacyPDPAttr& a = dynamic_cast<const LegacyPDPAttr&>(b);
  if (!a) return false;
  return (decision_ == a.decision_);
}

ArcSec::PDPStatus LegacyPDP::isPermitted(Arc::Message *msg) const {
  if(any_) return true; // No need to perform anything if everyone is allowed
  Arc::SecAttr* sattr = msg->Auth()->get("ARCLEGACY");
  if(!sattr) {
    // Only if information collection is done per context.
    // Check if decision is already made.
    Arc::SecAttr* dattr = msg->AuthContext()->get("ARCLEGACYPDP");
    if(dattr) {
      LegacyPDPAttr* pattr = dynamic_cast<LegacyPDPAttr*>(dattr);
      if(pattr) {
        // Decision is already made in this context
        return pattr->GetDecision();
      };
    };
  };
  if(!sattr) sattr = msg->AuthContext()->get("ARCLEGACY");
  if(!sattr) {
    logger.msg(Arc::ERROR, "LegacyPDP: there is no ARCLEGACY Sec Attribute defined. Probably ARC Legacy Sec Handler is not configured or failed.");
    return false;
  };
  LegacySecAttr* lattr = dynamic_cast<LegacySecAttr*>(sattr);
  if(!lattr) {
    logger.msg(Arc::ERROR, "LegacyPDP: ARC Legacy Sec Attribute not recognized.");
    return false;
  };
  const std::list<std::string>& groups(lattr->GetGroups());
  const std::list<std::string>& vos(lattr->GetVOs());
  bool decision = false;







  if(match_lists(groups_,groups,logger) ||
     match_lists(vos_,vos,logger)) decision = true;
  msg->AuthContext()->set("ARCLEGACYPDP",new LegacyPDPAttr(decision));
  return decision;
}


} // namespace ArcSHCLegacy

