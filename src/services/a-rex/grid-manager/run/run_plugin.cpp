#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <dlfcn.h>
#endif

#include <arc/Run.h>

#include "../conf/environment.h"
#include "../conf/conf.h"
#include "run_plugin.h"

void free_args(char** args) {
  if(args == NULL) return;
  for(int i=0;args[i];i++) free(args[i]);
  free(args);
}

char** string_to_args(const std::string& command) {
  if(command.length() == 0) return NULL;
  int n = 100;
  char** args = (char**)malloc(n*sizeof(char**));
  int i;
  for(i=0;i<n;i++) args[i]=NULL;
  if(args == NULL) return NULL;
  std::string args_s = command;
  std::string arg_s;
  for(i=0;;i++) {
    if(i==(n-1)) {
      n+=10;
      char** args_ = (char**)realloc(args,n*sizeof(char**));
      if(args_ == NULL) {
        free_args(args);
        return NULL;
      };
      args=args_; for(int i_ = i;i_<n;i_++) args_[i_]=NULL;
    };
    arg_s=config_next_arg(args_s);
    if(arg_s.length() == 0) break;
    args[i]=strdup(arg_s.c_str());
    if(args[i] == NULL) {
      free_args(args);
      return NULL;
    };
  };
  return args;
}


void RunPlugin::set(const std::string& cmd) {
  args_.resize(0); lib="";
  char** args = string_to_args(cmd);
  if(args == NULL) return;
  for(char** arg = args;*arg;arg++) {
    args_.push_back(std::string(*arg));
  };
  free_args(args);
  if(args_.size() == 0) return;
  std::string& exc = *(args_.begin());
  if(exc[0] == '/') return;
  std::string::size_type n = exc.find('@');
  if(n == std::string::npos) return;
  std::string::size_type p = exc.find('/');
  if((p != std::string::npos) && (p < n)) return;
  lib=exc.substr(n+1); exc.resize(n);
  if(lib[0] != '/') lib="./"+lib;
}

void RunPlugin::set(char const * const * args) {
  args_.resize(0); lib="";
  if(args == NULL) return;
  for(char const * const * arg = args;*arg;arg++) {
    args_.push_back(std::string(*arg));
  };
  if(args_.size() == 0) return;
  std::string& exc = *(args_.begin());
  if(exc[0] == '/') return;
  std::string::size_type n = exc.find('@');
  if(n == std::string::npos) return;
  std::string::size_type p = exc.find('/');
  if((p != std::string::npos) && (p < n)) return;
  lib=exc.substr(n+1); exc.resize(n);
  if(lib[0] != '/') lib="./"+lib;
}

bool RunPlugin::run(void) {
  if(args_.size() == 0) return true;
  char** args = (char**)malloc(sizeof(char*)*(args_.size()+1));
  if(args == NULL) return false;
  int n = 0;
  for(std::list<std::string>::iterator i = args_.begin();
              i!=args_.end();++i,++n) args[n]=(char*)(i->c_str());
  args[n]=NULL;
  if(lib.length() == 0) {
    bool r = false;
    Arc::Run re(args_);
    re.AssignStdin(stdin_);
    re.AssignStdout(stdout_);
    re.AssignStderr(stderr_);
    if(re.Start()) {
      if(re.Wait(timeout_)) {
        result_=re.Result();
        r=true;
      } else {
        re.Kill(0);
      };
    };
    if(!r) {
      free(args);
      return false;
    };
  } else {
#ifndef WIN32
    void* lib_h = dlopen(lib.c_str(),RTLD_NOW);
    if(lib_h == NULL) { free(args); return false; };
    lib_plugin_t f;
    f.v = dlsym(lib_h,args[0]);
    if(f.v == NULL) { dlclose(lib_h); free(args); return false; };
    result_ = (*f.f)(args[1],args[2],args[3],args[4],args[5],
                   args[6],args[7],args[8],args[9],args[10],
                   args[11],args[12],args[13],args[14],args[15],
                   args[16],args[17],args[18],args[19],args[20],
                   args[21],args[22],args[23],args[24],args[25],
                   args[26],args[27],args[28],args[29],args[30],
                   args[31],args[32],args[33],args[34],args[35],
                   args[36],args[37],args[38],args[39],args[40],
                   args[41],args[42],args[43],args[44],args[45],
                   args[56],args[57],args[58],args[59],args[60],
                   args[61],args[62],args[63],args[64],args[65],
                   args[66],args[67],args[68],args[69],args[70],
                   args[71],args[72],args[73],args[74],args[75],
                   args[76],args[77],args[78],args[79],args[80],
                   args[81],args[82],args[83],args[84],args[85],
                   args[86],args[87],args[88],args[89],args[90],
                   args[91],args[92],args[93],args[94],args[95],
                   args[96],args[97],args[98],args[99],args[100]);
    dlclose(lib_h);
#else
#warning Implement calling function from library for Windows
    result=-1;
#endif
  };
  free(args);
  return true;
}

bool RunPlugin::run(substitute_t subst,void* arg) {
  result_=0; stdout_=""; stderr_="";
  if(subst == NULL) return run();
  if(args_.size() == 0) return true;
  char** args = (char**)malloc(sizeof(char*)*(args_.size()+1));
  if(args == NULL) return false;
  std::list<std::string> args__;
  for(std::list<std::string>::iterator i = args_.begin();i!=args_.end();++i) {
    args__.push_back(*i);
  };
  for(std::list<std::string>::iterator i = args__.begin();i!=args__.end();++i) {
    (*subst)(*i,arg);
  };
  int n = 0;
  for(std::list<std::string>::iterator i = args__.begin();
              i!=args__.end();++i,++n) args[n]=(char*)(i->c_str());
  args[n]=NULL;
  if(lib.length() == 0) {
    bool r = false;
    Arc::Run re(args_);
    re.AssignStdin(stdin_);
    re.AssignStdout(stdout_);
    re.AssignStderr(stderr_);
    if(re.Start()) {
      if(re.Wait(timeout_)) {
        result_=re.Result();
        r=true;
      } else {
        re.Kill(0);
      };
    };
    if(!r) {
      free(args);
      return false;
    };
  } else {
#ifndef WIN32
    void* lib_h = dlopen(lib.c_str(),RTLD_NOW);
    if(lib_h == NULL) { 
      free(args); return false;
    };
    lib_plugin_t f;
    f.v = dlsym(lib_h,args[0]);
    if(f.v == NULL) { 
      dlclose(lib_h); free(args); return false;
    };
    result_ = (*f.f)(args[1],args[2],args[3],args[4],args[5],
                   args[6],args[7],args[8],args[9],args[10],
                   args[11],args[12],args[13],args[14],args[15],
                   args[16],args[17],args[18],args[19],args[20],
                   args[21],args[22],args[23],args[24],args[25],
                   args[26],args[27],args[28],args[29],args[30],
                   args[31],args[32],args[33],args[34],args[35],
                   args[36],args[37],args[38],args[39],args[40],
                   args[41],args[42],args[43],args[44],args[45],
                   args[56],args[57],args[58],args[59],args[60],
                   args[61],args[62],args[63],args[64],args[65],
                   args[66],args[67],args[68],args[69],args[70],
                   args[71],args[72],args[73],args[74],args[75],
                   args[76],args[77],args[78],args[79],args[80],
                   args[81],args[82],args[83],args[84],args[85],
                   args[86],args[87],args[88],args[89],args[90],
                   args[91],args[92],args[93],args[94],args[95],
                   args[96],args[97],args[98],args[99],args[100]);
    dlclose(lib_h);
#else
#warning Implement calling function from library for Windows
    result=-1;
#endif
  };
  free(args);
  return true;
}

void RunPlugins::add(const std::string& cmd) {
  RunPlugin* r = new RunPlugin(cmd);
  if(!r) return;
  if(!(*r)) return;
  plugins_.push_back(r);
}

bool RunPlugins::run(void) {
  for(std::list<RunPlugin*>::iterator r = plugins_.begin();
                                  r != plugins_.end();++r) {
    if(!(*r)->run()) return false;
    if((*r)->result() != 0) {
      result_=result(); return true;
    };
  };
  result_=0;
  return true;
}

bool RunPlugins::run(RunPlugin::substitute_t subst,void* arg) {
  for(std::list<RunPlugin*>::iterator r = plugins_.begin();
                                  r != plugins_.end();++r) {
    if(!(*r)->run(subst,arg)) return false;
    if((*r)->result() != 0) {
      result_=result(); return true;
    };
  };
  result_=0;
  return true;
}

