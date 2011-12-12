#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*
 routines to work with informational files
*/

#include <string>
#include <list>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <limits>

#include <arc/StringConv.h>
#include <arc/DateTime.h>
#include <arc/Thread.h>
#include "../files/delete.h"
#include "../misc/escaped.h"

#include "../run/run_function.h"
#include "../run/run_redirected.h"

#include "../conf/conf.h"
//@ #include <arc/certificate.h>
#include "info_types.h"
#include "info_files.h"

#if defined __GNUC__ && __GNUC__ >= 3

#define istream_readline(__f,__s,__n) {      \
   __f.get(__s,__n,__f.widen('\n'));         \
   if(__f.fail()) __f.clear();               \
   __f.ignore(std::numeric_limits<std::streamsize>::max(), __f.widen('\n')); \
}

#else

#define istream_readline(__f,__s,__n) {      \
   __f.get(__s,__n,'\n');         \
   if(__f.fail()) __f.clear();               \
   __f.ignore(INT_MAX,'\n'); \
}

#endif


const char * const sfx_failed      = ".failed";
const char * const sfx_cancel      = ".cancel";
const char * const sfx_restart     = ".restart";
const char * const sfx_clean       = ".clean";
const char * const sfx_status      = ".status";
const char * const sfx_local       = ".local";
const char * const sfx_errors      = ".errors";
const char * const sfx_rsl         = ".description";
const char * const sfx_diag        = ".diag";
const char * const sfx_lrmsoutput  = ".comment";
const char * const sfx_diskusage   = ".disk";
const char * const sfx_acl         = ".acl";
const char * const sfx_proxy       = ".proxy";
const char * const sfx_xml         = ".xml";
const char * const sfx_input       = ".input";
const char * const sfx_output      = ".output";
const char * const sfx_inputstatus = ".input_status";
const char * const sfx_outputstatus = ".output_status";
const char * const subdir_new      = "accepting";
const char * const subdir_cur      = "processing";
const char * const subdir_old      = "finished";
const char * const subdir_rew      = "restarting";

static Arc::Logger& logger = Arc::Logger::getRootLogger();

static job_state_t job_state_read_file(const std::string &fname,bool &pending);
static bool job_state_write_file(const std::string &fname,job_state_t state,bool pending = false);
// static bool job_Xput_read_file(std::list<FileData> &files);
static bool job_strings_write_file(const std::string &fname,std::list<std::string> &str);


bool fix_file_permissions(const std::string &fname,bool executable) {
  mode_t mode = S_IRUSR | S_IWUSR;
  if(executable) { mode |= S_IXUSR; };
  return (chmod(fname.c_str(),mode) == 0);
}

bool fix_file_permissions(const std::string &fname,const JobDescription &desc,const JobUser &user) {
  mode_t mode = S_IRUSR | S_IWUSR;
  uid_t uid = desc.get_uid();
  gid_t gid = desc.get_gid();
  if( uid == 0 ) {
    uid=user.get_uid(); gid=user.get_gid();
  };
  if(!user.match_share_uid(uid)) {
    mode |= S_IRGRP;
    if(!user.match_share_gid(gid)) {
      mode |= S_IROTH;
    };
  };
  return (chmod(fname.c_str(),mode) == 0);
}

bool fix_file_owner(const std::string &fname,const JobUser &user) {
  if(getuid() == 0) {
    if(lchown(fname.c_str(),user.get_uid(),user.get_gid()) == -1) {
      logger.msg(Arc::ERROR,"Failed setting file owner: %s",fname);
      return false;
    };
  };
  return true;
}

bool fix_file_owner(const std::string &fname,const JobDescription &desc,const JobUser &user) {
  if(getuid() == 0) {
    uid_t uid = desc.get_uid();
    gid_t gid = desc.get_gid();
    if( uid == 0 ) {
      uid=user.get_uid(); gid=user.get_gid();
    };
    if(lchown(fname.c_str(),uid,gid) == -1) {
      logger.msg(Arc::ERROR,"Failed setting file owner: %s",fname);
      return false;
    };
  };
  return true;
}

bool check_file_owner(const std::string &fname,const JobUser &user) {
  uid_t uid;
  gid_t gid;
  time_t t;
  return check_file_owner(fname,user,uid,gid,t);
}

bool check_file_owner(const std::string &fname,const JobUser &user,uid_t &uid,gid_t &gid) {
  time_t t;
  return check_file_owner(fname,user,uid,gid,t);
}

bool check_file_owner(const std::string &fname,const JobUser &user,uid_t &uid,gid_t &gid,time_t &t) {
  struct stat st;
  if(lstat(fname.c_str(),&st) != 0) return false;
  if(!S_ISREG(st.st_mode)) return false;
  uid=st.st_uid; gid=st.st_gid; t=st.st_ctime;
  /* superuser can't run jobs */
  if(uid == 0) return false;
  /* accept any file if superuser */
  if(user.get_uid() != 0) {
    if(uid != user.get_uid()) return false;
  };
  return true;
}

bool job_lrms_mark_put(const JobDescription &desc,const JobUser &user,int code) {
  LRMSResult r(code);
  return job_lrms_mark_put(desc,user,r);
}

bool job_lrms_mark_put(const JobDescription &desc,const JobUser &user,const LRMSResult &r) {
  std::string fname = user.ControlDir()+"/job."+desc.get_id()+".lrms_done";
  std::string content = Arc::tostring(r.code());
  content+=" ";
  content+=r.description();
  return job_mark_write_s(fname,content) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}

bool job_lrms_mark_check(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/job." + id + ".lrms_done";
  return job_mark_check(fname);
}

bool job_lrms_mark_remove(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/job." + id + ".lrms_done";
  return job_mark_remove(fname);
}

LRMSResult job_lrms_mark_read(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/job." + id + ".lrms_done";
  LRMSResult r("-1 Internal error");
  std::ifstream f(fname.c_str()); if(! f.is_open() ) return r;
  f>>r;
  return r;
}

bool job_cancel_mark_put(const JobDescription &desc,const JobUser &user) {
  std::string fname = user.ControlDir() + "/" + subdir_new + "/job." + desc.get_id() + sfx_cancel;
  return job_mark_put(fname) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}

bool job_cancel_mark_check(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/" + subdir_new + "/job." + id + sfx_cancel;
  return job_mark_check(fname);
}

bool job_cancel_mark_remove(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/" + subdir_new + "/job." + id + sfx_cancel;
  return job_mark_remove(fname);
}

bool job_restart_mark_put(const JobDescription &desc,const JobUser &user) {
  std::string fname = user.ControlDir() + "/" + subdir_new + "/job." + desc.get_id() + sfx_restart;
  return job_mark_put(fname) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}

bool job_restart_mark_check(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/" + subdir_new + "/job." + id + sfx_restart;
  return job_mark_check(fname);
}

bool job_restart_mark_remove(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/" + subdir_new + "/job." + id + sfx_restart;
  return job_mark_remove(fname);
}

bool job_clean_mark_put(const JobDescription &desc,const JobUser &user) {
  std::string fname = user.ControlDir() + "/" + subdir_new + "/job." + desc.get_id() + sfx_clean;
  return job_mark_put(fname) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}

bool job_clean_mark_check(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/" + subdir_new + "/job." + id + sfx_clean;
  return job_mark_check(fname);
}

bool job_clean_mark_remove(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/" + subdir_new + "/job." + id + sfx_clean;
  return job_mark_remove(fname);
}

bool job_errors_mark_put(const JobDescription &desc,const JobUser &user) {
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + sfx_errors;
  return job_mark_put(fname) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}

std::string job_errors_filename(const JobId &id, const JobUser &user) {
  return user.ControlDir() + "/job." + id + sfx_errors;
}

bool job_failed_mark_put(const JobDescription &desc,const JobUser &user,const std::string &content) {
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + sfx_failed;
  if(job_mark_size(fname) > 0) return true;
  return job_mark_write_s(fname,content) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname,desc,user);
}

bool job_failed_mark_add(const JobDescription &desc,const JobUser &user,const std::string &content) {
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + sfx_failed;
  return job_mark_add_s(fname,content) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname,desc,user);
}

bool job_failed_mark_check(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_failed;
  return job_mark_check(fname);
}

bool job_failed_mark_remove(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_failed;
  return job_mark_remove(fname);
}

std::string job_failed_mark_read(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_failed;
  return job_mark_read_s(fname);
}

static int job_mark_put_callback(void* arg) {
  std::string& fname = *((std::string*)arg);
  if(job_mark_put(fname) & fix_file_permissions(fname)) return 0;
  return -1;
}

static int job_mark_remove_callback(void* arg) {
  std::string& fname = *((std::string*)arg);
  if(job_mark_remove(fname)) return 0;
  return -1;
}

static int job_dir_create_callback(void* arg) {
  std::string& dname = *((std::string*)arg);
  if(job_dir_create(dname) & fix_file_permissions(dname,true)) return 0;
  return -1;
}

typedef struct {
  const std::string* fname;
  const std::string* content;
} job_mark_add_t;

static int job_mark_add_callback(void* arg) {
  const std::string& fname = *(((job_mark_add_t*)arg)->fname);
  const std::string& content = *(((job_mark_add_t*)arg)->content);
  if(job_mark_add_s(fname,content) & fix_file_permissions(fname)) return 0;
  return -1;
}

typedef struct {
  const std::string* dname;
  const std::list<FileData>* flist;
} job_dir_remove_t;

static int job_dir_remove_callback(void* arg) {
  const std::string& dname = *(((job_dir_remove_t*)arg)->dname);
  const std::list<FileData>& flist = *(((job_dir_remove_t*)arg)->flist);
  delete_all_files(dname,flist,true);
  remove(dname.c_str());
  return 0;
}

bool job_diagnostics_mark_put(const JobDescription &desc,const JobUser &user) {
  std::string fname = desc.SessionDir() + sfx_diag;
  if(user.StrictSession()) {
    uid_t uid = user.get_uid()==0?desc.get_uid():user.get_uid();
    uid_t gid = user.get_uid()==0?desc.get_gid():user.get_gid();
    JobUser tmp_user(user.Env(),uid,gid);
    return (RunFunction::run(tmp_user,"job_diagnostics_mark_put",&job_mark_put_callback,&fname,-1) == 0);
  };
  return job_mark_put(fname) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}

bool job_session_create(const JobDescription &desc,const JobUser &user) {
  std::string dname = desc.SessionDir();
  if(user.StrictSession()) {
    uid_t uid = user.get_uid()==0?desc.get_uid():user.get_uid();
    uid_t gid = user.get_uid()==0?desc.get_gid():user.get_gid();
    JobUser tmp_user(user.Env(),uid,gid);
    return (RunFunction::run(tmp_user,"job_session_create",&job_dir_create_callback,&dname,-1) == 0);
  };
  return job_dir_create(dname) & fix_file_owner(dname,desc,user) & fix_file_permissions(dname,true);
}

bool job_controldiag_mark_put(const JobDescription &desc,const JobUser &user,char const * const args[]) {
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + sfx_diag;
  if(!job_mark_put(fname)) return false;
  if(!fix_file_owner(fname,desc,user)) return false;
  if(!fix_file_permissions(fname)) return false;
  if(args == NULL) return true;
  int h = open(fname.c_str(),O_WRONLY);
  if(h == -1) return false;
  int r;
  int t = 10;
  JobUser tmp_user(user.Env(),(uid_t)0,(gid_t)0);
  r=RunRedirected::run(tmp_user,"job_controldiag_mark_put",-1,h,-1,(char**)args,t);
  close(h);
  if(r != 0) return false;
  return true;
}

bool job_lrmsoutput_mark_put(const JobDescription &desc,const JobUser &user) {
  std::string fname = desc.SessionDir() + sfx_lrmsoutput;
  if(user.StrictSession()) {
    uid_t uid = user.get_uid()==0?desc.get_uid():user.get_uid();
    uid_t gid = user.get_uid()==0?desc.get_gid():user.get_gid();
    JobUser tmp_user(user.Env(),uid,gid);
    return (RunFunction::run(tmp_user,"job_lrmsoutput_mark_put",&job_mark_put_callback,&fname,-1) == 0);
  };
  return job_mark_put(fname) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}

bool job_diagnostics_mark_add(const JobDescription &desc,const JobUser &user,const std::string &content) {
  std::string fname = desc.SessionDir() + sfx_diag;
  if(user.StrictSession()) {
    uid_t uid = user.get_uid()==0?desc.get_uid():user.get_uid();
    uid_t gid = user.get_uid()==0?desc.get_gid():user.get_gid();
    JobUser tmp_user(user.Env(),uid,gid);
    job_mark_add_t arg; arg.fname=&fname; arg.content=&content;
    return (RunFunction::run(tmp_user,"job_diagnostics_mark_add",&job_mark_add_callback,&arg,-1) == 0);
  };
  return job_mark_add_s(fname,content) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}

bool job_diagnostics_mark_remove(const JobDescription &desc,const JobUser &user) {
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + sfx_diag;
  bool res1 = job_mark_remove(fname);
  fname = desc.SessionDir() + sfx_diag;
  if(user.StrictSession()) {
    uid_t uid = user.get_uid()==0?desc.get_uid():user.get_uid();
    uid_t gid = user.get_uid()==0?desc.get_gid():user.get_gid();
    JobUser tmp_user(user.Env(),uid,gid);
    return (res1 | (RunFunction::run(tmp_user,"job_diagnostics_mark_remove",&job_mark_remove_callback,&fname,-1) == 0));
  };
  return (res1 | job_mark_remove(fname));
}

bool job_lrmsoutput_mark_remove(const JobDescription &desc,const JobUser &user) {
  std::string fname = desc.SessionDir() + sfx_lrmsoutput;
  if(user.StrictSession()) {
    uid_t uid = user.get_uid()==0?desc.get_uid():user.get_uid();
    uid_t gid = user.get_uid()==0?desc.get_gid():user.get_gid();
    JobUser tmp_user(user.Env(),uid,gid);
    return (RunFunction::run(tmp_user,"job_lrmsoutpur_mark_remove",&job_mark_remove_callback,&fname,-1) == 0);
  };
  return job_mark_remove(fname);
}

typedef struct {
  int h;
  const std::string* fname;
} job_file_read_t;

static int job_file_read_callback(void* arg) {
  int h = ((job_file_read_t*)arg)->h;
  const std::string& fname = *(((job_file_read_t*)arg)->fname);
  int h1=open(fname.c_str(),O_RDONLY);
  if(h1==-1) return -1;
  char buf[256];
  int l;
  for(;;) {
    l=read(h1,buf,256);
    if((l==0) || (l==-1)) break;
    (write(h,buf,l) != -1);
  };
  close(h1); close(h);
  unlink(fname.c_str());
  return 0;
}

bool job_diagnostics_mark_move(const JobDescription &desc,const JobUser &user) {
  std::string fname2 = user.ControlDir() + "/job." + desc.get_id() + sfx_diag;
  int h2=open(fname2.c_str(),O_WRONLY | O_APPEND,S_IRUSR | S_IWUSR);
  if(h2==-1) return false;
  fix_file_owner(fname2,desc,user);
  fix_file_permissions(fname2,desc,user);
  std::string fname1 = user.SessionRoot(desc.get_id()) + "/" + desc.get_id() + sfx_diag;
  if(user.StrictSession()) {
    uid_t uid = user.get_uid()==0?desc.get_uid():user.get_uid();
    uid_t gid = user.get_uid()==0?desc.get_gid():user.get_gid();
    JobUser tmp_user(user.Env(),uid,gid);
    job_file_read_t arg; arg.h=h2; arg.fname=&fname1;
    RunFunction::run(tmp_user,"job_diagnostics_mark_move",&job_file_read_callback,&arg,-1);
    close(h2);
    return true;
  };
  int h1=open(fname1.c_str(),O_RDONLY);
  if(h1==-1) { close(h2); return false; };
  char buf[256];
  int l;
  for(;;) {
    l=read(h1,buf,256);
    if((l==0) || (l==-1)) break;
    (write(h2,buf,l) != -1);
  };
  close(h1); close(h2);
  unlink(fname1.c_str());
  return true;
}

bool job_stdlog_move(const JobDescription& /*desc*/,const JobUser& /*user*/,const std::string& /*logname*/) {
/*

 Status data is now available during runtime.

*/
  return true;
}

bool job_dir_create(const std::string &dname) {
  int err=mkdir(dname.c_str(),S_IRUSR | S_IWUSR | S_IXUSR);
  return (err==0);
}

bool job_mark_put(const std::string &fname) {
  int h=open(fname.c_str(),O_WRONLY | O_CREAT,S_IRUSR | S_IWUSR);
  if(h==-1) return false; close(h); return true;
}

bool job_mark_check(const std::string &fname) {
  struct stat st;
  if(lstat(fname.c_str(),&st) != 0) return false;
  if(!S_ISREG(st.st_mode)) return false;
  return true;
}

bool job_mark_remove(const std::string &fname) {
  if(unlink(fname.c_str()) != 0) { if(errno != ENOENT) return false; };
  return true;
}

std::string job_mark_read_s(const std::string &fname) {
  std::string s("");
  std::ifstream f(fname.c_str()); if(! f.is_open() ) return s;
  char buf[256]; f.getline(buf,254); s=buf;
  return s;
}

long int job_mark_read_i(const std::string &fname) {
  std::ifstream f(fname.c_str()); if(! f.is_open() ) return -1;
  char buf[32]; f.getline(buf,30); f.close();
  char* e; long int i;
  i=strtol(buf,&e,10); if((*e) == 0) return i;
  return -1;
}

bool job_mark_write_s(const std::string &fname,const std::string &content) {
  int h=open(fname.c_str(),O_WRONLY | O_CREAT | O_TRUNC,S_IRUSR | S_IWUSR);
  if(h==-1) return false;
  (write(h,(const void *)content.c_str(),content.length()) != -1);
  close(h); return true;
}

bool job_mark_add_s(const std::string &fname,const std::string &content) {
  int h=open(fname.c_str(),O_WRONLY | O_CREAT | O_APPEND,S_IRUSR | S_IWUSR);
  if(h==-1) return false;
  (write(h,(const void *)content.c_str(),content.length()) != -1);
  close(h); return true;
}

time_t job_mark_time(const std::string &fname) {
  struct stat st;
  if(lstat(fname.c_str(),&st) != 0) return 0;
  return st.st_mtime;
}

long int job_mark_size(const std::string &fname) {
  struct stat st;
  if(lstat(fname.c_str(),&st) != 0) return 0;
  if(!S_ISREG(st.st_mode)) return 0;
  return st.st_size;
}

bool job_diskusage_create_file(const JobDescription &desc,const JobUser& /*user*/,unsigned long long int &requested) {
  std::string fname = desc.SessionDir() + sfx_diskusage;
  int h=open(fname.c_str(),O_WRONLY | O_CREAT,S_IRUSR | S_IWUSR);
  if(h==-1) return false;
  char content[200];
  snprintf(content, sizeof(content), "%llu 0\n",requested);
  (write(h,content,strlen(content)) != -1);
  close(h); return true;
}

bool job_diskusage_read_file(const JobDescription &desc,const JobUser& /*user*/,unsigned long long int &requested,unsigned long long int &used) {
  std::string fname = desc.SessionDir() + sfx_diskusage;
  int h=open(fname.c_str(),O_RDONLY);
  if(h==-1) return false;
  char content[200];
  ssize_t l=read(h,content,199);
  if(l==-1) { close(h); return false; };
  content[l]=0;
  unsigned long long int req_,use_;
  if(sscanf(content,"%llu %llu",&req_,&use_) != 2) { close(h); return false; };
  requested=req_; used=use_;  
  close(h); return true;
}

bool job_diskusage_change_file(const JobDescription &desc,const JobUser& /*user*/,signed long long int used,bool &result) {
  // lock file, read, write, unlock
  std::string fname = desc.SessionDir() + sfx_diskusage;
  int h=open(fname.c_str(),O_RDWR);
  if(h==-1) return false;
  struct flock lock;
  lock.l_type=F_WRLCK;
  lock.l_whence=SEEK_SET;
  lock.l_start=0;
  lock.l_len=0;
  for(;;) {
    if(fcntl(h,F_SETLKW,&lock) != -1) break;
    if(errno == EINTR) continue;
    close(h); return false;
  };
  char content[200];
  ssize_t l=read(h,content,199);
  if(l==-1) {
    lock.l_whence=SEEK_SET; lock.l_start=0; lock.l_len=0;
    lock.l_type=F_UNLCK; fcntl(h,F_SETLK,&lock);
    close(h); return false;
  };
  content[l]=0;
  unsigned long long int req_,use_;
  if(sscanf(content,"%llu %llu",&req_,&use_) != 2) {
    lock.l_whence=SEEK_SET; lock.l_start=0; lock.l_len=0;
    lock.l_type=F_UNLCK; fcntl(h,F_SETLK,&lock);
    close(h); return false;
  };
  if((-used) > use_) {
    result=true; use_=0; // ??????
  }
  else {
    use_+=used; result=true;
    if(use_>req_) result=false;
  };
  lseek(h,0,SEEK_SET);  
  snprintf(content,sizeof(content),"%llu %llu\n",req_,use_);
  (write(h,content,strlen(content)) != -1);
  lock.l_whence=SEEK_SET; lock.l_start=0; lock.l_len=0;
  lock.l_type=F_UNLCK; fcntl(h,F_SETLK,&lock);
  close(h); return true;
}

bool job_diskusage_remove_file(const JobDescription &desc,const JobUser& /*user*/) {
  std::string fname = desc.SessionDir() + sfx_diskusage;
  return job_mark_remove(fname);
}

time_t job_state_time(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_status;
  time_t t = job_mark_time(fname);
  if(t != 0) return t;
  fname = user.ControlDir() + "/" + subdir_cur + "/job." + id + sfx_status;
  t = job_mark_time(fname);
  if(t != 0) return t;
  fname = user.ControlDir() + "/" + subdir_new + "/job." + id + sfx_status;
  t = job_mark_time(fname);
  if(t != 0) return t;
  fname = user.ControlDir() + "/" + subdir_rew + "/job." + id + sfx_status;
  t = job_mark_time(fname);
  if(t != 0) return t;
  fname = user.ControlDir() + "/" + subdir_old + "/job." + id + sfx_status;
  return job_mark_time(fname);
}

job_state_t job_state_read_file(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_status;
  bool pending;
  job_state_t st = job_state_read_file(fname,pending);
  if(st != JOB_STATE_DELETED) return st;
  fname = user.ControlDir() + "/" + subdir_cur + "/job." + id + sfx_status;
  st = job_state_read_file(fname,pending);
  if(st != JOB_STATE_DELETED) return st;
  fname = user.ControlDir() + "/" + subdir_new + "/job." + id + sfx_status;
  st = job_state_read_file(fname,pending);
  if(st != JOB_STATE_DELETED) return st;
  fname = user.ControlDir() + "/" + subdir_rew + "/job." + id + sfx_status;
  st = job_state_read_file(fname,pending);
  if(st != JOB_STATE_DELETED) return st;
  fname = user.ControlDir() + "/" + subdir_old + "/job." + id + sfx_status;
  return job_state_read_file(fname,pending);
}

job_state_t job_state_read_file(const JobId &id,const JobUser &user,bool& pending) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_status;
  job_state_t st = job_state_read_file(fname,pending);
  if(st != JOB_STATE_DELETED) return st;
  fname = user.ControlDir() + "/" + subdir_cur + "/job." + id + sfx_status;
  st = job_state_read_file(fname,pending);
  if(st != JOB_STATE_DELETED) return st;
  fname = user.ControlDir() + "/" + subdir_new + "/job." + id + sfx_status;
  st = job_state_read_file(fname,pending);
  if(st != JOB_STATE_DELETED) return st;
  fname = user.ControlDir() + "/" + subdir_rew + "/job." + id + sfx_status;
  st = job_state_read_file(fname,pending);
  if(st != JOB_STATE_DELETED) return st;
  fname = user.ControlDir() + "/" + subdir_old + "/job." + id + sfx_status;
  return job_state_read_file(fname,pending);
}

bool job_state_write_file(const JobDescription &desc,const JobUser &user,job_state_t state,bool pending) {
  std::string fname;
  if(state == JOB_STATE_ACCEPTED) { 
    fname = user.ControlDir() + "/" + subdir_old + "/job." + desc.get_id() + sfx_status; remove(fname.c_str());
    fname = user.ControlDir() + "/" + subdir_cur + "/job." + desc.get_id() + sfx_status; remove(fname.c_str());
    fname = user.ControlDir() + "/" + subdir_rew + "/job." + desc.get_id() + sfx_status; remove(fname.c_str());
    fname = user.ControlDir() + "/job." + desc.get_id() + sfx_status; remove(fname.c_str());
    fname = user.ControlDir() + "/" + subdir_new + "/job." + desc.get_id() + sfx_status;
  } else if((state == JOB_STATE_FINISHED) || (state == JOB_STATE_DELETED)) {
    fname = user.ControlDir() + "/" + subdir_new + "/job." + desc.get_id() + sfx_status; remove(fname.c_str());
    fname = user.ControlDir() + "/" + subdir_cur + "/job." + desc.get_id() + sfx_status; remove(fname.c_str());
    fname = user.ControlDir() + "/" + subdir_rew + "/job." + desc.get_id() + sfx_status; remove(fname.c_str());
    fname = user.ControlDir() + "/job." + desc.get_id() + sfx_status; remove(fname.c_str());
    fname = user.ControlDir() + "/" + subdir_old + "/job." + desc.get_id() + sfx_status;
  } else {
    fname = user.ControlDir() + "/" + subdir_new + "/job." + desc.get_id() + sfx_status; remove(fname.c_str());
    fname = user.ControlDir() + "/" + subdir_old + "/job." + desc.get_id() + sfx_status; remove(fname.c_str());
    fname = user.ControlDir() + "/" + subdir_rew + "/job." + desc.get_id() + sfx_status; remove(fname.c_str());
    fname = user.ControlDir() + "/job." + desc.get_id() + sfx_status; remove(fname.c_str());
    fname = user.ControlDir() + "/" + subdir_cur + "/job." + desc.get_id() + sfx_status;
  };
  return job_state_write_file(fname,state,pending) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname,desc,user);
}

static job_state_t job_state_read_file(const std::string &fname,bool &pending) {
  std::ifstream f(fname.c_str());
  if(!f.is_open() ) {
    if(!job_mark_check(fname)) return JOB_STATE_DELETED; /* job does not exist */
    return JOB_STATE_UNDEFINED; /* can't open file */
  };

  char buf[32];
  f.getline(buf,30);
  /* interpret information */
  char* p = buf;
  if(!strncmp("PENDING:",p,8)) { p+=8; pending=true; } else { pending=false; };
  for(int i = 0;states_all[i].name != NULL;i++) {
    if(!strcmp(states_all[i].name,p)) {
      f.close();
      return states_all[i].id;
    };
  };
  f.close();
  return JOB_STATE_UNDEFINED; /* broken file */
}

static bool job_state_write_file(const std::string &fname,job_state_t state,bool pending) {
  std::ofstream f(fname.c_str(),std::ios::out | std::ios::trunc);
  if(! f.is_open() ) return false; /* can't open file */
  if(pending) f<<"PENDING:";
  f<<states_all[state].name<<std::endl;
  f.close();
  return true;
}

time_t job_description_time(const JobId &id,const JobUser &user) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_rsl;
  return job_mark_time(fname);
}

bool job_description_read_file(const JobId &id,const JobUser &user,std::string &rsl) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_rsl;
  return job_description_read_file(fname,rsl);
}

bool job_description_read_file(const std::string &fname,std::string &rsl) {
  char buf[256];
  std::string::size_type n=0;
  std::ifstream f(fname.c_str());
  if(! f.is_open() ) return false; /* can't open file */
  rsl.erase();
  while(!f.eof()) {
    memset(buf,0,256);
    f.read(buf,255); rsl+=buf;
    for(;(n=rsl.find('\n',n)) != std::string::npos;) rsl.erase(n,1);
    n=rsl.length();
  };
  f.close();
  return true;
}

bool job_description_write_file(const std::string &fname,std::string &rsl) {
  return job_description_write_file(fname,rsl.c_str());
}

bool job_description_write_file(const std::string &fname,const char* rsl) {
  std::ofstream f(fname.c_str(),std::ios::out | std::ios::trunc);
  if(! f.is_open() ) return false; /* can't open file */
  f.write(rsl,strlen(rsl));
  f.close();
  return true;
}

bool job_acl_read_file(const JobId &id,const JobUser &user,std::string &acl) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_acl;
  return job_description_read_file(fname,acl);
}

bool job_acl_write_file(const JobId &id,const JobUser &user,std::string &acl) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_acl;
  return job_description_write_file(fname,acl.c_str());
}

bool job_xml_read_file(const JobId &id,const JobUser &user,std::string &xml) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_xml;
  return job_description_read_file(fname,xml);
}

bool job_xml_write_file(const JobId &id,const JobUser &user,const std::string &xml) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_xml;
  return job_description_write_file(fname,xml.c_str());
}

bool job_local_write_file(const JobDescription &desc,const JobUser &user,const JobLocalDescription &job_desc) {
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + sfx_local;
  return job_local_write_file(fname,job_desc) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname,desc,user);
}

static inline bool write_str(int f,const std::string &str) {
  const char* buf = str.c_str();
  std::string::size_type len = str.length();
  for(;len > 0;) {
    ssize_t l = write(f,buf,len);
    if(l < 0) {
      if(errno != EINTR) return false;
    } else {
      len -= l; buf += l;
    };
  };
  return true;
}

static inline bool read_str(int f,char* buf,int size) {
  char c;
  int pos = 0;
  for(;;) {
    ssize_t l = read(f,&c,1);
    if((l == -1) && (errno == EINTR)) continue;
    if(l < 0) return false;
    if(l == 0) {
      if(!pos) return false;
      break;
    };
    if(c == '\n') break;
    if(pos < (size-1)) {
      buf[pos] = c;
      ++pos;
      buf[pos] = 0;
    } else {
      ++pos;
    };
  };
  return true;
}

static inline void write_pair(int f,const std::string &name,const std::string &value) {
  if(value.length() <= 0) return;
  write_str(f,name);
  write_str(f,"=");
  write_str(f,value);
  write_str(f,"\n");
}

static inline void write_pair(int f,const std::string &name,const Arc::Time &value) {
  if(value == -1) return;
  write_str(f,name);
  write_str(f,"=");
  write_str(f,value.str(Arc::MDSTime));
  write_str(f,"\n");
}

static inline void write_pair(int f,const std::string &name,bool value) {
  write_str(f,name);
  write_str(f,"=");
  write_str(f,(value?"yes":"no"));
  write_str(f,"\n");
}

bool job_local_write_file(const std::string &fname,const JobLocalDescription &job_desc) {
  return job_desc.write(fname);
}

bool job_local_read_file(const JobId &id,const JobUser &user,JobLocalDescription &job_desc) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_local;
  return job_local_read_file(fname,job_desc);
}

bool job_local_read_file(const std::string &fname,JobLocalDescription &job_desc) {
  return job_desc.read(fname);
}

bool job_local_read_var(const std::string &fname,const std::string &vnam,std::string &value) {
  return JobLocalDescription::read_var(fname,vnam,value);
}

bool job_local_read_lifetime(const JobId &id,const JobUser &user,time_t &lifetime) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_local;
  std::string str;
  if(!job_local_read_var(fname,"lifetime",str)) return false;
  char* str_e;
  unsigned long int t = strtoul(str.c_str(),&str_e,10);
  if((*str_e) != 0) return false;
  lifetime=t;
  return true;
}

bool job_local_read_cleanuptime(const JobId &id,const JobUser &user,time_t &cleanuptime) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_local;
  std::string str;
  if(!job_local_read_var(fname,"cleanuptime",str)) return false;
  cleanuptime=Arc::Time(str).GetTime();
  return true;
}

bool job_local_read_notify(const JobId &id,const JobUser &user,std::string &notify) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_local;
  if(!job_local_read_var(fname,"notify",notify)) return false;
  return true;
}

bool job_local_read_failed(const JobId &id,const JobUser &user,std::string &state,std::string &cause) {
  state = "";
  cause = "";
  std::string fname = user.ControlDir() + "/job." + id + sfx_local;
  job_local_read_var(fname,"failedstate",state);
  job_local_read_var(fname,"failedcause",cause);
  return true;
}

/* job.ID.input functions */

bool job_input_write_file(const JobDescription &desc,const JobUser &user,std::list<FileData> &files) {
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + sfx_input;
  return job_Xput_write_file(fname,files) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}

bool job_input_read_file(const JobId &id,const JobUser &user,std::list<FileData> &files) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_input;
  return job_Xput_read_file(fname,files);
}

bool job_input_status_read_file(const JobId &id,const JobUser &user,std::list<std::string>& files) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_inputstatus;
  std::ifstream f(fname.c_str());
  if(! f.is_open() ) return false; /* can't open file */
  for(;!f.eof();) {
    std::string fn; f >> fn;
    if(fn.length() != 0) {  /* returns zero length only if empty std::string */
      files.push_back(fn);
    };
  };
  f.close();
  return true;
}

bool job_output_status_read_file(const JobId &id,const JobUser &user,std::list<FileData> &files) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_outputstatus;
  return job_Xput_read_file(fname,files);
}

bool job_output_status_write_file(const JobDescription &desc,const JobUser &user,std::list<FileData> &files) {
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + sfx_outputstatus;
  return job_Xput_write_file(fname,files) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}

bool job_input_status_add_file(const JobDescription &desc,const JobUser &user,const std::string& file) {
  // 1. lock
  // 2. add
  // 3. unlock
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + sfx_inputstatus;
  int h=open(fname.c_str(),O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
  if(h==-1) return false;
  if(file.empty()) {
    close(h); return true;
  };
  struct flock lock;
  lock.l_type=F_WRLCK;
  lock.l_whence=SEEK_SET; lock.l_start=0; lock.l_len=0;
  for(;;) {
    if(fcntl(h,F_SETLKW,&lock) != -1) break;
    if(errno == EINTR) continue;
    close(h); return false;
  };
  std::string line = file + "\n";
  bool r = write_str(h,line);
  lock.l_whence=SEEK_SET; lock.l_start=0; lock.l_len=0;
  lock.l_type=F_UNLCK; fcntl(h,F_SETLK,&lock);
  for(;;) {
    if(fcntl(h,F_SETLK,&lock) != -1) break;
    if(errno == EINTR) continue;
    r=false; break;
  };
  close(h);
  return r;
}

bool job_output_status_add_file(const JobDescription &desc,const JobUser &user,const FileData& file) {
  // 1. lock
  // 2. add
  // 3. unlock
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + sfx_outputstatus;
  int h=open(fname.c_str(),O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
  if(h==-1) return false;
  if(file.pfn.empty()) {
    close(h); return true;
  };
  struct flock lock;
  lock.l_type=F_WRLCK;
  lock.l_whence=SEEK_SET; lock.l_start=0; lock.l_len=0;
  for(;;) {
    if(fcntl(h,F_SETLKW,&lock) != -1) break;
    if(errno == EINTR) continue;
    close(h); return false;
  };
  std::ostringstream line;
  line<<file<<"\n";
  bool r = write_str(h,line.str());
  lock.l_whence=SEEK_SET; lock.l_start=0; lock.l_len=0;
  lock.l_type=F_UNLCK; fcntl(h,F_SETLK,&lock);
  for(;;) {
    if(fcntl(h,F_SETLK,&lock) != -1) break;
    if(errno == EINTR) continue;
    r=false; break;
  };
  close(h);
  return r;
}

std::string job_proxy_filename(const JobId &id, const JobUser &user){
	return user.ControlDir() + "/job." + id + sfx_proxy;
}
/* job.ID.rte functions */

bool job_rte_write_file(const JobDescription &desc,const JobUser &user,std::list<std::string> &rtes) {
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + ".rte";
  return job_strings_write_file(fname,rtes) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}

bool job_rte_read_file(const JobId &id,const JobUser &user,std::list<std::string> &rtes) {
  std::string fname = user.ControlDir() + "/job." + id + ".rte";
  return job_strings_read_file(fname,rtes);
}

/* job.ID.output functions */
/*
bool job_cache_write_file(const JobDescription &desc,const JobUser &user,std::list<FileData> &files) {
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + ".cache";
  return job_Xput_write_file(fname,files) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}
*/

bool job_output_write_file(const JobDescription &desc,const JobUser &user,std::list<FileData> &files,job_output_mode mode) {
  std::string fname = user.ControlDir() + "/job." + desc.get_id() + sfx_output;
  return job_Xput_write_file(fname,files,mode) & fix_file_owner(fname,desc,user) & fix_file_permissions(fname);
}

/*
bool job_cache_read_file(const JobId &id,const JobUser &user,std::list<FileData> &files) {
  std::string fname = user.ControlDir() + "/job." + id + ".cache";
  return job_Xput_read_file(fname,files);
}
*/

bool job_output_read_file(const JobId &id,const JobUser &user,std::list<FileData> &files) {
  std::string fname = user.ControlDir() + "/job." + id + sfx_output;
  return job_Xput_read_file(fname,files);
}

/* common functions */

bool job_Xput_write_file(const std::string &fname,std::list<FileData> &files,job_output_mode mode) {
  std::ofstream f(fname.c_str(),std::ios::out | std::ios::trunc);
  if(! f.is_open() ) return false; /* can't open file */
  for(FileData::iterator i=files.begin();i!=files.end(); ++i) { 
    if(mode == job_output_all) {
      f << (*i) << std::endl;
    } else if(mode == job_output_success) {
      if(i->ifsuccess) {
        f << (*i) << std::endl;
      };
    } else if(mode == job_output_cancel) {
      if(i->ifcancel) {
        f << (*i) << std::endl;
      };
    } else if(mode == job_output_failure) {
      if(i->iffailure) {
        f << (*i) << std::endl;
      };
    };
  };
  f.close();
  return true;
}

bool job_Xput_read_file(const std::string &fname,std::list<FileData> &files) {
  std::ifstream f(fname.c_str());
  if(! f.is_open() ) return false; /* can't open file */
  for(;!f.eof();) {
    FileData fd; f >> fd;
    if(!fd.pfn.empty()) files.push_back(fd);
  };
  f.close();
  return true;
}

static bool job_strings_write_file(const std::string &fname,std::list<std::string> &strs) {
  std::ofstream f(fname.c_str(),std::ios::out | std::ios::trunc);
  if(! f.is_open() ) return false; /* can't open file */
  for(std::list<std::string>::iterator i=strs.begin();i!=strs.end(); ++i) {
    f << (*i) << std::endl;
  };
  f.close();
  return true;
}

bool job_strings_read_file(const std::string &fname,std::list<std::string> &strs) {
  std::ifstream f(fname.c_str());
  if(! f.is_open() ) return false; /* can't open file */
  for(;!f.eof();) {
    std::string str; f >> str;
    if(!str.empty()) strs.push_back(str);
  };
  f.close();
  return true;
}

bool job_clean_finished(const JobId &id,const JobUser &user) {
  std::string fname;
  fname = user.ControlDir()+"/job."+id+".proxy.tmp"; remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+".lrms_done"; remove(fname.c_str());
  return true;
}

bool job_clean_deleted(const JobDescription &desc,const JobUser &user,std::list<std::string> cache_per_job_dirs) {
  std::string id = desc.get_id();
  job_clean_finished(id,user);
  std::string fname;
  fname = user.ControlDir()+"/job."+id+".proxy"; remove(fname.c_str());
  fname = user.ControlDir()+"/"+subdir_new+"/job."+id+sfx_restart; remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+sfx_errors; remove(fname.c_str());
  fname = user.ControlDir()+"/"+subdir_new+"/job."+id+sfx_cancel; remove(fname.c_str());
  fname = user.ControlDir()+"/"+subdir_new+"/job."+id+sfx_clean;  remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+sfx_output; remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+sfx_input; remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+".rte"; remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+".grami_log"; remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+".statistics"; remove(fname.c_str());
  fname = user.SessionRoot(id)+"/"+id+sfx_lrmsoutput; remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+sfx_outputstatus; remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+sfx_inputstatus; remove(fname.c_str());
  /* remove session directory */
  std::list<FileData> flist;
  std::string dname = user.SessionRoot(id)+"/"+id;
  if(user.StrictSession()) {
    uid_t uid = user.get_uid()==0?desc.get_uid():user.get_uid();
    uid_t gid = user.get_uid()==0?desc.get_gid():user.get_gid();
    JobUser tmp_user(user.Env(),uid,gid);
    job_dir_remove_t arg; arg.dname=&dname; arg.flist=&flist;
    return (RunFunction::run(tmp_user,"job_clean_deleted",&job_dir_remove_callback,&arg,-1) == 0);
  } else {
    delete_all_files(dname,flist,true);
    remove(dname.c_str());
  };
  // remove cache per-job links, in case this failed earlier
  // list all files in the dir and delete them
  for (std::list<std::string>::iterator i = cache_per_job_dirs.begin(); i != cache_per_job_dirs.end(); i++) {
    std::string cache_job_dir = (*i) + "/" + id;
    DIR * dirp = opendir(cache_job_dir.c_str());
    if ( dirp == NULL) return true; // already deleted
    struct dirent *dp;
    while ((dp = readdir(dirp)))  {
      if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) continue;
      std::string to_delete = cache_job_dir + "/" + dp->d_name; 
      remove(to_delete.c_str());
    }
    closedir(dirp);
    // remove now-empty dir
    rmdir(cache_job_dir.c_str());
  }
  return true;
}

bool job_clean_final(const JobDescription &desc,const JobUser &user) {
  std::string id = desc.get_id();
  job_clean_finished(id,user);
  job_clean_deleted(desc,user);
  std::string fname;
  fname = user.ControlDir()+"/job."+id+sfx_local;  remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+".statistics"; remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+".grami"; remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+sfx_failed; remove(fname.c_str());
  job_diagnostics_mark_remove(desc,user);
  job_lrmsoutput_mark_remove(desc,user);
  fname = user.ControlDir()+"/job."+id+sfx_status; remove(fname.c_str());
  fname = user.ControlDir()+"/"+subdir_new+"/job."+id+sfx_status; remove(fname.c_str());
  fname = user.ControlDir()+"/"+subdir_cur+"/job."+id+sfx_status; remove(fname.c_str());
  fname = user.ControlDir()+"/"+subdir_old+"/job."+id+sfx_status; remove(fname.c_str());
  fname = user.ControlDir()+"/"+subdir_rew+"/job."+id+sfx_status; remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+sfx_rsl; remove(fname.c_str());
  fname = user.ControlDir()+"/job."+id+sfx_xml; remove(fname.c_str());
  return true;
}


