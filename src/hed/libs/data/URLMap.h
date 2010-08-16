// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_URLMAP_H__
#define __ARC_URLMAP_H__

#include <list>

#include <arc/URL.h>
#include <arc/Logger.h>

namespace Arc {

  class URLMap {
  private:
    class map_entry {
    public:
      URL initial;
      URL replacement;
      URL access;
      map_entry() {}
      map_entry(const URL& templ, const URL& repl, const URL& accs = URL())
        : initial(templ),
          replacement(repl),
          access(accs) {}
    };
    std::list<map_entry> entries;
    static Logger logger;
  public:
    URLMap();
    ~URLMap();
    bool map(URL& url) const;
    bool local(const URL& url) const;
    void add(const URL& templ, const URL& repl, const URL& accs = URL());
    operator bool() const { return entries.size() != 0; };
    bool operator!() const { return entries.size() == 0; };
  };

} // namespace Arc

#endif // __ARC_URLMAP_H__
