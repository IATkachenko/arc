#include <string>
#include <list>

#include <arc/XMLNode.h>

class GMEnvironment;
class JobUsers;
class DTRGenerator;

namespace ARex {

class GridManager {
 private:
  bool active_;
  GMEnvironment* env_;
  JobUsers* users_;
  DTRGenerator* dtr_generator_;
  GridManager(void) { };
  GridManager(const GridManager&) { };
  static void grid_manager(void* arg);
 public:
  //GridManager(Arc::XMLNode argv);
  GridManager(GMEnvironment& env);
  ~GridManager(void);
  operator bool(void) { return active_; };
  JobUsers* Users(void) { return users_; };
  GMEnvironment* Environment(void) { return env_; };
};

}

