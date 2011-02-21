#ifndef ARC_GRIDFTPD_RUN_PLUGIN_H
#define ARC_GRIDFTPD_RUN_PLUGIN_H

#include <sys/resource.h>
#include <sys/wait.h>
#include <string>
#include <list>
#include <pthread.h>

extern char** environ;

namespace gridftpd {

  class RunPlugin {
   private:
    std::list<std::string> args_;
    std::string lib;
    std::string stdin_;
    std::string stdout_;
    std::string stderr_;
    int timeout_;
    int result_;
    void set(const std::string& cmd);
    void set(char const * const * args);
   public:
    typedef void (*substitute_t)(std::string& str,void* arg);
    union lib_plugin_t {
      int (*f)(...);
      void* v;
    };
    RunPlugin(void):timeout_(10),result_(0) { };
    RunPlugin(const std::string& cmd):timeout_(10),result_(0) { set(cmd); };
    RunPlugin(char const * const * args):timeout_(10),result_(0) { set(args); };
    void operator=(const std::string& cmd) { set(cmd); };
    void operator=(char const * const * args) { set(args); };
    bool run(void);
    bool run(substitute_t subst,void* arg);
    int result(void) const { return result_; };
    void timeout(int t) { timeout_=t; };
    void stdin_channel(const std::string& s) { stdin_=s; };
    const std::string& stdout_channel(void) const { return stdout_; };
    const std::string& stderr_channel(void) const { return stderr_; };
    operator bool(void) const { return (args_.size() > 0); };
  };


  void free_args(char** args);
  char** string_to_args(const std::string& command);

} // namespace gridftpd

#endif // ARC_GRIDFTPD_RUN_PLUGIN_H
