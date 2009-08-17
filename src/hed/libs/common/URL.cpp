// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>

#include <unistd.h>

#include <glibmm/miscutils.h>

#ifdef WIN32
#include "win32.h"
#endif

#include <arc/Logger.h>
#include <arc/StringConv.h>
#include <arc/URL.h>


namespace Arc {

  static Logger URLLogger(Logger::getRootLogger(), "URL");

  static std::map<std::string, std::string> ParseOptions(const std::string& optstring, char separator) {

    std::map<std::string, std::string> options;

    if (optstring.empty())
      return options;

    std::string::size_type pos = 0;
    while (pos != std::string::npos) {

      std::string::size_type pos2 = optstring.find(separator, pos);

      std::string opt = (pos2 == std::string::npos ?
                         optstring.substr(pos) :
                         optstring.substr(pos, pos2 - pos));

      pos = pos2;
      if (pos != std::string::npos)
        pos++;

      pos2 = opt.find('=');
      if (pos2 == std::string::npos)
        options[opt] = "";
      else
        options[opt.substr(0, pos2)] = opt.substr(pos2 + 1);
    }
    return options;
  }

  static std::list<std::string> ParseAttributes(const std::string& attrstring, char separator) {

    std::list<std::string> attributes;

    if (attrstring.empty())
      return attributes;

    std::string::size_type pos = 0;
    while (pos != std::string::npos) {

      std::string::size_type pos2 = attrstring.find(separator, pos);

      std::string attr = (pos2 == std::string::npos ?
                          attrstring.substr(pos) :
                          attrstring.substr(pos, pos2 - pos));

      pos = pos2;
      if (pos != std::string::npos)
        pos++;

      attributes.push_back(attr);
    }
    return attributes;
  }


  static std::string AttributeString(const std::list<std::string> attributes,
                                     char separator) {

    std::string attrstring;

    if (attributes.empty())
      return attrstring;

    for (std::list<std::string>::const_iterator it = attributes.begin();
         it != attributes.end(); it++) {
      if (it != attributes.begin())
        attrstring += separator;
      attrstring += *it;
    }
    return attrstring;
  }


  URL::URL()
    : port(-1),
      ldapscope(base) {}

  URL::URL(const std::string& url)
    : port(-1),
      ldapscope(base) {

    std::string::size_type pos, pos2, pos3;

    if (url[0] == '\0')
      return;

    if (url[0] == '#') {
      URLLogger.msg(ERROR, "URL is not valid: %s", url);
      return;
    }

    // Looking for protocol separator
    pos = url.find(":");
    if (pos != std::string::npos) {
      // Check if protocol looks like protocol
      for(std::string::size_type p = 0; p < pos; ++p) {
        char c = url[p];
        if(isalnum(c) || (c == '+') || (c == '-') || (c == '.')) continue;
        pos = std::string::npos;
        break;
      }
    }
    if (pos == std::string::npos) {
      // URL does not start from protocol - must be simple path
      if (url[0] == '@') {
        protocol = "urllist";
        path = url.substr(1);
      }
      else {
        protocol = "file";
        path = url;
      }
      if (!Glib::path_is_absolute(path)) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, PATH_MAX))
          path = Glib::build_filename(cwd, path);
      }
      // Simple paths are not expected to contain any options or metadata
      return;
    }

    // RFC says protocols should be lowercase and uppercase 
    // must be converted to lowercase for consistency
    protocol = lower(url.substr(0, pos));

    // Checking if protocol followed by host/authority part 
    // or by path directly
    if((url[pos+1] != '/') || (url[pos+2] != '/')) {
      host = "";
      pos += 1;
      pos2 = pos; // path start position
      path = url.substr(pos2);
      // This must be only path - we can accept path only for 
      // limited set of protocols
      if ((protocol == "file" || protocol == "urllist")) {
        if (!Glib::path_is_absolute(path)) {
          char cwd[PATH_MAX];
          if (getcwd(cwd, PATH_MAX))
            path = Glib::build_filename(cwd, path);
        }
        return;
      } else if (protocol == "arc") {
        // TODO: It is not defined how arc protocol discovers 
        // entry point in general case.
        // For same reason let's assume path must be always
        // absolute.
        if(url[pos] != '/') {
          URLLogger.msg(ERROR, "Illegal URL - path must be absolute");
          return;
        }
      } else {
        URLLogger.msg(ERROR, "Illegal URL - no hostname given");
        return;
      }
    } else {
      // There is host/authority part in this URL. That also 
      // means path is absolute if present
      pos += 3;

      pos2 = url.find("@", pos);
      if (pos2 != std::string::npos) {

        if (protocol == "rc" || protocol == "rls" ||
            protocol == "fireman" || protocol == "lfc") {
          // Indexing protocols may contain locations

          std::string locstring = url.substr(pos, pos2 - pos);
          pos = pos2 + 1;

          pos2 = 0;
          while (pos2 != std::string::npos) {

            pos3 = locstring.find('|', pos2);
            std::string loc = (pos3 == std::string::npos ?
                               locstring.substr(pos2) :
                               locstring.substr(pos2, pos3 - pos2));

            pos2 = pos3;
            if (pos2 != std::string::npos)
              pos2++;

            if (loc[0] == ';')
              commonlocoptions = ParseOptions(loc.substr(1), ';');
            else {
              if (protocol == "rc") {
                pos3 = loc.find(';');
                if (pos3 == std::string::npos)
                  locations.push_back(URLLocation(ParseOptions("", ';'), loc));
                else
                  locations.push_back(URLLocation(ParseOptions
                                                  (loc.substr(pos3 + 1), ';'),
                                                  loc.substr(pos3 + 1)));
              }
              else
                locations.push_back(loc);
            }
          }
        }
        else {
          pos3 = url.find("/", pos);
          if (pos3 == std::string::npos)
            pos3 = url.length();
          if (pos3 > pos2) {
            username = url.substr(pos, pos2 - pos);
            pos3 = username.find(':');
            if (pos3 != std::string::npos) {
              passwd = username.substr(pos3 + 1);
              username.resize(pos3);
            }
            pos = pos2 + 1;
          }
        }
      }

      // Looking for end of host/authority part
      pos2 = url.find("/", pos);
      if (pos2 == std::string::npos) {
        // Path part is empty, host may be empty too
        host = url.substr(pos);
        path = "";
      }
      else if (pos2 == pos) {
        // Empty host and non-empty absolute path 
        host = "";
        path = url.substr(pos2);
      }
      else {
        // Both host and absolute path present
        host = url.substr(pos, pos2 - pos);
        path = url.substr(pos2);
      }
    }

    // At this point path must be absolutely absolute (starts with /) or empty
    if ((!path.empty()) && (path[0] != '/')) {
      URLLogger.msg(ERROR, "Illegal URL - path must be absolute or empty");
      return;
    }

    // Extracting port URL options (ARC extension)
    if (!host.empty()) {
      pos2 = host.find(':');
      if (pos2 != std::string::npos) {
        pos3 = host.find(';', pos2);
        port = stringtoi(pos3 == std::string::npos ?
                         host.substr(pos2 + 1) :
                         host.substr(pos2 + 1, pos3 - pos2 - 1));
      }
      else {
        pos3 = host.find(';');
        pos2 = pos3;
      }
      if (pos3 != std::string::npos)
        urloptions = ParseOptions(host.substr(pos3 + 1), ';');
      if (pos2 != std::string::npos)
        host.resize(pos2);
    }

    if (port == -1) {
      if (protocol == "rc")
        port = RC_DEFAULT_PORT;
      if (protocol == "rls")
        port = RLS_DEFAULT_PORT;
      if (protocol == "http")
        port = HTTP_DEFAULT_PORT;
      if (protocol == "https")
        port = HTTPS_DEFAULT_PORT;
      if (protocol == "httpg")
        port = HTTPG_DEFAULT_PORT;
      if (protocol == "srm")
        port = SRM_DEFAULT_PORT;
      if (protocol == "ldap")
        port = LDAP_DEFAULT_PORT;
      if (protocol == "ftp")
        port = FTP_DEFAULT_PORT;
      if (protocol == "gsiftp")
        port = GSIFTP_DEFAULT_PORT;
      if (protocol == "lfc")
        port = LFC_DEFAULT_PORT;
    }

    if (protocol != "ldap" && protocol != "arc") {
      pos = path.find(':');
      if (pos != std::string::npos) {
        metadataoptions = ParseOptions(path.substr(pos + 1), ':');
        path = path.substr(0, pos);
      }
    }

    // if protocol = http, get the options after the ?
    if (protocol == "http" ||
        protocol == "https" ||
        protocol == "httpg" ||
        protocol == "arc" ||
        protocol == "srm") {
      pos = path.find("?");
      if (pos != std::string::npos) {
        httpoptions = ParseOptions(path.substr(pos + 1), '&');
        path = path.substr(0, pos);
      }
    }

    // parse ldap protocol specific attributes
    if (protocol == "ldap") {
      std::string ldapscopestr;
      pos = path.find('?');
      if (pos != std::string::npos) {
        pos2 = path.find('?', pos + 1);
        if (pos2 != std::string::npos) {
          pos3 = path.find('?', pos2 + 1);
          if (pos3 != std::string::npos) {
            ldapfilter = path.substr(pos3 + 1);
            ldapscopestr = path.substr(pos2 + 1, pos3 - pos2 - 1);
          }
          else
            ldapscopestr = path.substr(pos2 + 1);
          ldapattributes = ParseAttributes(path.substr(pos + 1,
                                                       pos2 - pos - 1), ',');
        }
        else
          ldapattributes = ParseAttributes(path.substr(pos + 1), ',');
        path = path.substr(0, pos);
      }
      if (ldapscopestr == "base")
        ldapscope = base;
      else if (ldapscopestr == "one")
        ldapscope = onelevel;
      else if (ldapscopestr == "sub")
        ldapscope = subtree;
      else if (!ldapscopestr.empty())
        URLLogger.msg(ERROR, "Unknown LDAP scope %s - using base",
                      ldapscopestr);
      if (ldapfilter.empty())
        ldapfilter = "(objectClass=*)";
      if (path.find("/",1) != std::string::npos)
        path = Path2BaseDN(path);
      else
        path.erase(0,1);
    }

    // Normally host/authority names are case-insensitive
    host = lower(host);

  }

  URL::~URL() {}

  const std::string& URL::Protocol() const {
    return protocol;
  }

  void URL::ChangeProtocol(const std::string& newprot) {
    protocol = lower(newprot);
  }

  const std::string& URL::Username() const {
    return username;
  }

  const std::string& URL::Passwd() const {
    return passwd;
  }

  const std::string& URL::Host() const {
    return host;
  }

  void URL::ChangeHost(const std::string& newhost) {
    host = lower(newhost);
  }

  int URL::Port() const {
    return port;
  }

  void URL::ChangePort(int newport) {
    port = newport;
  }

  const std::string& URL::Path() const {
    return path;
  }

  std::string URL::FullPath() const {
    std::string fullpath;

    if (!path.empty())
      fullpath += path;

    if (!httpoptions.empty())
      fullpath += '?' + OptionString(httpoptions, '&');

    if (!ldapattributes.empty() || (ldapscope != base) || !ldapfilter.empty())
      fullpath += '?' + AttributeString(ldapattributes, ',');

    if ((ldapscope != base) || !ldapfilter.empty()) {
      switch (ldapscope) {
      case base:
        fullpath += "?base";
        break;

      case onelevel:
        fullpath += "?one";
        break;

      case subtree:
        fullpath += "?sub";
        break;
      }
    }

    if (!ldapfilter.empty())
      fullpath += '?' + ldapfilter;

    return fullpath;
  }

  void URL::ChangePath(const std::string& newpath) {
    path = newpath;

    // parse basedn in case of ldap-protocol
    if (protocol == "ldap") {
      if (path.find("/") != std::string::npos)
        path = Path2BaseDN(path);

    // add absolute path for relative file URLs
    } else if ((protocol == "file" || protocol == "urllist") && !Glib::path_is_absolute(path)) {
      char cwd[PATH_MAX];
      if (getcwd(cwd, PATH_MAX))
        path = Glib::build_filename(cwd, path);
    } else if ((path[0] != '/') && (!path.empty())) {
      URLLogger.msg(WARNING, "Attempt to assign relative path to URL - making it absolute");
      path = "/" + path;
    }
  }

  const std::map<std::string, std::string>& URL::HTTPOptions() const {
    return httpoptions;
  }

  const std::string& URL::HTTPOption(const std::string& option,
                                     const std::string& undefined) const {
    std::map<std::string, std::string>::const_iterator
    opt = httpoptions.find(option);
    if (opt != httpoptions.end())
      return opt->second;
    else
      return undefined;
  }

  const std::map<std::string, std::string>& URL::MetaDataOptions() const {
    return metadataoptions;
  }

  const std::string& URL::MetaDataOption(const std::string& option,
                                         const std::string& undefined) const {
    std::map<std::string, std::string>::const_iterator opt = metadataoptions.find(option);
    if (opt != metadataoptions.end())
      return opt->second;
    else
      return undefined;
  }

  const std::list<std::string>& URL::LDAPAttributes() const {
    return ldapattributes;
  }

  void URL::AddLDAPAttribute(const std::string& attribute) {
    ldapattributes.push_back(attribute);
  }

  URL::Scope URL::LDAPScope() const {
    return ldapscope;
  }

  void URL::ChangeLDAPScope(const Scope newscope) {
    ldapscope = newscope;
  }

  const std::string& URL::LDAPFilter() const {
    return ldapfilter;
  }

  void URL::ChangeLDAPFilter(const std::string& newfilter) {
    ldapfilter = newfilter;
  }

  const std::map<std::string, std::string>& URL::Options() const {
    return urloptions;
  }

  const std::string& URL::Option(const std::string& option,
                                 const std::string& undefined) const {
    std::map<std::string, std::string>::const_iterator
    opt = urloptions.find(option);
    if (opt != urloptions.end())
      return opt->second;
    else
      return undefined;
  }

  void URL::AddOption(const std::string& option, const std::string& value,
                      bool overwrite) {
    if (!overwrite && urloptions.find(option) != urloptions.end())
      return;
    urloptions[option] = value;
  }

  const std::list<URLLocation>& URL::Locations() const {
    return locations;
  }

  const std::map<std::string, std::string>& URL::CommonLocOptions() const {
    return commonlocoptions;
  }

  const std::string& URL::CommonLocOption(const std::string& option,
                                          const std::string& undefined) const {
    std::map<std::string, std::string>::const_iterator
    opt = commonlocoptions.find(option);
    if (opt != commonlocoptions.end())
      return opt->second;
    else
      return undefined;
  }

  std::string URL::fullstr() const {

    std::string urlstr;

    if (!username.empty())
      urlstr += username;

    if (!passwd.empty())
      urlstr += ':' + passwd;

    for (std::list<URLLocation>::const_iterator it = locations.begin();
         it != locations.end(); it++) {
      if (it != locations.begin())
        urlstr += '|';
      urlstr += it->fullstr();
    }

    if (!locations.empty() && !commonlocoptions.empty())
      urlstr += '|';

    if (!commonlocoptions.empty())
      urlstr += ';' + OptionString(commonlocoptions, ';');

    if (!username.empty() || !passwd.empty() || !locations.empty())
      urlstr += '@';

    if (!host.empty())
      urlstr += host;

    if (port != -1)
      urlstr += ':' + tostring(port);

    if (!urloptions.empty())
      urlstr += ';' + OptionString(urloptions, ';');

    if (!protocol.empty()) {
      if (!urlstr.empty())
        urlstr = protocol + "://" + urlstr;
      else
        urlstr = protocol + ":";
    }

    // Constructor makes sure path is absolute or empty.
    // ChangePath() also makes such check.
    if ( protocol == "ldap") // Unfortunately ldap is special case
      urlstr += '/';
    urlstr += path;

    // If there is nothing at this point there is no sense
    // to add any options
    if (urlstr.empty())
      return urlstr;

    if (!httpoptions.empty())
      urlstr += '?' + OptionString(httpoptions, '&');

    if (!ldapattributes.empty() || (ldapscope != base) || !ldapfilter.empty())
      urlstr += '?' + AttributeString(ldapattributes, ',');

    if ((ldapscope != base) || !ldapfilter.empty()) {
      switch (ldapscope) {
      case base:
        urlstr += "?base";
        break;

      case onelevel:
        urlstr += "?one";
        break;

      case subtree:
        urlstr += "?sub";
        break;
      }
    }

    if (!ldapfilter.empty())
      urlstr += '?' + ldapfilter;

    return urlstr;
  }

  std::string URL::str() const {

    std::string urlstr;

    if (!username.empty())
      urlstr += username;

    if (!passwd.empty())
      urlstr += ':' + passwd;

    if (!username.empty() || !passwd.empty())
      urlstr += '@';

    if (!host.empty())
      urlstr += host;

    if (port != -1)
      urlstr += ':' + tostring(port);

    if (!protocol.empty()) {
      if (!urlstr.empty())
        urlstr = protocol + "://" + urlstr;
      else
        urlstr = protocol + ":";
    }

    // Constructor makes sure path is absolute or empty.
    // ChangePath also makes such check.
    if ( protocol == "ldap") // Unfortunately ldap is special case
      urlstr += '/';
    urlstr += path;

    // If there is nothing at this point there is no sense
    // to add any options
    if (urlstr.empty())
      return urlstr;

    if (!httpoptions.empty())
      urlstr += '?' + OptionString(httpoptions, '&');

    if (!ldapattributes.empty() || (ldapscope != base) || !ldapfilter.empty())
      urlstr += '?' + AttributeString(ldapattributes, ',');

    if ((ldapscope != base) || !ldapfilter.empty()) {
      switch (ldapscope) {
      case base:
        urlstr += "?base";
        break;

      case onelevel:
        urlstr += "?one";
        break;

      case subtree:
        urlstr += "?sub";
        break;
      }
    }

    if (!ldapfilter.empty())
      urlstr += '?' + ldapfilter;

    return urlstr;
  }

  std::string URL::ConnectionURL() const {

    std::string urlstr;
    if (!protocol.empty())
      urlstr = protocol + "://";

    if (!host.empty())
      urlstr += host;

    if (port != -1)
      urlstr += ':' + tostring(port);

    return urlstr;
  }

  bool URL::operator<(const URL& url) const {
    return (str() < url.str());
  }

  bool URL::operator==(const URL& url) const {
    return (str() == url.str());
  }

  std::string URL::BaseDN2Path(const std::string& basedn) {

    std::string::size_type pos, pos2;
    // mds-vo-name=local, o=grid --> o=grid/mds-vo-name=local
    std::string newpath;

    pos = basedn.size();
    while ((pos2 = basedn.rfind(",", pos - 1)) != std::string::npos) {
      std::string tmppath = basedn.substr(pos2 + 1, pos - pos2 - 1);
      tmppath = tmppath.substr(tmppath.find_first_not_of(' '));
      newpath += tmppath + '/';
      pos = pos2;
    }

    newpath += basedn.substr(0, pos);

    return newpath;
  }

  std::string URL::Path2BaseDN(const std::string& newpath) {

    if (newpath.empty())
      return "";

    std::string basedn;
    std::string::size_type pos, pos2;

    pos = newpath.size();
    while ((pos2 = newpath.rfind("/", pos - 1)) != std::string::npos) {
      if (pos2 == 0) break;
      basedn += newpath.substr(pos2 + 1, pos - pos2 - 1) + ", ";
      pos = pos2;
    }
    
    if (pos2 == std::string::npos)
      basedn += newpath.substr(0, pos);
    else
      basedn += newpath.substr(pos2 + 1, pos - pos2 - 1);

    return basedn;
  }

  URL::operator bool() const {
    return (!protocol.empty());
  }

  bool URL::operator!() const {
    return (protocol.empty());
  }

  std::string URL::OptionString(const std::map<std::string,
                                               std::string>& options, char separator) {

    std::string optstring;

    if (options.empty())
      return optstring;

    for (std::map<std::string, std::string>::const_iterator
         it = options.begin(); it != options.end(); it++) {
      if (it != options.begin())
        optstring += separator;
      optstring += it->first + '=' + it->second;
    }
    return optstring;
  }


  std::ostream& operator<<(std::ostream& out, const URL& url) {
    return (out << url.str());
  }


  URLLocation::URLLocation(const std::string& url)
    : URL(url) {}

  URLLocation::URLLocation(const std::string& url,
                           const std::string& name)
    : URL(url),
      name(name) {}

  URLLocation::URLLocation(const URL& url)
    : URL(url) {}

  URLLocation::URLLocation(const URL& url,
                           const std::string& name)
    : URL(url),
      name(name) {}

  URLLocation::URLLocation(const std::map<std::string, std::string>& options,
                           const std::string& name)
    : URL(),
      name(name) {
    urloptions = options;
  }

  const std::string& URLLocation::Name() const {
    return name;
  }

  URLLocation::~URLLocation() {}

  std::string URLLocation::str() const {

    if (*this)
      return URL::str();
    else
      return name;
  }

  std::string URLLocation::fullstr() const {

    if (*this)
      return URL::fullstr();
    else if (urloptions.empty())
      return name;
    else
      return name + ';' + OptionString(urloptions, ';');
  }


  PathIterator::PathIterator(const std::string& path, bool end)
    : path(path),
      pos(std::string::npos),
      end(end) {
    if (end)
      operator--();
    else
      operator++();
  }

  PathIterator::~PathIterator() {}

  PathIterator& PathIterator::operator++() {
    done = false;
    if (pos != std::string::npos)
      pos = path.find('/', pos + 1);
    else if (!end && !path.empty())
      pos = path.find('/',(path[0] == '/')?1:0);
    else
      done = true;
    end = true;
    return *this;
  }

  PathIterator& PathIterator::operator--() {
    done = false;
    if (pos != std::string::npos)
      pos = pos ? path.rfind('/', pos - 1) : std::string::npos;
    else if (end && !path.empty())
      if((pos = path.rfind('/')) == 0) pos = std::string::npos;
    else
      done = true;
    end = false;
    return *this;
  }

  PathIterator::operator bool() const {
    return !done;
  }

  std::string PathIterator::operator*() const {
    if (pos != std::string::npos)
      return path.substr(0, pos);
    else if (end)
      return path;
    else
      return "";
  }

  std::string PathIterator::Rest() const {
    if (pos != std::string::npos)
      return path.substr(pos + 1);
    else if (!end)
      return path;
    else
      return "";
  }


  std::list<URL> ReadURLList(const URL& url) {

    std::list<URL> urllist;
    if (url.Protocol() == "urllist") {
      std::ifstream f(url.Path().c_str());
      std::string line;
      while (getline(f, line)) {
        URL url(line);
        if (url)
          urllist.push_back(url);
        else
          URLLogger.msg(ERROR, "urllist %s contains invalid URL: %s",
                        url.Path(), line);
      }
    }
    else
      URLLogger.msg(ERROR, "URL protocol is not urllist: %s", url.str());
    return urllist;
  }

} // namespace Arc
