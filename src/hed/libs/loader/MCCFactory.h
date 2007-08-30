#ifndef __ARC_MCCFACTORY_H__
#define __ARC_MCCFACTORY_H__

#include "LoaderFactory.h"
#include "MCCLoader.h"

namespace Arc {

  /// MCC Plugins handler
  /** This class handles shared libraries containing MCCs */
  class MCCFactory : public LoaderFactory {
   public:
    /** Constructor - accepts configuration (not yet used) meant to
	tune loading of module. */
    MCCFactory(Config *cfg);
    ~MCCFactory();
    /** These methods load shared library named lib'name', locate symbol
	representing descriptor of MCC and calls it's constructor
	function. Supplied configuration tree is passed to constructor.
	Returns created MCC instance. */
    MCC* get_instance(const std::string& name,
		      Config *cfg, ChainContext *ctx);
    MCC* get_instance(const std::string& name, int version,
		      Config *cfg, ChainContext *ctx);
    MCC* get_instance(const std::string& name,
		      int min_version, int max_version,
		      Config *cfg, ChainContext *ctx);
  };

} // namespace Arc

#endif /* __ARC_MCCFACTORY_H__ */
