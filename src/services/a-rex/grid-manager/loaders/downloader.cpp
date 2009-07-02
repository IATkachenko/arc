#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
/*
  Download files specified in job.ID.input and check if user uploaded files.
  Additionally check if this is a migrated job and if so kill the job on old cluster.
  result: 0 - ok, 1 - unrecoverable error, 2 - potentially recoverable,
  3 - certificate error, 4 - should retry.
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>

#include <arc/XMLNode.h>
#include <arc/client/ACCLoader.h>
#include <arc/client/Job.h>
#include <arc/client/JobController.h>
#include <arc/data/DMC.h>
#include <arc/data/CheckSum.h>
#include <arc/data/FileCache.h>
#include <arc/data/DataPoint.h>
#include <arc/data/DataMover.h>
#include <arc/message/MCC.h>
#include <arc/StringConv.h>
#include <arc/Thread.h>
#include <arc/URL.h>
#include <arc/Utils.h>

#include "../jobs/job.h"
#include "../jobs/users.h"
#include "../files/info_types.h"
#include "../files/info_files.h"
#include "../files/delete.h"
#include "../conf/environment.h"
#include "../misc/proxy.h"
#include "../conf/conf_map.h"
#include "../conf/conf_cache.h"

#define olog std::cerr
#define odlog(LEVEL) std::cerr

static Arc::Logger& logger = Arc::Logger::getRootLogger();

/* check for user uploaded files every 60 seconds */
#define CHECK_PERIOD 60
/* maximum number of retries (for every source/destination) */
#define MAX_RETRIES 5
/* maximum number simultaneous downloads */
#define MAX_DOWNLOADS 5
/* maximum time for user files to upload (per file) */
#define MAX_USER_TIME 600

class PointPair;

static bool CollectCredentials(std::string& proxy,std::string& cert,std::string& key,std::string& cadir) {
  proxy=Arc::GetEnv("X509_USER_PROXY");
  if(proxy.empty()) {
    cert=Arc::GetEnv("X509_USER_CERT");
    key=Arc::GetEnv("X509_USER_KEY");
  };
  if(proxy.empty() && cert.empty()) {
    proxy="/tmp/x509_up"+Arc::tostring(getuid());
  };
  cadir=Arc::GetEnv("X509_CERT_DIR");
  if(cadir.empty()) cadir="/etc/grid-security/certificates";
}

class FileDataEx : public FileData {
 public:
  typedef std::list<FileDataEx>::iterator iterator;
  Arc::DataStatus res;
  std::string failure_description;
  PointPair* pair;
  FileDataEx(const FileData& f) : FileData(f),
				  res(Arc::DataStatus::Success),
				  pair(NULL) {}
  FileDataEx(const FileData& f,
	     Arc::DataStatus r,
	     const std::string& fd) : FileData(f),
				      res(r),
				      failure_description(fd),
				      pair(NULL) {}
};

static std::list<FileData> job_files_;
static std::list<FileDataEx> job_files;
static std::list<FileDataEx> processed_files;
static std::list<FileDataEx> failed_files;
static Arc::SimpleCondition pair_condition;
static int pairs_initiated = 0;

bool is_checksum(std::string str) {
  /* check if have : */
  std::string::size_type n;
  const char *s = str.c_str();
  if((n=str.find('.')) == std::string::npos) return false;
  if(str.find('.',n+1) != std::string::npos) return false;
  for(unsigned int i=0;i<n;i++) { if(!isdigit(s[i])) return false; };
  for(unsigned int i=n+1;s[i];i++) { if(!isdigit(s[i])) return false; };
  return true;
}

int clean_files(std::list<FileData> &job_files,char* session_dir) {
  std::string session(session_dir);
  /* delete only downloadable files, let user manage his/hers files */
  std::list<FileData> tmp;
  if(delete_all_files(session,job_files,false,true,false) != 2) return 0;
  return 1;
}

/*
   Check for existence of user uploadable file
   returns 0 if file exists 
           1 - it is not proper file or other error
           2 - not here yet 
*/
int user_file_exists(FileData &dt,char* session_dir,std::string* error = NULL) {
  struct stat st;
  const char *str = dt.lfn.c_str();
  if(strcmp(str,"*.*") == 0) return 0; /* do not wait for this file */
  std::string fname=std::string(session_dir) + '/' + dt.pfn;
  /* check if file does exist at all */
  if(lstat(fname.c_str(),&st) != 0) return 2;
  /* check for misconfiguration */
  /* parse files information */
  char *str_;
  unsigned long long int fsize;
  unsigned long long int fsum = (unsigned long long int)(-1);
  bool have_size = false;
  bool have_checksum = false;
  errno = 0;
  fsize = strtoull(str,&str_,10);
  if((*str_) == '.') {
    if(str_ != str) have_size=true;
    str=str_+1;
    fsum = strtoull(str,&str_,10);
    if((*str_) != 0) {
      olog << "Invalid checksum in " << dt.lfn << " for " << dt.pfn << std::endl;
      if(error) (*error)="Bad information about file: checksum can't be parsed.";
      return 1;
    };
    have_checksum=true;
  }
  else {
    if(str_ != str) have_size=true;
    if((*str_) != 0) {
      olog << "Invalid file size in " << dt.lfn << " for " << dt.pfn << std::endl;
      if(error) (*error)="Bad information about file: size can't be parsed.";
      return 1;
    };
  };
  if(S_ISDIR(st.st_mode)) {
    if(have_size || have_checksum) {
      if(error) (*error)="Expected file. Directory found.";
      return 1;
    };
    return 0;
  };
  if(!S_ISREG(st.st_mode)) {
    if(error) (*error)="Expected ordinary file. Special object found.";
    return 1;
  };
  /* now check if proper size */
  if(have_size) {
    if(st.st_size < fsize) return 2;
    if(st.st_size > fsize) {
      olog << "Invalid file " << dt.pfn << " is too big." << std::endl;
      if(error) (*error)="Delivered file is bigger than specified.";
      return 1; /* too big file */
    };
  };
  if(have_checksum) {
    int h=open(fname.c_str(),O_RDONLY);
    if(h==-1) { /* if we can't read that file job won't too */
      olog << "Error accessing file " << dt.pfn << std::endl;
      if(error) (*error)="Delivered file is unreadable.";
      return 1;
    };
    Arc::CRC32Sum crc;
    char buffer[1024];
    ssize_t l;
    size_t ll = 0;
    for(;;) {
      if((l=read(h,buffer,1024)) == -1) {
        olog << "Error reading file " << dt.pfn << std::endl; return 1;
        if(error) (*error)="Could not read file to compute checksum.";
      };
      if(l==0) break; ll+=l;
      crc.add(buffer,l);
    };
    close(h);
    crc.end();
    if(fsum != crc.crc()) {
      if(have_size) { /* size was checked - it is an error to have wrong crc */ 
        olog << "File " << dt.pfn << " has wrong CRC." << std::endl;
        if(error) (*error)="Delivered file has wrong checksum.";
        return 1;
      };
      return 2; /* just not uploaded yet */
    };
  };
  return 0; /* all checks passed - file is ok */
}

class PointPair {
 public:
  Arc::DataPoint* source;
  Arc::DataPoint* destination;
  PointPair(const std::string& source_url,const std::string& destination_url):
                                                      source(Arc::DMC::GetDataPoint(source_url)),
                                                      destination(Arc::DMC::GetDataPoint(destination_url)) {};
  ~PointPair(void) { if(source) delete source; if(destination) delete destination; };
  static void callback(Arc::DataMover*,Arc::DataStatus res,const std::string&,void* arg) {
    FileDataEx::iterator &it = *((FileDataEx::iterator*)arg);
    pair_condition.lock();
    if(!res) {
      it->failure_description=(std::string)res;
      it->res=res;
      olog<<"Failed downloading file "<<it->lfn<<" - "<<it->failure_description<<std::endl;
      if((it->pair->source->GetTries() <= 0) || (it->pair->destination->GetTries() <= 0)) {
        delete it->pair; it->pair=NULL;
        failed_files.push_back(*it);
      } else {
        job_files.push_back(*it);
        olog<<"Retrying"<<std::endl;
      };
    } else {
      olog<<"Downloaded file "<<it->lfn<<std::endl;
      delete it->pair; it->pair=NULL;
      processed_files.push_back(*it);
    };
    job_files.erase(it);
    --pairs_initiated;
    pair_condition.signal_nonblock();
    pair_condition.unlock();
    delete &it;
  };
};

int main(int argc,char** argv) {
  Arc::LogStream logcerr(std::cerr);
  Arc::Logger::getRootLogger().addDestination(logcerr);
  int res=0;
  bool not_uploaded;
  time_t start_time=time(NULL);
  time_t upload_timeout = 0;
  int n_threads = 1;
  int n_files = MAX_DOWNLOADS;
  /* if != 0 - change owner of downloaded files to this user */
  std::string file_owner_username = "";
  uid_t file_owner = 0;
  gid_t file_group = 0;
  std::vector<std::string> caches;
  bool use_conf_cache=false;
  unsigned long long int min_speed = 0;
  time_t min_speed_time = 300;
  unsigned long long int min_average_speed = 0;
  time_t max_inactivity_time = 300;
  bool secure = true;
  bool userfiles_only = false;
  bool passive = false;
  std::string failure_reason("");
  std::string x509_proxy, x509_cert, x509_key, x509_cadir;

  // process optional arguments
  for(;;) {
    opterr=0;
    int optc=getopt(argc,argv,"+hclpZfn:t:n:u:U:s:S:a:i:d:");
    if(optc == -1) break;
    switch(optc) {
      case 'h': {
        olog<<"Usage: downloader [-hclpZf] [-n files] [-t threads] [-U uid]"<<std::endl;
        olog<<"          [-u username] [-s min_speed] [-S min_speed_time]"<<std::endl;
        olog<<"          [-a min_average_speed] [-i min_activity_time]"<<std::endl;
        olog<<"          [-d debug_level] job_id control_directory"<<std::endl;
        olog<<"          session_directory [cache options]"<<std::endl; 
        exit(1);
      }; break;
      case 'c': {
        secure=false;
      }; break;
      case 'l': {
        userfiles_only=true;
      }; break;
      case 'p': {
        passive=true;
      }; break;
      case 'Z': {
        central_configuration=true;
      }; break;
      case 'f': {
        use_conf_cache=true;
      }; break;
      case 'd': {
//@        LogTime::Level(NotifyLevel(atoi(optarg)));
      }; break;
      case 't': {
        n_threads=atoi(optarg);
        if(n_threads < 1) {
          olog<<"Wrong number of threads"<<std::endl; exit(1);
        };
      }; break;
      case 'n': {
        n_files=atoi(optarg);
        if(n_files < 1) {
          olog<<"Wrong number of files"<<std::endl; exit(1);
        };
      }; break;
      case 'U': {
        unsigned int tuid;
        if(!Arc::stringto(std::string(optarg),tuid)) {
          olog<<"Bad number "<<optarg<<std::endl; exit(1);
        };
        struct passwd pw_;
        struct passwd *pw;
        char buf[BUFSIZ];
        getpwuid_r(tuid,&pw_,buf,BUFSIZ,&pw);
        if(pw == NULL) {
          olog<<"Wrong user name"<<std::endl; exit(1);
        };
        file_owner=pw->pw_uid;
        file_group=pw->pw_gid;
        if(pw->pw_name) file_owner_username=pw->pw_name;
        if((getuid() != 0) && (getuid() != file_owner)) {
          olog<<"Specified user can't be handled"<<std::endl; exit(1);
        };
      }; break;
      case 'u': {
        struct passwd pw_;
        struct passwd *pw;
        char buf[BUFSIZ];
        getpwnam_r(optarg,&pw_,buf,BUFSIZ,&pw);
        if(pw == NULL) {
          olog<<"Wrong user name"<<std::endl; exit(1);
        };
        file_owner=pw->pw_uid;
        file_group=pw->pw_gid;
        if(pw->pw_name) file_owner_username=pw->pw_name;
        if((getuid() != 0) && (getuid() != file_owner)) {
          olog<<"Specified user can't be handled"<<std::endl; exit(1);
        };
      }; break;
      case 's': {
        unsigned int tmpi;
        if(!Arc::stringto(std::string(optarg),tmpi)) {
          olog<<"Bad number "<<optarg<<std::endl; exit(1);
        };
        min_speed=tmpi;
      }; break;
      case 'S': {
        unsigned int tmpi;
        if(!Arc::stringto(std::string(optarg),tmpi)) {
          olog<<"Bad number "<<optarg<<std::endl; exit(1);
        };
        min_speed_time=tmpi;
      }; break;
      case 'a': {
        unsigned int tmpi;
        if(!Arc::stringto(std::string(optarg),tmpi)) {
          olog<<"Bad number "<<optarg<<std::endl; exit(1);
        };
        min_average_speed=tmpi;
      }; break;
      case 'i': {
        unsigned int tmpi;
        if(!Arc::stringto(std::string(optarg),tmpi)) {
          olog<<"Bad number "<<optarg<<std::endl; exit(1);
        };
        max_inactivity_time=tmpi;
      }; break;
      case '?': {
        olog<<"Unsupported option '"<<(char)optopt<<"'"<<std::endl;
        exit(1);
      }; break;
      case ':': {
        olog<<"Missing parameter for option '"<<(char)optopt<<"'"<<std::endl;
        exit(1);
      }; break;
      default: {
        olog<<"Undefined processing error"<<std::endl;
        exit(1);
      };
    };
  };
  // process required arguments
  char * id = argv[optind+0];
  if(!id) { olog << "Missing job id" << std::endl; return 1; };
  char* control_dir = argv[optind+1];
  if(!control_dir) { olog << "Missing control directory" << std::endl; return 1; };
  char* session_dir = argv[optind+2];
  if(!session_dir) { olog << "Missing session directory" << std::endl; return 1; };

  // prepare Job and User descriptions (needed for substitutions in cache dirs)
  JobDescription desc(id,session_dir);
  uid_t uid;
  gid_t gid;
  if(file_owner != 0) { uid=file_owner; }
  else { uid= getuid(); };
  if(file_group != 0) { gid=file_group; }
  else { gid= getgid(); };
  desc.set_uid(uid,gid);
  JobUser user(uid);
  user.SetControlDir(control_dir);
  user.SetSessionRoot(session_dir);
  
  // if u or U option not set, use our username
  if (file_owner_username == "") {
    struct passwd pw_;
    struct passwd *pw;
    char buf[BUFSIZ];
    getpwuid_r(getuid(),&pw_,buf,BUFSIZ,&pw);
    if(pw == NULL) {
      olog<<"Wrong user name"<<std::endl; exit(1);
    }
    if(pw->pw_name) file_owner_username=pw->pw_name;
  }
  
  if(use_conf_cache) {
    // use cache dir(s) from conf file
    try {
      CacheConfig * cache_config = new CacheConfig(std::string(file_owner_username));
      std::list<std::string> conf_caches = cache_config->getCacheDirs();
      // add each cache to our list
      for (std::list<std::string>::iterator i = conf_caches.begin(); i != conf_caches.end(); i++) {
        user.substitute(*i);
        caches.push_back(*i);
      }
    }
    catch (CacheConfigException e) {
      olog<<"Error with cache configuration: "<<e.what()<<std::endl;
      olog<<"Will not use caching"<<std::endl;
    }
  }
  else {
    if(argv[optind+3]) {
      std::string cache_path = argv[optind+3];
      if(argv[optind+4]) cache_path += " "+std::string(argv[optind+4]);
      caches.push_back(cache_path);
    }
  }
  if(min_speed != 0) { olog<<"Minimal speed: "<<min_speed<<" B/s during "<<min_speed_time<<" s"<<std::endl; };
  if(min_average_speed != 0) { olog<<"Minimal average speed: "<<min_average_speed<<" B/s"<<std::endl; };
  if(max_inactivity_time != 0) { olog<<"Maximal inactivity time: "<<max_inactivity_time<<" s"<<std::endl; };

  CollectCredentials(x509_proxy,x509_cert,x509_key,x509_cadir);
  prepare_proxy();

  if(n_threads > 10) {
    olog<<"Won't use more than 10 threads"<<std::endl;
    n_threads=10;
  };
/*
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  Add this to DataMove !!!!!!!!!!!!
*/
  UrlMapConfig url_map;
  olog<<"Downloader started"<<std::endl;

  Arc::FileCache * cache;
  if(!caches.empty()) {
    cache = new Arc::FileCache(caches,std::string(id),uid,gid);
    if (!(*cache)) {
      olog << "Error creating cache" << std::endl;
      exit(1);
    }
  }
  else {
    // if no cache defined, use null cache
    cache = new Arc::FileCache();
  }
  Arc::DataMover mover;
  mover.retry(false);
  mover.secure(secure);
  mover.passive(passive);
  if(min_speed != 0)
    mover.set_default_min_speed(min_speed,min_speed_time);
  if(min_average_speed != 0)
    mover.set_default_min_average_speed(min_average_speed);
  if(max_inactivity_time != 0)
    mover.set_default_max_inactivity_time(max_inactivity_time);
  bool transfered = true;
  bool credentials_expired = false;

  if(!job_input_read_file(desc.get_id(),user,job_files_)) {
    failure_reason+="Internal error in downloader\n";
    olog << "Can't read list of input files" << std::endl; res=1; goto exit;
  };
  // check for duplicates (see bug 1285)
  for (std::list<FileData>::iterator i = job_files_.begin(); i != job_files_.end(); i++) {
    for (std::list<FileData>::iterator j = job_files_.begin(); j != job_files_.end(); j++) {
      if (i != j && j->pfn == i->pfn) { olog << "Error: duplicate file in list of input files: " << i->pfn << std::endl; res=1; goto exit; }
    }
  }
  
  // remove bad files
  if(clean_files(job_files_,session_dir) != 0) { 
    failure_reason+="Internal error in downloader\n";
    olog << "Can't remove junk files" << std::endl; res=1; goto exit;
  };
  for(std::list<FileData>::iterator i = job_files_.begin();i!=job_files_.end();++i) {
    job_files.push_back(*i);
  };
  // initialize structures to handle download
  /* TODO: add threads=# to all urls if n_threads!=1 */
  // Compute wait time for user files
  for(FileDataEx::iterator i=job_files.begin();i!=job_files.end();++i) {
    if(i->lfn.find(":") == std::string::npos) { /* is it lfn ? */
      upload_timeout+=MAX_USER_TIME;
    } else {
      userfiles_only=false;
    };
  };
  // Main download cycle
  if(!userfiles_only) for(;;) {
    // Initiate transfers
    int n = 0;
    pair_condition.lock();
    for(FileDataEx::iterator i=job_files.begin();i!=job_files.end();++i) {
      if(i->lfn.find(":") != std::string::npos) { /* is it lfn ? */
        ++n;
        if(n <= pairs_initiated) continue; // skip files being processed
        if(n > n_files) break; // quit if not allowed to process more
        /* have place and file to download */
        std::string destination=std::string("file://") + session_dir + i->pfn;
        std::string source = i->lfn;
        if(i->pair == NULL) {
          /* define place to store */
          if(strncasecmp(source.c_str(),"file://",7) == 0) {
            failure_reason+=std::string("User requested local input file ")+source.c_str()+"\n";
            olog<<"FATAL ERROR: local source for download: "<<source<<std::endl; res=1; goto exit;
          };
          PointPair* pair = new PointPair(source,destination);
          if(!(pair->source)) {
            failure_reason+=std::string("Can't accept URL ")+source.c_str()+"\n";
            olog<<"FATAL ERROR: can't accept URL: "<<source<<std::endl; res=1; goto exit;
          };
          if(!(pair->destination)) {
            failure_reason+=std::string("Can't accept URL ")+destination.c_str()+"\n";
            olog<<"FATAL ERROR: can't accept URL: "<<destination<<std::endl; res=1; goto exit;
          };
          pair->source->AssignCredentials(x509_proxy,x509_cert,x509_key,x509_cadir);
          i->pair=pair;
        };
        FileDataEx::iterator* it = new FileDataEx::iterator(i);
        if(!mover.Transfer(*(i->pair->source), *(i->pair->destination), *cache,
			   url_map, min_speed, min_speed_time,
			   min_average_speed, max_inactivity_time,
			   i->failure_description, &PointPair::callback, it,
			   i->pfn.c_str())) {
          failure_reason+=std::string("Failed to initiate file transfer: ")+source.c_str()+" - "+i->failure_description+"\n";
          olog<<"FATAL ERROR: Failed to initiate file transfer: "<<source<<" - "<<i->failure_description<<std::endl;
          delete it; res=1; goto exit;
        };
        ++pairs_initiated;
      };
    };
    if(pairs_initiated <= 0) { pair_condition.unlock(); break; }; // Looks like no more files to process
    // Processing initiated - now wait for event
    pair_condition.wait_nonblock();
    pair_condition.unlock();
  };
  // Print download summary
  for(FileDataEx::iterator i=processed_files.begin();i!=processed_files.end();++i) {
    odlog(INFO)<<"Downloaded "<<i->lfn<<std::endl;
    if(Arc::URL(i->lfn).Option("exec") == "yes") {
      fix_file_permissions(session_dir+i->pfn,true);
    };
  };
  for(FileDataEx::iterator i=failed_files.begin();i!=failed_files.end();++i) {
    odlog(ERROR)<<"Failed to download "<<i->lfn<<std::endl;
    failure_reason+="Input file: "+i->lfn+" - "+(std::string)(i->res)+"\n";
    if(i->res == Arc::DataStatus::CredentialsExpiredError)
      credentials_expired=true;
    transfered=false;
  };
  // Check if all files have been properly downloaded
  if(!transfered) {
    odlog(INFO)<<"Some downloads failed"<<std::endl; res=2;
    if(credentials_expired) res=3;
    goto exit;
  };
  job_files_.clear();
  for(FileDataEx::iterator i = job_files.begin();i!=job_files.end();++i) job_files_.push_back(*i);
  if(!job_input_write_file(desc,user,job_files_)) {
    olog << "WARNING: Failed writing changed input file." << std::endl;
  };
  // check for user uploadable files
  // run cycle waiting for uploaded files
  for(;;) {
    not_uploaded=false;
    for(FileDataEx::iterator i=job_files.begin();i!=job_files.end();) {
      if(i->lfn.find(":") == std::string::npos) { /* is it lfn ? */
        /* process user uploadable file */
        olog<<"Check user uploadable file: "<<i->pfn<<std::endl;
        std::string error;
        int err=user_file_exists(*i,session_dir,&error);
        if(err == 0) { /* file is uploaded */
          olog<<"User has uploaded file"<<std::endl;
          i=job_files.erase(i);
          job_files_.clear();
          for(FileDataEx::iterator i = job_files.begin();i!=job_files.end();++i) job_files_.push_back(*i);
          if(!job_input_write_file(desc,user,job_files_)) {
            olog << "WARNING: Failed writing changed input file." << std::endl;
          };
        }
        else if(err == 1) { /* critical failure */
          olog<<"Critical error for uploadable file"<<std::endl;
          failure_reason+="User file: "+i->pfn+" - "+error+"\n";
          res=1; goto exit;
        }
        else {
          not_uploaded=true; ++i;
        };
      }
      else {
        ++i;
      };
    };
    if(!not_uploaded) break;
    // check for timeout
    if((time(NULL)-start_time) > upload_timeout) {
      for(FileDataEx::iterator i=job_files.begin();i!=job_files.end();++i) {
        if(i->lfn.find(":") == std::string::npos) { /* is it lfn ? */
          failure_reason+="User file: "+i->pfn+" - Timeout waiting\n";
        };
      };
      olog<<"Uploadable files timed out"<<std::endl; res=2; break;
    };
    sleep(CHECK_PERIOD);
  };
  job_files_.clear();
  for(FileDataEx::iterator i = job_files.begin();i!=job_files.end();++i) job_files_.push_back(*i);
  if(!job_input_write_file(desc,user,job_files_)) {
    olog << "WARNING: Failed writing changed input file." << std::endl;
  };

  // Job migration functionality
  if (res == 0) {
    if(desc.GetLocalDescription(user) &&
       (desc.get_local()->migrateactivityid != "")) {
    // Complete the migration.
      const size_t found = desc.get_local()->migrateactivityid.rfind("/");

      if (found != std::string::npos) {
        Arc::Job job;
        job.Flavour = "ARC1";
        job.JobID = Arc::URL(desc.get_local()->migrateactivityid);
        job.Cluster = Arc::URL(desc.get_local()->migrateactivityid.substr(0, found));

        Arc::UserConfig cfg(true);

        Arc::ACCLoader loader;
	Arc::JobController *jobctrl = dynamic_cast<Arc::JobController*>(loader.loadACC("JobControllerARC1", &cfg.ConfTree()));
        if (jobctrl) {
          jobctrl->FillJobStore(job);

          std::list<std::string> status;
          status.push_back("Running/Executing/Queuing");
          
          if (!jobctrl->Kill(status, true) && !desc.get_local()->forcemigration) {
            res = 1;
            failure_reason = "FATAL ERROR: Migration failed attempting to kill old job \"" + desc.get_local()->migrateactivityid + "\".";
          }
        }
        else {
          res = 1;
          failure_reason = "FATAL ERROR: Could not locate ARC1 JobController plugin. Maybe it is not installed?";
        }
      }
    }
  }

exit:
  olog << "Leaving downloader ("<<res<<")"<<std::endl;
  // clean unfinished files here 
  job_files_.clear();
  for(FileDataEx::iterator i = job_files.begin();i!=job_files.end();++i) job_files_.push_back(*i);
  clean_files(job_files_,session_dir);
  // release cache just in case
  if(res != 0) {
    cache->Release();
  };
  remove_proxy();
  if(res != 0 && res != 4) {
    job_failed_mark_add(desc,user,failure_reason);
  };
  return res;
}
