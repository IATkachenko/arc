// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>
#include <iostream>
#include <string>

#include <glibmm/miscutils.h>

#include <arc/ArcConfig.h>
#include <arc/ArcLocation.h>
#include <arc/User.h>

namespace Arc {

  Config::Config(const char *filename)
    : file_name_(filename) {
    ReadFromFile(filename);
  }

  Config::~Config(void) {
    // NOP
  }

  static void _print(XMLNode& node, int skip) {
    int n;
    for (n = 0; n < skip; n++)
      std::cout << " ";
    std::string content = (std::string)node;
    std::cout << "* " << node.Name() << "(" << node.Size() << ")"
              << " = " << content << std::endl;
    for (n = 0;; n++) {
      XMLNode _node = node.Child(n);
      if (!_node)
        break;
      _print(_node, skip + 2);
    }
  }

  void Config::print(void) {
    _print(*((XMLNode*)this), 0);
  }

  void Config::parse(const char *filename) {
    file_name_ = filename;
    ReadFromFile(filename);
  }

  Config::Config(long cfg_ptr_addr) {
    Config *cfg = (Config*)cfg_ptr_addr;
    cfg->New(*this);
  }

  Config::Config(const Config& cfg)
    : XMLNode() {
    cfg.New(*this);
  }

  void Config::save(const char *filename) {
    std::string str;
    GetDoc(str);
    std::ofstream out(filename);
    out << str;
    out.close();
  }

  BaseConfig::BaseConfig() {
    if (getenv("ARC_PLUGIN_PATH")) {
      std::string arcpluginpath = getenv("ARC_PLUGIN_PATH");
      std::string::size_type pos = 0;
      while (pos != std::string::npos) {
        std::string::size_type pos2 = arcpluginpath.find(G_SEARCHPATH_SEPARATOR, pos);
        AddPluginsPath(pos2 == std::string::npos ?
                       arcpluginpath.substr(pos) :
                       arcpluginpath.substr(pos, pos2 - pos));
        pos = pos2;
        if (pos != std::string::npos)
          pos++;
      }
    }
    else
#ifdef WIN32
      AddPluginsPath(ArcLocation::Get() + "\\lib\\arc");
#else
      AddPluginsPath(ArcLocation::Get() + G_DIR_SEPARATOR_S + PKGLIBSUBDIR);
#endif
  }

  void BaseConfig::AddPluginsPath(const std::string& path) {
    plugin_paths.push_back(path);
  }

  XMLNode BaseConfig::MakeConfig(XMLNode cfg) const {
    XMLNode mm = cfg.NewChild("ModuleManager");
    for (std::list<std::string>::const_iterator p = plugin_paths.begin();
         p != plugin_paths.end(); ++p)
      mm.NewChild("Path") = *p;
    return mm;
  }

  void BaseConfig::AddPrivateKey(const std::string& path) {
    key = path;
  }

  void BaseConfig::AddCertificate(const std::string& path) {
    cert = path;
  }

  void BaseConfig::AddProxy(const std::string& path) {
    proxy = path;
  }

  void BaseConfig::AddCAFile(const std::string& path) {
    cafile = path;
  }

  void BaseConfig::AddCADir(const std::string& path) {
    cadir = path;
  }

  void BaseConfig::AddOverlay(XMLNode cfg) {
    overlay.Destroy();
    cfg.New(overlay);
  }

  void BaseConfig::GetOverlay(std::string fname) {
    overlay.Destroy();
    if (fname.empty())
      return;
    overlay.ReadFromFile(fname);
  }

} // namespace Arc
