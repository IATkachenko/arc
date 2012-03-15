#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/ArcLocation.h>
#include <arc/loader/ModuleManager.h>

namespace Arc {
  Logger ModuleManager::logger(Logger::rootLogger, "ModuleManager");

static std::string strip_newline(const std::string& str) {
  std::string s(str);
  std::string::size_type p = 0;
  while((p=s.find('\r',p)) != std::string::npos) s[p]=' ';
  p=0;
  while((p=s.find('\n',p)) != std::string::npos) s[p]=' ';
  return s;
}

ModuleManager::ModuleManager(XMLNode cfg)
{
  if(!cfg) return;
  ModuleManager::logger.msg(DEBUG, "Module Manager Init");
  if(!MatchXMLName(cfg,"ArcConfig")) return;
  XMLNode mm = cfg["ModuleManager"];
  for (int n = 0;;++n) {
    XMLNode path = mm.Child(n);
    if (!path) {
      break;
    }
    if (MatchXMLName(path, "Path")) {
      plugin_dir.push_back((std::string)path);
    }
  }
  if (plugin_dir.empty()) {
    plugin_dir = ArcLocation::GetPlugins();
  }
}

ModuleManager::~ModuleManager(void)
{
  Glib::Mutex::Lock lock(mlock);
  // Try to unload all modules
  // Remove unloaded plugins from cache
  for(plugin_cache_t::iterator i = plugin_cache.begin(); i != plugin_cache.end();) {
    while(i->second.unload() > 0) { };
    if(i->second) {
      // module is on unloaded only if it is in use according to usage counter
std::cerr<<">>>>> BUSY PLUGIN found: "<<i->second.usage()<<std::endl;
      ++i;
    } else {
      plugin_cache.erase(i);
      i = plugin_cache.begin(); // for map erase does not return iterator
    }
  }
  // exit only when all plugins unloaded
  if(plugin_cache.empty()) return;
std::cerr<<">>>>> BUSY PLUGINS found: "<<plugin_cache.size()<<std::endl;
  // otherwise wait for plugins to be released
  logger.msg(WARNING, "Busy plugins found while unloading Module Manager. Waiting for them to be released.");
  for(;;) {
    // wait for plugins to be released
    lock.release();
    sleep(1);
    lock.acquire();
    // Check again
    // Just in case something called load() - unloading them again
    for(plugin_cache_t::iterator i = plugin_cache.begin(); i != plugin_cache.end();) {
      while(i->second.unload() > 0) { };
      if(i->second) {
std::cerr<<">>>>> BUSY PLUGIN still here: "<<i->second.usage()<<std::endl;
        ++i;
      } else {
        plugin_cache.erase(i);
        i = plugin_cache.begin(); // for map erase does not return iterator
      }
    }
    if(plugin_cache.empty()) return;
  };
}

std::string ModuleManager::findLocation(const std::string& name)
{
  Glib::Mutex::Lock lock(mlock);
  std::string path;
  std::list<std::string>::const_iterator i = plugin_dir.begin();
  for (; i != plugin_dir.end(); i++) {
    path = Glib::Module::build_path(*i, name);
    // Loader::logger.msg(VERBOSE, "Try load %s", path);
    FILE *file = fopen(path.c_str(), "r");
    if (file == NULL) {
      continue;
    } else {
      fclose(file);
      break;
    }
  }
  if(i == plugin_dir.end()) path="";
  return path;
}

void ModuleManager::unload(Glib::Module *module)
{
  Glib::Mutex::Lock lock(mlock);
  for(plugin_cache_t::iterator p = plugin_cache.begin();
                               p!=plugin_cache.end();++p) {
    if(p->second == module) {
      p->second.unload();
      if(!(p->second)) plugin_cache.erase(p);
      break;
    }
  }
}

void ModuleManager::unload(const std::string& name)
{
  Glib::Mutex::Lock lock(mlock);
  plugin_cache_t::iterator p = plugin_cache.find(name);
  if (p != plugin_cache.end()) {
    p->second.unload();
    if(!(p->second)) plugin_cache.erase(p);
  }
}

void ModuleManager::use(Glib::Module *module)
{
  Glib::Mutex::Lock lock(mlock);
  for(plugin_cache_t::iterator p = plugin_cache.begin();
                               p!=plugin_cache.end();++p) {
    if(p->second == module) {
      p->second.use();
      break;
    }
  }
}

void ModuleManager::unuse(Glib::Module *module)
{
  Glib::Mutex::Lock lock(mlock);
  for(plugin_cache_t::iterator p = plugin_cache.begin();
                               p!=plugin_cache.end();++p) {
    if(p->second == module) {
      p->second.unuse();
      if(!(p->second)) plugin_cache.erase(p);
      break;
    }
  }
}

std::string ModuleManager::find(const std::string& name)
{
  return findLocation(name);
}

Glib::Module* ModuleManager::load(const std::string& name,bool probe)
{
  if (!Glib::Module::get_supported()) {
    return NULL;
  }
  // find name in plugin_cache
  {
    Glib::Mutex::Lock lock(mlock);
    plugin_cache_t::iterator p = plugin_cache.find(name);
    if (p != plugin_cache.end()) {
      ModuleManager::logger.msg(DEBUG, "Found %s in cache", name);
      p->second.load();
      return static_cast<Glib::Module*>(p->second);
    }
  }
  std::string path = findLocation(name);
  if(path.empty()) {
    ModuleManager::logger.msg(VERBOSE, "Could not locate module %s in following paths:", name);
    Glib::Mutex::Lock lock(mlock);
    std::list<std::string>::const_iterator i = plugin_dir.begin();
    for (; i != plugin_dir.end(); i++) {
      ModuleManager::logger.msg(VERBOSE, "\t%s", *i);
    }
    return NULL;
  };
  // race!
  Glib::Mutex::Lock lock(mlock);
  Glib::ModuleFlags flags = Glib::ModuleFlags(0);
  if(probe) flags|=Glib::MODULE_BIND_LAZY;
  Glib::Module *module = new Glib::Module(path,flags);
  if ((!module) || (!(*module))) {
    ModuleManager::logger.msg(ERROR, strip_newline(Glib::Module::get_last_error()));
    if(module) delete module;
    return NULL;
  }
  ModuleManager::logger.msg(DEBUG, "Loaded %s", path);
  (plugin_cache[name] = module).load();
  return module;
}

Glib::Module* ModuleManager::reload(Glib::Module* omodule)
{
  Glib::Mutex::Lock lock(mlock);
  plugin_cache_t::iterator p = plugin_cache.begin();
  for(;p!=plugin_cache.end();++p) {
    if(p->second == omodule) break;
  }
  if(p==plugin_cache.end()) return NULL;
  Glib::ModuleFlags flags = Glib::ModuleFlags(0);
  //flags|=Glib::MODULE_BIND_LOCAL;
  Glib::Module *module = new Glib::Module(omodule->get_name(),flags);
  if ((!module) || (!(*module))) {
    ModuleManager::logger.msg(ERROR, strip_newline(Glib::Module::get_last_error()));
    if(module) delete module;
    return NULL;
  }
  p->second=module;
  delete omodule;
  return module;
}

void ModuleManager::setCfg (XMLNode cfg) {
  if(!cfg) return;
  ModuleManager::logger.msg(DEBUG, "Module Manager Init by ModuleManager::setCfg");

  if(!MatchXMLName(cfg,"ArcConfig")) return;
  XMLNode mm = cfg["ModuleManager"];
  for (int n = 0;;++n) {
    XMLNode path = mm.Child(n);
    if (!path) {
      break;
    }
    if (MatchXMLName(path, "Path")) {
      Glib::Mutex::Lock lock(mlock);
      //std::cout<<"Size:"<<plugin_dir.size()<<"plugin cache size:"<<plugin_cache.size()<<std::endl;
      std::list<std::string>::const_iterator it;
      for( it = plugin_dir.begin(); it != plugin_dir.end(); it++){
        //std::cout<<(std::string)path<<"*********"<<(*it)<<std::endl;
        if(((*it).compare((std::string)path)) == 0)break;
      }
      if(it == plugin_dir.end()) plugin_dir.push_back((std::string)path);
    }
  }
  Glib::Mutex::Lock lock(mlock);
  if (plugin_dir.empty()) {
    plugin_dir = ArcLocation::GetPlugins();
  }
}

bool ModuleManager::makePersistent(const std::string& name) {
  if (!Glib::Module::get_supported()) {
    return false;
  }
  // find name in plugin_cache
  {
    Glib::Mutex::Lock lock(mlock);
    plugin_cache_t::iterator p = plugin_cache.find(name);
    if (p != plugin_cache.end()) {
      p->second.makePersistent();
      ModuleManager::logger.msg(DEBUG, "%s made persistent", name);
      return true;
    }
  }
  ModuleManager::logger.msg(DEBUG, "Not found %s in cache", name);
  return false;
}

bool ModuleManager::makePersistent(Glib::Module* module) {
  Glib::Mutex::Lock lock(mlock);
  for(plugin_cache_t::iterator p = plugin_cache.begin();
                               p!=plugin_cache.end();++p) {
    if(p->second == module) {
      ModuleManager::logger.msg(DEBUG, "%s made persistent", p->first);
      p->second.makePersistent();
      return true;
    }
  }
  ModuleManager::logger.msg(DEBUG, "Specified module not found in cache");
  return false;
}

} // namespace Arc

