// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sstream>
#include <fstream>

#include <unistd.h>

#include <arc/DateTime.h>
#include <arc/StringConv.h>
#include <arc/Utils.h>

#include <unistd.h>
#ifdef WIN32
#include <process.h>
#endif

#include "Logger.h"

#undef rootLogger

namespace Arc {

  #define DefaultLogLevel (DEBUG)

  static std::string list_to_domain(const std::list<std::string>& subdomains) {
    std::string domain;
    for(std::list<std::string>::const_iterator subdomain = subdomains.begin();
        subdomain != subdomains.end();++subdomain) {
      domain += "."+(*subdomain);
    }
    return domain;
  }

  std::ostream& operator<<(std::ostream& os, LogLevel level) {
    if (level == DEBUG)
      os << "DEBUG";
    else if (level == VERBOSE)
      os << "VERBOSE";
    else if (level == INFO)
      os << "INFO";
    else if (level == WARNING)
      os << "WARNING";
    else if (level == ERROR)
      os << "ERROR";
    else if (level == FATAL)
      os << "FATAL";
    // There should be no more alternative!
    return os;
  }

  LogLevel string_to_level(const std::string& str) {
    if (str == "DEBUG")
      return DEBUG;
    else if (str == "VERBOSE")
      return VERBOSE;
    else if (str == "INFO")
      return INFO;
    else if (str == "WARNING")
      return WARNING;
    else if (str == "ERROR")
      return ERROR;
    else if (str == "FATAL")
      return FATAL;
    else { // should not happen...
      Logger::getRootLogger().msg(WARNING, "Invalid log level. Using default "+level_to_string(DefaultLogLevel)+".");
      return DefaultLogLevel;
    }
  }

  bool string_to_level(const std::string& str, LogLevel& ll) {
    if (str == "DEBUG")
      ll = DEBUG;
    else if (str == "VERBOSE")
      ll = VERBOSE;
    else if (str == "INFO")
      ll = INFO;
    else if (str == "WARNING")
      ll = WARNING;
    else if (str == "ERROR")
      ll = ERROR;
    else if (str == "FATAL")
      ll = FATAL;
    else  // should not happen...
      return false;

    return true;
  }

  bool istring_to_level(const std::string& llStr, LogLevel& ll) {
    const std::string str = upper(llStr);
    if (str == "DEBUG")
      ll = DEBUG;
    else if (str == "VERBOSE")
      ll = VERBOSE;
    else if (str == "INFO")
      ll = INFO;
    else if (str == "WARNING")
      ll = WARNING;
    else if (str == "ERROR")
      ll = ERROR;
    else if (str == "FATAL")
      ll = FATAL;
    else
      return false;

    return true;
  }

  std::string level_to_string(const LogLevel& level) {
    switch (level) {
      case DEBUG:
        return "DEBUG";
      case VERBOSE:
        return "VERBOSE";
      case INFO:
        return "INFO";
      case WARNING:
        return "WARNING";
      case ERROR:
        return "ERROR";
      case FATAL:
        return "FATAL";
      default:  // should not happen...
        return "";
    }
  }

  LogLevel old_level_to_level(unsigned int old_level) {
    if (old_level >= 5)
      return DEBUG;
    else if (old_level == 4)
      return VERBOSE;
    else if (old_level == 3)
      return INFO;
    else if (old_level == 2)
      return WARNING;
    else if (old_level == 1)
      return ERROR;
    else if (old_level == 0)
      return FATAL;
    else { // cannot happen...
      Logger::getRootLogger().msg(WARNING, "Invalid old log level. Using default "+level_to_string(DefaultLogLevel)+".");
      return DefaultLogLevel;
    }
  }

  LogMessage::LogMessage(LogLevel level,
                         const IString& message)
    : time(TimeStamp()),
      level(level),
      domain("---"),
      identifier(getDefaultIdentifier()),
      message(message) {}

  LogMessage::LogMessage(LogLevel level,
                         const IString& message,
                         const std::string& identifier)
    : time(TimeStamp()),
      level(level),
      domain("---"),
      identifier(identifier),
      message(message) {}

  LogLevel LogMessage::getLevel() const {
    return level;
  }

  void LogMessage::setIdentifier(std::string identifier) {
    this->identifier = identifier;
  }

  std::string LogMessage::getDefaultIdentifier() {
    std::ostringstream sout;
#ifdef HAVE_GETPID
    sout << getpid() << "/"
#ifdef WIN32
         << (unsigned long int)GetCurrentThreadId();
#else
         << (unsigned long int)(void*)Glib::Thread::self();
#endif
#else
#ifdef WIN32
    sout << (unsigned long int)GetCurrentThreadId();
#else
    sout << (unsigned long int)(void*)Glib::Thread::self();
#endif
#endif
    return sout.str();
  }

  void LogMessage::setDomain(std::string domain) {
    this->domain = domain;
  }

  static const int formatindex = std::ios_base::xalloc();

  std::ostream& operator<<(std::ostream& os, const LoggerFormat& format) {
    os.iword(formatindex) = format.format;
    return os;
  }

  std::ostream& operator<<(std::ostream& os, const LogMessage& message) {
    switch (os.iword(formatindex)) {
    case LongFormat:
      os << "[" << message.time << "] "
         << "[" << message.domain << "] "
         << "[" << message.level << "] "
         << "[" << message.identifier << "] "
         << message.message;
      break;
    case ShortFormat:
      os << message.level << ": " << message.message;
      break;
    case EmptyFormat:
      os << message.message;
      break;
    case DebugFormat:
      Time ct;
      static Time lt(0);
      os << "[" << ct.GetTime() << "." << std::setfill('0') << std::setw(6) << ct.GetTimeNanosec()/1000 << std::setfill(' ') << std::setw(0);
      if(lt.GetTime()) {
        Period d = ct - lt;
        os << "(" << d.GetPeriod()*1000000+d.GetPeriodNanoseconds()/1000<<")";
      };
      lt = ct;
      os << "] " << message.message;
      break;
    }
    return os;
  }

  LogDestination::LogDestination()
    : format(LongFormat) {}

  LogDestination::LogDestination(const std::string& locale)
    : locale(locale),
      format(LongFormat) {}

  void LogDestination::setFormat(const LogFormat& newformat) {
    format = newformat;
  }

  LogStream::LogStream(std::ostream& destination)
    : destination(destination) {}

  LogStream::LogStream(std::ostream& destination,
                       const std::string& locale)
    : LogDestination(locale),
      destination(destination) {}

  void LogStream::log(const LogMessage& message) {
    Glib::Mutex::Lock lock(mutex);
    EnvLockWrap(true); // Protecting any setenv/getenv we do not know about
    const char *loc = NULL;
    if (!locale.empty()) {
      loc = setlocale(LC_ALL, NULL);
      setlocale(LC_ALL, locale.c_str());
    }
    destination << LoggerFormat(format) << message << std::endl;
    if (!locale.empty()) setlocale(LC_ALL, loc);
    EnvLockUnwrap(true);
  }

  LogFile::LogFile(const std::string& path)
    : LogDestination(),
      path(path),
      destination(),
      maxsize(-1),
      backups(-1),
      reopen(false) {
    if(path.empty()) {
      //logger.msg(Arc::ERROR,"Log file path is not specified");
      return;
    }
    destination.open(path.c_str(), std::fstream::out | std::fstream::app);
    if(!destination.is_open()) {
      //logger.msg(Arc::ERROR,"Failed to open log file: %s",path);
      return;
    }
  }

  LogFile::LogFile(const std::string& path, const std::string& locale)
    : LogDestination(locale),
      path(path),
      destination(),
      maxsize(-1),
      backups(-1),
      reopen(false) {
    if(path.empty()) {
      //logger.msg(Arc::ERROR,"Log file path is not specified");
      return;
    }
    destination.open(path.c_str(), std::fstream::out | std::fstream::app);
    if(!destination.is_open()) {
      //logger.msg(Arc::ERROR,"Failed to open log file: %s",path);
      return;
    }
  }
  void LogFile::setMaxSize(int newsize) {
    maxsize = newsize;
  }

  void LogFile::setBackups(int newbackups) {
    backups = newbackups;
  }
  void LogFile::setReopen(bool newreopen) {
    reopen = newreopen;
    if(reopen) {
      destination.close();
    } else {
      if(!destination.is_open()) {
        destination.open(path.c_str(), std::fstream::out | std::fstream::app);
      }
    }
  }

  LogFile::operator bool(void) {
    Glib::Mutex::Lock lock(mutex);
    return (reopen)?(!path.empty()):destination.is_open();
  }

  bool LogFile::operator!(void) {
    Glib::Mutex::Lock lock(mutex);
    return (reopen)?path.empty():(!destination.is_open());
  }

  void LogFile::log(const LogMessage& message) {
    Glib::Mutex::Lock lock(mutex);
    const char *loc = NULL;
    // If requested to reopen on every write or if was closed because of error
    if (reopen || !destination.is_open()) {
      destination.open(path.c_str(), std::fstream::out | std::fstream::app);
    }
    if(!destination.is_open()) return;
    EnvLockWrap(true); // Protecting any setenv/getenv we do not know about
    if (!locale.empty()) {
      loc = setlocale(LC_ALL, NULL);
      setlocale(LC_ALL, locale.c_str());
    }
    destination << LoggerFormat(format) << message << std::endl;
    if (!locale.empty()) setlocale(LC_ALL, loc);
    EnvLockUnwrap(true);
    // Check if unrecoverwable error occured. Close if error 
    // and reopen on next write.
    if(destination.bad()) destination.close();
    // Before closing check if must backup
    backup();
    if (reopen) destination.close();
  }

  void LogFile::backup(void) {
    if(maxsize <= 0) return;
    if(destination.tellp() < maxsize) return;
    bool backup_done = true;
    // Not sure if this will work on windows, but glibmm
    // has no functions for removing and renaming files
    if(backups > 0) {
      std::string backup_path = path+"."+tostring(backups);
      ::unlink(backup_path.c_str());
      for(int n = backups;n>0;--n) {
        std::string old_backup_path = (n>1)?(path+"."+tostring(n-1)):path;
        if(::rename(old_backup_path.c_str(),backup_path.c_str()) != 0) {
          if(n == 1) backup_done=false;
        }
        backup_path = old_backup_path;
      }
    } else {
      if(::unlink(path.c_str()) != 0) backup_done=false;
    }
    if((backup_done) && (!reopen)) {
      destination.close();
      destination.open(path.c_str(), std::fstream::out | std::fstream::app);
    }
  }

  class LoggerContextRef: public ThreadDataItem {
    friend class Logger;
    private:
      std::string id;
      LoggerContext& context;
      LoggerContextRef(LoggerContext& ctx, std::string& i);
      virtual ~LoggerContextRef(void);
    public:
      virtual void Dup(void);
  };

  LoggerContextRef::LoggerContextRef(LoggerContext& ctx, std::string& i):
                                               ThreadDataItem(i),context(ctx) {
    id = i;
    context.Acquire();
  }

  LoggerContextRef::~LoggerContextRef(void) {
    context.Release();
  }

  void LoggerContextRef::Dup(void) {
    new LoggerContextRef(context,id);
  }

  void LoggerContext::Acquire(void) {
    mutex.lock();
    ++usage_count;
    mutex.unlock();
  }

  void LoggerContext::Release(void) {
    mutex.lock();
    --usage_count;
    if(!usage_count) {
      delete this;
    } else {
      mutex.unlock();
    }
  }

  LoggerContext::~LoggerContext(void) {
    mutex.trylock();
    mutex.unlock();
  }

  Logger* Logger::rootLogger = NULL;
  std::map<std::string,LogLevel>* Logger::defaultThresholds = NULL;
  unsigned int Logger::rootLoggerMark = ~rootLoggerMagic;

  Logger& Logger::getRootLogger(void) {
    if ((rootLogger == NULL) || (rootLoggerMark != rootLoggerMagic)) {
      rootLogger = new Logger();
      defaultThresholds = new std::map<std::string,LogLevel>;
      rootLoggerMark = rootLoggerMagic;
    }
    return *rootLogger;
  }

  Logger::Logger(Logger& parent,
                 const std::string& subdomain)
    : parent(&parent),
      domain(parent.getDomain() + "." + subdomain),
      context((LogLevel)0) {
    std::map<std::string,LogLevel>::const_iterator thr =
                                   defaultThresholds->find(domain);
    if(thr != defaultThresholds->end()) {
      context.threshold = thr->second;
    }
  }

  Logger::Logger(Logger& parent,
                 const std::string& subdomain,
                 LogLevel threshold)
    : parent(&parent),
      domain(parent.getDomain() + "." + subdomain),
      context(threshold) {
  }

  Logger::~Logger() {
  }

  void Logger::addDestination(LogDestination& destination) {
    Glib::Mutex::Lock lock(mutex);
    getContext().destinations.push_back(&destination);
  }

  void Logger::addDestinations(const std::list<LogDestination*>& destinations) {
    Glib::Mutex::Lock lock(mutex);
    for(std::list<LogDestination*>::const_iterator dest = destinations.begin();
                            dest != destinations.end();++dest) {
      getContext().destinations.push_back(*dest);
    }
  }

  const std::list<LogDestination*>& Logger::getDestinations(void) const {
    Glib::Mutex::Lock lock((Glib::Mutex&)mutex);
    return ((Logger*)this)->getContext().destinations;
  }

  void Logger::removeDestinations(void) {
    Glib::Mutex::Lock lock(mutex);
    getContext().destinations.clear();
  }

  void Logger::deleteDestinations(void) {
    Glib::Mutex::Lock lock(mutex);
    std::list<LogDestination*>& destinations = getContext().destinations;
    for(std::list<LogDestination*>::iterator dest = destinations.begin();
                            dest != destinations.end();) {
      delete *dest;
      *dest = NULL;
      dest = destinations.erase(dest);
    }
  }

  void Logger::setThreshold(LogLevel threshold) {
    Glib::Mutex::Lock lock(mutex);
    this->getContext().threshold = threshold;
  }

  void Logger::setThresholdForDomain(LogLevel threshold,
                                     const std::list<std::string>& subdomains) {
    setThresholdForDomain(threshold, list_to_domain(subdomains));
  }

  void Logger::setThresholdForDomain(LogLevel threshold,
                                     const std::string& domain) {
    getRootLogger();
    if(domain.empty() || (domain == "Arc")) {
      getRootLogger().setThreshold(threshold);
    } else {
      (*defaultThresholds)[domain] = threshold;
    }
  }

  LogLevel Logger::getThreshold() const {
    Glib::Mutex::Lock lock((Glib::Mutex&)mutex);
    const LoggerContext& ctx = ((Logger*)this)->getContext();
    if(ctx.threshold != (LogLevel)0) return ctx.threshold;
    if(parent) return parent->getThreshold();
    return (LogLevel)0;
  }

  void Logger::setThreadContext(void) {
    Glib::Mutex::Lock lock(mutex);
    LoggerContext* nctx = new LoggerContext(getContext());
    new LoggerContextRef(*nctx,context_id);
  }

  LoggerContext& Logger::getContext(void) {
    if(context_id.empty()) return context;
    try {
      ThreadDataItem* item = ThreadDataItem::Get(context_id);
      if(!item) return context;
      LoggerContextRef* citem = dynamic_cast<LoggerContextRef*>(item);
      if(!citem) return context;
      return citem->context;
    } catch(std::exception&) {
    };
    return context;
    
  }

  void Logger::msg(LogMessage message) {
    message.setDomain(domain);
    if (message.getLevel() >= getThreshold()) {
      log(message);
    }
  }

  Logger::Logger()
    : parent(0),
      domain("Arc"),
      context(DefaultLogLevel) {
    // addDestination(cerr);
  }

  std::string Logger::getDomain() {
    return domain;
  }

  void Logger::log(const LogMessage& message) {
    Glib::Mutex::Lock lock(mutex);
    LoggerContext& ctx = getContext();
    std::list<LogDestination*>::iterator dest;
    std::list<LogDestination*>::iterator begin = ctx.destinations.begin();
    std::list<LogDestination*>::iterator end = ctx.destinations.end();
    for (dest = begin; dest != end; ++dest)
      (*dest)->log(message);
    if (parent)
      parent->log(message);
  }

}
