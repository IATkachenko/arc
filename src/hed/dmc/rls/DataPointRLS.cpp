// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32 
#include <arc/win32.h>
#include <fcntl.h>
#endif

#include <arc/DateTime.h>
#include <arc/GUID.h>
#include <arc/Logger.h>
#include <arc/StringConv.h>
#include <arc/globusutils/GlobusWorkarounds.h>

#include "DataPointRLS.h"
#include "RLS.h"

#define globus_rls_free_result(err) \
  globus_rls_client_error_info(err, NULL, NULL, 0, GLOBUS_FALSE)

namespace Arc {

  static bool proxy_initialized = false;
  static bool globus_initialized = false;
  static bool persistent_initialized = false;

  Logger DataPointRLS::logger(Logger::getRootLogger(), "DataPoint.RLS");

  DataPointRLS::DataPointRLS(const URL& url, const UserConfig& usercfg)
    : DataPointIndex(url, usercfg),
      guid_enabled(false) {
    valid_url_options.push_back("guid");
    if(!globus_initialized) {
      globus_module_activate(GLOBUS_COMMON_MODULE);
      globus_module_activate(GLOBUS_IO_MODULE);
      globus_module_activate(GLOBUS_RLS_CLIENT_MODULE);
      if (!proxy_initialized)
        proxy_initialized = GlobusRecoverProxyOpenSSL();
    }
    std::string guidopt = url.Option("guid", "no");
    if ((guidopt == "yes") || (guidopt == ""))
      guid_enabled = true;
  }

  DataPointRLS::~DataPointRLS() {
    if(!persistent_initialized) {
      globus_module_deactivate(GLOBUS_RLS_CLIENT_MODULE);
      globus_module_deactivate(GLOBUS_IO_MODULE);
      globus_module_deactivate(GLOBUS_COMMON_MODULE);
      globus_initialized = false;
    }
  }

  Plugin* DataPointRLS::Instance(PluginArgument *arg) {
    DataPointPluginArgument *dmcarg =
      dynamic_cast<DataPointPluginArgument*>(arg);
    if (!dmcarg)
      return NULL;
    if (((const URL&)(*dmcarg)).Protocol() != "rls")
      return NULL;
    // Make this code non-unloadable because Globus
    // may have problems with unloading
    Glib::Module* module = dmcarg->get_module();
    PluginsFactory* factory = dmcarg->get_factory();
    if(factory && module) {
      factory->makePersistent(module);
      persistent_initialized = true;
    }
    return new DataPointRLS(*dmcarg, *dmcarg);
  }

  static const char* get_path_str(const URL& url) {
    const std::string& path = url.Path();
    if(path.empty() || (path[0] != '/')) return path.c_str();
    return (path.c_str()+1);
  }

  static bool path_empty(const URL& url) {
    const std::string& path = url.Path();
    if(path.empty()) return true;
    if((path.length() == 1) && (path[0] == '/')) return true;
    return false;
  }

  static globus_result_t globus_rls_client_lrc_attr_put(globus_rls_handle_t *h, char *key,
                                                        globus_rls_attribute_t *attr,
                                                        int overwrite) {
    globus_result_t err;
    int errcode;
    err = globus_rls_client_lrc_attr_add(h, key, attr);
    if (err != GLOBUS_SUCCESS) {
      err = globus_rls_client_error_info(err, &errcode, NULL, 0, GLOBUS_TRUE);
      if ((overwrite) && (errcode == GLOBUS_RLS_DBERROR)) {
        /* guess this can mean duplicate entry */
        globus_result_t err_ = globus_rls_client_lrc_attr_remove(h, key, attr);
        globus_rls_free_result(err_);
        if (err_ != GLOBUS_SUCCESS)
          return err;
        return globus_rls_client_lrc_attr_put(h, key, attr, 0);
      }
      if (errcode != GLOBUS_RLS_ATTR_NEXIST)
        return err;
      globus_rls_free_result(err);
      err = globus_rls_client_lrc_attr_create(h, attr->name,
                                              attr->objtype, attr->type);
      if (err != GLOBUS_SUCCESS)
        return err;
      err = globus_rls_client_lrc_attr_add(h, key, attr);
    }
    return err;
  }

  class meta_resolve_rls_t {
  public:
    DataPointRLS& dprls;
    bool source;
    DataStatus success;
    bool obtained_info;
    std::string guid;
    meta_resolve_rls_t(DataPointRLS& d, bool s)
      : dprls(d),
        source(s),
        success(DataStatus::NoLocationError),
        obtained_info(false) {}
  };

  static bool meta_resolve_callback(globus_rls_handle_t *h,
                                    const URL& rlsurl, void *arg) {
    return ((meta_resolve_rls_t*)arg)->dprls.ResolveCallback(h, rlsurl, arg);
  }

  URL DataPointRLS::AddPFN(const URL& purl,bool source) {
    if(source) return purl;
    bool se_uses_lfn = false;
    std::string u = purl.fullstr();
    if (purl.Protocol() == "se") {
      u += "?";
      se_uses_lfn = true;
    } else {
      u += "/";
    }
    if (guid_enabled) {
      std::string guid = UUID();
      if ((!se_uses_lfn) && (!pfn_path.empty()))
        u += pfn_path;
      else
        u += guid;
    }
    else if ((!se_uses_lfn) && (!pfn_path.empty()))
      u += pfn_path;
    else
      u += get_path_str(url);
    return URL(u);
  }


  bool DataPointRLS::ResolveCallback(globus_rls_handle_t *h,
                                     const URL& rlsurl, void *arg) {
    bool& source(((meta_resolve_rls_t*)arg)->source);
    DataStatus& success(((meta_resolve_rls_t*)arg)->success);
    bool& obtained_info(((meta_resolve_rls_t*)arg)->obtained_info);
    std::string& guid(((meta_resolve_rls_t*)arg)->guid);

    char errmsg[MAXERRMSG + 32];
    globus_result_t err;
    int errcode;

    // Ask LRC if it contains file of interest

    if (guid_enabled && source && guid.empty()) {
      // map lfn->guid (only once)
      globus_rls_attribute_t opr;
      opr.type = globus_rls_attr_type_str;
      opr.val.s = const_cast<char*>(get_path_str(url));
      int off = 0;
      globus_list_t *guids = NULL;
      err = globus_rls_client_lrc_attr_search(h, const_cast<char*>("lfn"),
                                              globus_rls_obj_lrc_lfn,
                                              globus_rls_attr_op_eq, &opr,
                                              NULL, &off, 1, &guids);
      if (err != GLOBUS_SUCCESS) {
        globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                     GLOBUS_FALSE);
        logger.msg(INFO, "Failed to find GUID for specified LFN in %s: %s",
                   rlsurl.str(), errmsg);
        return true;
      }
      if (!guids) {
        logger.msg(INFO, "There is no GUID for specified LFN in %s",
                   rlsurl.str());
        return true;
      }
      globus_rls_attribute_object_t *obattr =
        (globus_rls_attribute_object_t*)globus_list_first(guids);
      guid = obattr->key;
      globus_rls_client_free_list(guids);
    }
    globus_list_t *pfns_list = NULL;
    if (source) {
      if (!guid.empty())
        err = globus_rls_client_lrc_get_pfn
                (h, const_cast<char*>(guid.c_str()), 0, 0, &pfns_list);
      else
        err = globus_rls_client_lrc_get_pfn
                (h, const_cast<char*>(get_path_str(url)), 0, 0, &pfns_list);
    } else {
      err = globus_rls_client_lrc_get_pfn
              (h, const_cast<char*>("__storage_service__"), 0, 0, &pfns_list);
    }
    if (err != GLOBUS_SUCCESS) {
      globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                   GLOBUS_FALSE);
      if (errcode == GLOBUS_RLS_INVSERVER) {
        return true;
      } else if (errcode == GLOBUS_RLS_LFN_NEXIST) {
        return true;
      } else { // do not know
        logger.msg(INFO, "Warning: can't get PFNs from server %s: %s",
                   rlsurl.str(), errmsg);
        return true;
      }
    }
    if (!success) {
      success = DataStatus::Success; // got something
      if (source)
        registered = true;
    }
    if (url.Locations().size() == 0) {
      for (globus_list_t *lp = pfns_list; lp; lp = globus_list_rest(lp)) {
        globus_rls_string2_t *str2 =
          (globus_rls_string2_t*)globus_list_first(lp);
        URL pfn(str2->s2);
        for (std::map<std::string, std::string>::const_iterator i =
               url.CommonLocOptions().begin();
             i != url.CommonLocOptions().end(); i++) {
          pfn.AddOption(i->first, i->second, false);
        }
        URL pfn_ = AddPFN(pfn,source);
        if (!pfn_) {
          logger.msg(WARNING, "Skipping invalid location %s - %s", rlsurl.str(), pfn.plainstr());
        } else {
          logger.msg(VERBOSE, "Adding location: %s - %s", rlsurl.str(), pfn.plainstr());
          AddLocation(pfn_, rlsurl.str());
        }
      }
    } else {
      for (std::list<URLLocation>::const_iterator loc = url.Locations().begin();
           loc != url.Locations().end(); loc++) {
        if(source) {
          // for source find subset of locations registered in RLS
          for (globus_list_t *lp = pfns_list; lp; lp = globus_list_rest(lp)) {
            globus_rls_string2_t *str2 =
              (globus_rls_string2_t*)globus_list_first(lp);
            URL pfn(str2->s2);
            // for RLS URLs are used instead of metanames
            if (pfn == *loc) {
              logger.msg(VERBOSE, "Adding location: %s - %s",
                         rlsurl.str(), pfn.plainstr());
              for (std::map<std::string, std::string>::const_iterator i =
                     url.CommonLocOptions().begin();
                   i != url.CommonLocOptions().end(); i++) {
                pfn.AddOption(i->first, i->second, false);
              }
              URL pfn_ = AddPFN(pfn,source);
              if (!pfn_) {
                logger.msg(WARNING, "Skipping invalid location %s - %s", rlsurl.str(), pfn.plainstr());
              } else {
                logger.msg(VERBOSE, "Adding location: %s - %s", rlsurl.str(), pfn.plainstr());
                AddLocation(pfn_, rlsurl.str());
              }
              break;
            }
          }
        } else {
          // for destination accept specified locations
          URL pfn_(*loc);
          if (!pfn_) {
            logger.msg(WARNING, "Skipping invalid location %s - %s", rlsurl.str(), loc->str());
          } else {
            for (std::map<std::string, std::string>::const_iterator i =
                   url.CommonLocOptions().begin();
              i != url.CommonLocOptions().end(); i++) {
              pfn_.AddOption(i->first, i->second, false);
            }
            pfn_ = AddPFN(pfn_,source);
            AddLocation(pfn_, rlsurl.str());
          }
        }
      }
    }
    globus_rls_client_free_list(pfns_list);
    if (!obtained_info) {
      /* obtain metadata - assume it is same everywhere */
      globus_list_t *attr_list;
      if (!guid.empty())
        err = globus_rls_client_lrc_attr_value_get
                (h, const_cast<char*>(guid.c_str()),
                NULL, globus_rls_obj_lrc_lfn, &attr_list);
      else
        err = globus_rls_client_lrc_attr_value_get
                (h, const_cast<char*>(get_path_str(url)),
                NULL, globus_rls_obj_lrc_lfn, &attr_list);
      if (err != GLOBUS_SUCCESS) {
        globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                     GLOBUS_FALSE);
        if (errcode == GLOBUS_RLS_ATTR_NEXIST)
          return true;
        logger.msg(INFO, "Warning: Failed to obtain attributes from %s: %s",
                   rlsurl.str(), errmsg);
        return true;
      }
      registered = true; // even for destination
      for (globus_list_t *lpa = attr_list; lpa; lpa = globus_list_rest(lpa)) {
        globus_rls_attribute_t *attr =
          (globus_rls_attribute_t*)globus_list_first(lpa);
        if (attr->type != globus_rls_attr_type_str)
          continue;
        logger.msg(VERBOSE, "Attribute: %s - %s", attr->name, attr->val.s);
        if (strcmp(attr->name, "filechecksum") == 0) {
          if (!CheckCheckSum())
            SetCheckSum(attr->val.s);
        }
        else if (strcmp(attr->name, "size") == 0) {
          if (!CheckSize())
            SetSize(stringtoull(attr->val.s));
        }
        else if (strcmp(attr->name, "modifytime") == 0) {
          if (!CheckCreated()) {
            Time created(attr->val.s);
            if (created == -1)
              created.SetTime(stringtoull(attr->val.s));
            SetCreated(created);
          }
        }
        else if (strcmp(attr->name, "created") == 0)
          if (!CheckCreated()) {
            Time created(attr->val.s);
            if (created == -1)
              created.SetTime(stringtoull(attr->val.s));
            SetCreated(created);
          }
      }
      globus_rls_client_free_list(attr_list);
      obtained_info = true;
    }
    return true;
  }

  /* perform resolve operation, which can take long time */
  DataStatus DataPointRLS::Resolve(bool source) {
    resolved = false;
    registered = false;
    if (url.Host().empty()) {
      logger.msg(INFO, "RLS URL must contain host");
      if (source)
        return DataStatus::ReadResolveError;
      else
        return DataStatus::WriteResolveError;
    }
    if (source) {
      if (get_path_str(url)[0] == 0) {
        logger.msg(INFO, "Source must contain LFN");
        return DataStatus::ReadResolveError;
      }
      std::list<URL> rlis;
      std::list<URL> lrcs;
      rlis.push_back(url.ConnectionURL());
      lrcs.push_back(url.ConnectionURL());
      meta_resolve_rls_t arg(*this, source);
      rls_find_lrcs(rlis, lrcs, true, false,
                    &meta_resolve_callback, (void*)&arg);
      if (!arg.success)
        return arg.success;
    }
    else {
      if (get_path_str(url) == 0) {
        logger.msg(INFO, "Destination must contain LFN");
        return DataStatus::WriteResolveError;
      }
      std::list<URL> rlis;
      std::list<URL> lrcs;
      rlis.push_back(url.ConnectionURL());
      lrcs.push_back(url.ConnectionURL());
      if (url.Locations().size() == 0)
        logger.msg(INFO, "Locations are missing in destination RLS url - "
                   "will use those registered with special name");
      meta_resolve_rls_t arg(*this, source);
      rls_find_lrcs(rlis, lrcs, true, false,
                    &meta_resolve_callback, (void*)&arg);
      if (!arg.success)
        return arg.success;
      if (!HaveLocations()) {
        logger.msg(INFO, "No locations found for destination");
        return DataStatus::WriteResolveError;
      }
/*
      // This part is done directly in callback. The difference
      // is that instead of "arbitrary" LRC the one provided by
      // callback is used
      // Make pfns
      std::list<URL>::iterator lrc_p = lrcs.begin();
      for (std::list<URLLocation>::iterator loc = locations.begin();
           loc != locations.end();) {
        // AddPFN
        if (!loc->Name().empty()) {
          logger.msg(VERBOSE, "Using location: %s - %s",
                     loc->Name(), loc->str());
          ++loc;
        }
        // Use arbitrary lrc
        else if (lrc_p == lrcs.end()) {   // no LRC
          logger.msg(VERBOSE, "Removing location: %s - %s",
                     loc->Name(), loc->str());
          loc = locations.erase(loc);
        }
        else {
          *loc = URLLocation(*loc, lrc_p->str());
          ++lrc_p;
          if (lrc_p == lrcs.end())
            lrc_p = lrcs.begin();
          logger.msg(VERBOSE, "Using location: %s - %s",
                     loc->Name(), loc->str());
          ++loc;
        }
      }
*/
    }
    if (CheckCheckSum()) logger.msg(VERBOSE, "meta_get_data: checksum: %s", GetCheckSum());
    if (CheckSize()) logger.msg(VERBOSE, "meta_get_data: size: %llu", GetSize());
    if (CheckCreated()) logger.msg(VERBOSE, "meta_get_data: created: %s", GetCreated().str());
    resolved = true;
    return DataStatus::Success;
  }

  DataStatus DataPointRLS::PreRegister(bool replication, bool force) {
    if (url.Host().empty()) {
      logger.msg(INFO, "RLS URL must contain host");
      return DataStatus::PreRegisterError;
    }
    if (replication) { /* replicating inside same lfn */
      if (!registered) { /* for replication it must be there */
        logger.msg(ERROR, "LFN is missing in RLS (needed for replication)");
        return DataStatus::PreRegisterError;
      }
      return DataStatus::Success;
    }
    if (registered)  /* algorithm require this to be new file */
      if (!force) {
        logger.msg(ERROR, "LFN already exists in replica");
        return DataStatus::PreRegisterError;
      }
    /* RLS does not support LFN only in database - hence doing nothing here */
    return DataStatus::Success;
  }

  DataStatus DataPointRLS::PostRegister(bool replication) {
    globus_rls_handle_t *h;
    char errmsg[MAXERRMSG + 32];
    globus_result_t err;
    int errcode;

    if (url.Host().empty()) {
      logger.msg(INFO, "RLS URL must contain host");
      return DataStatus::PostRegisterError;
    }
    err = globus_rls_client_connect
            (const_cast<char*>(url.ConnectionURL().c_str()), &h);
    if (err != GLOBUS_SUCCESS) {
      globus_rls_client_error_info(err, NULL, errmsg, MAXERRMSG + 32,
                                   GLOBUS_FALSE);
      logger.msg(INFO, "Failed to connect to RLS server: %s", errmsg);
      return DataStatus::PostRegisterError;
    }
    // assume that is RLI and try to resolve for special/any name

    std::string pfn;
    std::string guid;
    pfn = CurrentLocation().plainstr();
    // it is always better to register pure url
    std::string rls_lfn = get_path_str(url);
    if (!replication)
      if (guid_enabled) {
        for (;;) {
          // generate guid
          guid = UUID();
          // store in LRC
          err = globus_rls_client_lrc_create
                  (h, const_cast<char*>(guid.c_str()),
                  const_cast<char*>(pfn.c_str()));
          if (err != GLOBUS_SUCCESS) {
            err = globus_rls_client_error_info(err, &errcode, NULL, 0,
                                               GLOBUS_TRUE);
            if (errcode == GLOBUS_RLS_LFN_EXIST) {
              globus_rls_free_result(err);
              continue;
            }
          }
          rls_lfn = guid;
          break;
        }
        if (err != GLOBUS_SUCCESS) {
          globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                       GLOBUS_FALSE);
          logger.msg(INFO, "Failed to create GUID in RLS: %s", errmsg);
          globus_rls_client_close(h);
          return DataStatus::PostRegisterError;
        }
        // Check if there is no same LFN
        globus_rls_attribute_t opr;
        opr.type = globus_rls_attr_type_str;
        opr.val.s = const_cast<char*>(get_path_str(url));
        int off = 0;
        globus_list_t *guids = NULL;
        err = globus_rls_client_lrc_attr_search(h, const_cast<char*>("lfn"),
                                                globus_rls_obj_lrc_lfn,
                                                globus_rls_attr_op_eq,
                                                &opr, NULL, &off, 1, &guids);
        if (err != GLOBUS_SUCCESS) {
          globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                       GLOBUS_FALSE);
          if ((errcode != GLOBUS_RLS_LFN_NEXIST) &&
              (errcode != GLOBUS_RLS_ATTR_NEXIST) &&
              (errcode != GLOBUS_RLS_ATTR_VALUE_NEXIST)) {
            logger.msg(INFO, "Failed to check for existing LFN in %s: %s",
                       url.str(), errmsg);
            globus_rls_client_close(h);
            return DataStatus::PostRegisterError;
          }
        }
        if (guids) {
          globus_rls_client_free_list(guids);
          logger.msg(INFO, "There is same LFN in %s", url.str());
          globus_rls_client_close(h);
          return DataStatus::PostRegisterError;
        }
        // add LFN
        globus_rls_attribute_t attr;
        attr.objtype = globus_rls_obj_lrc_lfn;
        attr.type = globus_rls_attr_type_str;
        attr.name = const_cast<char*>("lfn");
        attr.val.s = const_cast<char*>(get_path_str(url));
        err = globus_rls_client_lrc_attr_put
                (h, const_cast<char*>(rls_lfn.c_str()), &attr, 0);
        if (err != GLOBUS_SUCCESS) {
          globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                       GLOBUS_FALSE);
          logger.msg(INFO, "Failed to add LFN-GUID to RLS: %s", errmsg);
          globus_rls_client_close(h);
          return DataStatus::PostRegisterError;
        }
      }
      else {
        err = globus_rls_client_lrc_create
                (h, const_cast<char*>(get_path_str(url)),
                const_cast<char*>(pfn.c_str()));
        if (err != GLOBUS_SUCCESS) {
          err = globus_rls_client_error_info(err, &errcode, NULL, 0,
                                             GLOBUS_TRUE);
          if (errcode == GLOBUS_RLS_LFN_EXIST) {
            globus_rls_free_result(err);
            err = globus_rls_client_lrc_add
                    (h, const_cast<char*>(get_path_str(url)),
                    const_cast<char*>(pfn.c_str()));
          }
        }
      }
    else {
      if (guid_enabled) {
        // get guid
        globus_rls_attribute_t opr;
        opr.type = globus_rls_attr_type_str;
        opr.val.s = const_cast<char*>(get_path_str(url));
        int off = 0;
        globus_list_t *guids = NULL;
        err = globus_rls_client_lrc_attr_search(h, const_cast<char*>("lfn"),
                                                globus_rls_obj_lrc_lfn,
                                                globus_rls_attr_op_eq,
                                                &opr, NULL, &off, 1, &guids);
        if (err != GLOBUS_SUCCESS) {
          globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                       GLOBUS_FALSE);
          logger.msg(INFO, "Failed to find GUID for specified LFN in %s: %s",
                     url.str(), errmsg);
          globus_rls_client_close(h);
          return DataStatus::PostRegisterError;
        }
        if (!guids) {
          logger.msg(INFO, "There is no GUID for specified LFN in %s",
                     url.str());
          globus_rls_client_close(h);
          return DataStatus::PostRegisterError;
        }
        globus_rls_attribute_object_t *obattr =
          (globus_rls_attribute_object_t*)globus_list_first(guids);
        guid = obattr->key;
        globus_rls_client_free_list(guids);
        rls_lfn = guid;
      }
      err = globus_rls_client_lrc_add
              (h, const_cast<char*>(rls_lfn.c_str()),
              const_cast<char*>(pfn.c_str()));
    }
    if (err != GLOBUS_SUCCESS) {
      globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                   GLOBUS_FALSE);
      if (errcode != GLOBUS_RLS_MAPPING_EXIST) {
        logger.msg(INFO, "Failed to create/add LFN-PFN mapping: %s", errmsg);
        globus_rls_client_close(h);
        return DataStatus::PostRegisterError;
      }
    }
    globus_rls_attribute_t attr;
    std::string attr_val;
    attr.objtype = globus_rls_obj_lrc_lfn;
    attr.type = globus_rls_attr_type_str;
    attr.name = const_cast<char*>("filetype");
    attr.val.s = const_cast<char*>("file");
    err = globus_rls_client_lrc_attr_put
            (h, const_cast<char*>(rls_lfn.c_str()), &attr, 0);
    if (err != GLOBUS_SUCCESS) {
      globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                   GLOBUS_FALSE);
      if (errcode != GLOBUS_RLS_ATTR_EXIST)
        logger.msg(INFO, "Warning: failed to add attribute to RLS: %s",
                   errmsg);
    }
    if (CheckSize()) {
      attr.name = const_cast<char*>("size");
      attr_val = tostring(GetSize());
      attr.val.s = const_cast<char*>(attr_val.c_str());
      err = globus_rls_client_lrc_attr_put
              (h, const_cast<char*>(rls_lfn.c_str()), &attr, 0);
      if (err != GLOBUS_SUCCESS) {
        globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                     GLOBUS_FALSE);
        if (errcode != GLOBUS_RLS_ATTR_EXIST)
          logger.msg(INFO, "Warning: failed to add attribute to RLS: %s",
                     errmsg);
      }
    }
    if (CheckCheckSum()) {
      attr.name = const_cast<char*>("filechecksum");
      attr_val = GetCheckSum();
      attr.val.s = const_cast<char*>(attr_val.c_str());
      err = globus_rls_client_lrc_attr_put
              (h, const_cast<char*>(rls_lfn.c_str()), &attr, 0);
      if (err != GLOBUS_SUCCESS) {
        globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                     GLOBUS_FALSE);
        if (errcode != GLOBUS_RLS_ATTR_EXIST)
          logger.msg(INFO, "Warning: failed to add attribute to RLS: %s",
                     errmsg);
      }
    }
    if (CheckCreated()) {
      attr.name = const_cast<char*>("modifytime");
      attr_val = GetCreated();
      attr.val.s = const_cast<char*>(attr_val.c_str());
      err = globus_rls_client_lrc_attr_put
              (h, const_cast<char*>(rls_lfn.c_str()), &attr, 0);
      if (err != GLOBUS_SUCCESS) {
        globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                     GLOBUS_FALSE);
        if (errcode != GLOBUS_RLS_ATTR_EXIST)
          logger.msg(INFO, "Warning: failed to add attribute to RLS: %s",
                     errmsg);
      }
    }
    if (url.Options().size() > 0)
      for (std::map<std::string, std::string>::const_iterator pos =
             url.Options().begin(); pos != url.Options().end(); pos++) {
        attr.name = const_cast<char*>(pos->first.c_str());
        attr.val.s = const_cast<char*>(pos->second.c_str());
        err = globus_rls_client_lrc_attr_put
                (h, const_cast<char*>(rls_lfn.c_str()), &attr, 0);
        if (err != GLOBUS_SUCCESS) {
          globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                       GLOBUS_FALSE);
          if (errcode != GLOBUS_RLS_ATTR_EXIST)
            logger.msg(INFO, "Warning: failed to add attribute to RLS: %s",
                       errmsg);
        }
      }
    globus_rls_client_close(h);
    return DataStatus::Success;
  }

  DataStatus DataPointRLS::PreUnregister(bool) {
    return DataStatus::Success;
  }

  class meta_unregister_rls_t {
  public:
    DataPointRLS& dprls;
    bool all;
    DataStatus success;
    std::string guid;
    meta_unregister_rls_t(DataPointRLS& d, bool a)
      : dprls(d),
        all(a),
        success(DataStatus::Success) {}
  };

  static bool meta_unregister_callback(globus_rls_handle_t *h,
                                       const URL& rlsurl, void *arg) {
    return ((meta_unregister_rls_t*)arg)->dprls.UnregisterCallback(h, rlsurl, arg);
  }

  bool DataPointRLS::UnregisterCallback(globus_rls_handle_t *h,
                                        const URL& rlsurl, void *arg) {
    bool& all(((meta_unregister_rls_t*)arg)->all);
    DataStatus& success(((meta_unregister_rls_t*)arg)->success);
    std::string& guid(((meta_unregister_rls_t*)arg)->guid);

    int lrc_offset = 0;
    int lrc_limit = 0;
    globus_result_t err;
    int errcode;
    char errmsg[MAXERRMSG + 32];
    globus_list_t *pfns_list;
    if (guid_enabled && guid.empty()) {
      // map lfn->guid (only once)
      globus_rls_attribute_t opr;
      opr.type = globus_rls_attr_type_str;
      opr.val.s = const_cast<char*>(get_path_str(url));
      int off = 0;
      globus_list_t *guids = NULL;
      err = globus_rls_client_lrc_attr_search(h, const_cast<char*>("lfn"),
                                              globus_rls_obj_lrc_lfn,
                                              globus_rls_attr_op_eq,
                                              &opr, NULL, &off, 1, &guids);
      if (err != GLOBUS_SUCCESS) {
        globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                     GLOBUS_FALSE);
        logger.msg(DEBUG, "Failed to find GUID for specified LFN in %s: %s",
                   rlsurl.str(), errmsg);
        return true;
      }
      if (!guids) {
        logger.msg(DEBUG, "There is no GUID for specified LFN in %s",
                   rlsurl.str());
        return true;
      }
      globus_rls_attribute_object_t *obattr =
        (globus_rls_attribute_object_t*)globus_list_first(guids);
      guid = obattr->key;
      globus_rls_client_free_list(guids);
    }
    if (all) {
      if (!guid.empty())
        err = globus_rls_client_lrc_get_pfn
                (h, const_cast<char*>(guid.c_str()),
                &lrc_offset, lrc_limit, &pfns_list);
      else
        err = globus_rls_client_lrc_get_pfn
                (h, const_cast<char*>(get_path_str(url)),
                &lrc_offset, lrc_limit, &pfns_list);
      if (err != GLOBUS_SUCCESS) {
        globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                     GLOBUS_FALSE);
        logger.msg(INFO, "Warning: Failed to retrieve LFN/PFNs from %s: %s",
                   rlsurl.str(), errmsg);
        success = DataStatus::UnregisterError;
        return true;
      }
      for (globus_list_t *lp = pfns_list; lp; lp = globus_list_rest(lp)) {
        globus_rls_string2_t *str2 =
          (globus_rls_string2_t*)globus_list_first(lp);
        URL pfn(str2->s2);
        if (pfn.Protocol() == "se")
          logger.msg(VERBOSE, "SE location will be unregistered automatically");
        else {
          err = globus_rls_client_lrc_delete(h, str2->s1, str2->s2);
          if (err != GLOBUS_SUCCESS) {
            globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                         GLOBUS_FALSE);
            if ((errcode != GLOBUS_RLS_MAPPING_NEXIST) &&
                (errcode != GLOBUS_RLS_LFN_NEXIST) &&
                (errcode != GLOBUS_RLS_PFN_NEXIST)) {
              logger.msg(INFO, "Warning: Failed to delete LFN/PFN from %s: %s",
                         rlsurl.str(), errmsg);
              success = DataStatus::UnregisterError;
              continue;
            }
          }
        }
      }
      globus_rls_client_free_list(pfns_list);
    }
    else { // ! all
      err = globus_rls_client_lrc_delete
              (h, const_cast<char*>(get_path_str(url)),
              const_cast<char*>(CurrentLocation().plainstr().c_str()));
      if (err != GLOBUS_SUCCESS) {
        globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                     GLOBUS_FALSE);
        if ((errcode != GLOBUS_RLS_MAPPING_NEXIST) &&
            (errcode != GLOBUS_RLS_LFN_NEXIST) &&
            (errcode != GLOBUS_RLS_PFN_NEXIST)) {
          logger.msg(INFO, "Warning: Failed to delete LFN/PFN from %s: %s",
                     rlsurl.str(), errmsg);
          success = DataStatus::UnregisterError;
        }
      }
    }
    return true;
  }

  DataStatus DataPointRLS::Unregister(bool all) {
    if (url.Host().empty()) {
      logger.msg(INFO, "RLS URL must contain host");
      return DataStatus::UnregisterError;
    }
    if (!all) {
      if (!LocationValid()) {
        logger.msg(ERROR, "Location is missing");
        return DataStatus::UnregisterError;
      }
      if (CurrentLocation().Protocol() == "se") {
        logger.msg(VERBOSE, "SE location will be unregistered automatically");
        return DataStatus::Success;
      }
    }
    if (!guid_enabled) {
      globus_rls_handle_t *h;
      char errmsg[MAXERRMSG + 32];
      globus_result_t err;
      int errcode;
      globus_list_t *pfns_list;

      err = globus_rls_client_connect
              (const_cast<char*>(url.ConnectionURL().c_str()), &h);
      if (err != GLOBUS_SUCCESS) {
        globus_rls_client_error_info(err, NULL, errmsg, MAXERRMSG + 32,
                                     GLOBUS_FALSE);
        logger.msg(INFO, "Failed to connect to RLS server: %s", errmsg);
        return DataStatus::UnregisterError;
      }
      // first find all LRC servers storing required information
      globus_list_t *lrcs = NULL;
      globus_rls_string2_t lrc_direct;
      globus_bool_t free_lrcs = GLOBUS_FALSE;
      lrc_direct.s1 = const_cast<char*>(get_path_str(url));
      lrc_direct.s2 = NULL; // for current connection
      int lrc_offset = 0;
      int lrc_limit = 0;
      err = globus_rls_client_rli_get_lrc
              (h, const_cast<char*>(get_path_str(url)),
              &lrc_offset, lrc_limit, &lrcs);
      if (err != GLOBUS_SUCCESS) {
        globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                     GLOBUS_FALSE);
        if (errcode == GLOBUS_RLS_LFN_NEXIST) {
          logger.msg(INFO, "LFN must be already deleted, try LRC anyway");
          lrcs = NULL;
        }
        else if (errcode != GLOBUS_RLS_INVSERVER) {
          logger.msg(INFO, "Failed to retrieve LFN/LRC: %s", errmsg);
          globus_rls_client_close(h);
          return DataStatus::UnregisterError;
        }
        // Probably that is LRC server only.
        globus_list_insert(&lrcs, &lrc_direct);
      }
      else
        free_lrcs = GLOBUS_TRUE;
      err = GLOBUS_SUCCESS;
      // TODO: sort by lrc and cache connections
      DataStatus success = DataStatus::Success;
      for (globus_list_t *p = lrcs; p; p = globus_list_rest(p)) {
        globus_rls_string2_t *str2 =
          (globus_rls_string2_t*)globus_list_first(p);
        char *lrc = str2->s2;
        globus_rls_handle_t *h_;
        if (lrc) {
          err = globus_rls_client_connect(lrc, &h_);
          if (err != GLOBUS_SUCCESS) {
            globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                         GLOBUS_FALSE);
            logger.msg(INFO, "Warning: Failed to connect to LRC at %s: %s",
                       lrc, errmsg);
            success = DataStatus::UnregisterError;
            continue;
          }
        }
        else
          h_ = h; // This server is already connected
        if (all) {
          err = globus_rls_client_lrc_get_pfn
                  (h_, const_cast<char*>(get_path_str(url)),
                  &lrc_offset, lrc_limit, &pfns_list);
          if (err != GLOBUS_SUCCESS) {
            globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                         GLOBUS_FALSE);
            if ((errcode != GLOBUS_RLS_MAPPING_NEXIST) &&
                (errcode != GLOBUS_RLS_LFN_NEXIST) &&
                (errcode != GLOBUS_RLS_PFN_NEXIST)) {
              logger.msg(INFO,
                         "Warning: Failed to retrieve LFN/PFNs from %s: %s",
                         lrc ? lrc : url.ConnectionURL(), errmsg);
              if (lrc)
                globus_rls_client_close(h_);
              success = DataStatus::UnregisterError;
              continue;
            }
            // Probably no such LFN - good, less work to do
            pfns_list = NULL;
          }
          for (globus_list_t *lp = pfns_list; lp; lp = globus_list_rest(lp)) {
            globus_rls_string2_t *str2 =
              (globus_rls_string2_t*)globus_list_first(lp);
            URL pfn(str2->s1);
            if (pfn.Protocol() == "se")
              logger.msg(VERBOSE,
                         "SE location will be unregistered automatically");
            else {
              err = globus_rls_client_lrc_delete
                      (h_, const_cast<char*>(get_path_str(url)), str2->s1);
              if (err != GLOBUS_SUCCESS) {
                globus_rls_client_error_info(err, &errcode, errmsg,
                                             MAXERRMSG + 32, GLOBUS_FALSE);
                if ((errcode != GLOBUS_RLS_MAPPING_NEXIST) &&
                    (errcode != GLOBUS_RLS_LFN_NEXIST) &&
                    (errcode != GLOBUS_RLS_PFN_NEXIST)) {
                  logger.msg(INFO,
                             "Warning: Failed to delete LFN/PFN from %s: %s",
                             lrc ? lrc : url.ConnectionURL(), errmsg);
                  if (lrc)
                    globus_rls_client_close(h_);
                  success = DataStatus::UnregisterError;
                  continue;
                }
              }
            }
          }
          if (pfns_list)
            globus_rls_client_free_list(pfns_list);
        }
        else { // ! all
          err = globus_rls_client_lrc_delete
                  (h_, const_cast<char*>(get_path_str(url)),
                  const_cast<char*>(CurrentLocation().plainstr().c_str()));
          if (err != GLOBUS_SUCCESS) {
            globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                         GLOBUS_FALSE);
            if ((errcode != GLOBUS_RLS_MAPPING_NEXIST) &&
                (errcode != GLOBUS_RLS_LFN_NEXIST) &&
                (errcode != GLOBUS_RLS_PFN_NEXIST)) {
              logger.msg(INFO, "Warning: Failed to delete LFN/PFN from %s: %s",
                         lrc, errmsg);
              if (lrc)
                globus_rls_client_close(h_);
              success = DataStatus::UnregisterError;
              continue;
            }
          }
        }
        if (lrc)
          globus_rls_client_close(h_);
      }
      globus_rls_client_close(h);
      if (free_lrcs)
        globus_rls_client_free_list(lrcs);
      else
        globus_list_free(lrcs);
      if (!success) {
        registered = false;
        ClearLocations();
      }
      return success;
    }
    else { // guid_enabled
      std::list<URL> rlis;
      std::list<URL> lrcs;
      rlis.push_back(url.ConnectionURL());
      lrcs.push_back(url.ConnectionURL());
      meta_unregister_rls_t arg(*this, all);
      rls_find_lrcs(rlis, lrcs, true, false,
                    &meta_unregister_callback, (void*)&arg);
      if (!arg.success) {
        registered = false;
        ClearLocations();
      }
      return arg.success;
    }
  }

  static bool get_attributes(globus_rls_handle_t *h, const std::string& lfn,
                             FileInfo& f) {
    globus_list_t *attr_list;
    char errmsg[MAXERRMSG + 32];
    globus_result_t err;
    int errcode;
    err = globus_rls_client_lrc_attr_value_get
            (h, const_cast<char*>(lfn.c_str()),
            NULL, globus_rls_obj_lrc_lfn, &attr_list);
    if (err != GLOBUS_SUCCESS) {
      globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                   GLOBUS_FALSE);
      if (errcode != GLOBUS_RLS_ATTR_NEXIST)
        // logger.msg(INFO, "Warning: Failed to retrieve attributes: %s",
        // errmsg);
        return false;
      return true;
    }
    for (globus_list_t *pa = attr_list; pa; pa = globus_list_rest(pa)) {
      globus_rls_attribute_t *attr =
        (globus_rls_attribute_t*)globus_list_first(pa);
      if (attr->type != globus_rls_attr_type_str)
        continue;
      // logger.msg(VERBOSE, "Attribute: %s - %s", attr->name, attr->val.s);
      if (strcmp(attr->name, "filechecksum") == 0)
        f.SetCheckSum(attr->val.s);
      else if (strcmp(attr->name, "size") == 0)
        f.SetSize(stringtoull(attr->val.s));
      else if (strcmp(attr->name, "modifytime") == 0) {
        Time created(attr->val.s);
        if (created == -1)
          created.SetTime(stringtoull(attr->val.s));
        f.SetCreated(created);
      }
      else if (strcmp(attr->name, "created") == 0) {
        Time created(attr->val.s);
        if (created == -1)
          created.SetTime(stringtoull(attr->val.s));
        f.SetCreated(created);
      }
    }
    globus_rls_client_free_list(attr_list);
    return true;
  }

  class list_files_rls_t {
  public:
    DataPointRLS& dprls;
    std::list<FileInfo>& files;
    DataStatus success;
    bool resolve;
    std::string guid;
    list_files_rls_t(DataPointRLS& d, std::list<FileInfo>& f,
                     bool r)
      : dprls(d),
        files(f),
        success(DataStatus::Success),
        resolve(r) {}
  };

  static bool list_files_callback(globus_rls_handle_t *h,
                                  const URL& rlsurl, void *arg) {
    return ((list_files_rls_t*)arg)->dprls.ListFilesCallback(h, rlsurl, arg);
  }

  bool DataPointRLS::ListFilesCallback(globus_rls_handle_t *h,
                                       const URL& rlsurl, void *arg) {
    std::list<FileInfo>& files(((list_files_rls_t*)arg)->files);
    DataStatus& success(((list_files_rls_t*)arg)->success);
    bool& resolve(((list_files_rls_t*)arg)->resolve);
    std::string& guid(((list_files_rls_t*)arg)->guid);

    int lrc_offset = 0;
    globus_result_t err;
    int errcode;
    char errmsg[MAXERRMSG + 32];
    globus_list_t *pfns = NULL;
    if (guid_enabled && !path_empty(url) && guid.empty()) {
      // looking gor guid only once
      // looking for guid only if lfn specified
      globus_rls_attribute_t opr;
      opr.type = globus_rls_attr_type_str;
      opr.val.s = const_cast<char*>(get_path_str(url));
      int off = 0;
      globus_list_t *guids = NULL;
      err = globus_rls_client_lrc_attr_search(h, const_cast<char*>("lfn"),
                                              globus_rls_obj_lrc_lfn,
                                              globus_rls_attr_op_eq,
                                              &opr, NULL, &off, 1, &guids);
      if (err != GLOBUS_SUCCESS) {
        globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                     GLOBUS_FALSE);
        logger.msg(INFO, "Failed to find GUID for specified LFN in %s: %s",
                   rlsurl.str(), errmsg);
        return true;
      }
      if (!guids) {
        logger.msg(INFO, "There is no GUID for specified LFN in %s",
                   rlsurl.str());
        return true;
      }
      globus_rls_attribute_object_t *obattr =
        (globus_rls_attribute_object_t*)globus_list_first(guids);
      guid = obattr->key;
      globus_rls_client_free_list(guids);
    }
    if (!guid.empty())
      err = globus_rls_client_lrc_get_pfn
              (h, const_cast<char*>(guid.c_str()), &lrc_offset, 1000, &pfns);
    else if (!path_empty(url))
      err = globus_rls_client_lrc_get_pfn
              (h, const_cast<char*>(get_path_str(url)), &lrc_offset, 1000, &pfns);
    else
      err = globus_rls_client_lrc_get_pfn_wc(h, const_cast<char*>("*"),
                                             rls_pattern_unix,
                                             &lrc_offset, 1000, &pfns);
    if (err != GLOBUS_SUCCESS) {
      globus_rls_client_error_info(err, &errcode, errmsg, MAXERRMSG + 32,
                                   GLOBUS_FALSE);
      if (errcode == GLOBUS_RLS_LFN_NEXIST) {
        logger.msg(VERBOSE, "No LFNs found in %s", rlsurl.str());
        success = DataStatus::Success;
        return true;
      }
      logger.msg(INFO, "Failed to retrieve list of LFNs/PFNs from %s",
                 rlsurl.str());
      return true;
    }
    success = DataStatus::Success;
    std::string last_lfn = "";
    std::string last_guid = "";
    for (globus_list_t *p = pfns; p; p = globus_list_rest(p)) {
      globus_rls_string2_t *str2 =
        (globus_rls_string2_t*)globus_list_first(p);
      std::string lfn(str2->s1);
      URL pfn(str2->s2);
      if (guid_enabled) {
        if (lfn != last_guid) {
          last_guid = lfn;
          last_lfn = "";
          // get real lfn
          globus_list_t *lfn_list = NULL;
          err = globus_rls_client_lrc_attr_value_get
                  (h, const_cast<char*>(lfn.c_str()),
                  const_cast<char*>("lfn"), globus_rls_obj_lrc_lfn, &lfn_list);
          if (err != GLOBUS_SUCCESS) {
            globus_rls_client_error_info(err, &errcode, errmsg,
                                         MAXERRMSG + 32, GLOBUS_FALSE);
            continue;
          }
          if (lfn_list == NULL)
            continue;
          globus_rls_attribute_t *attr =
            (globus_rls_attribute_t*)globus_list_first(lfn_list);
          if (attr->type != globus_rls_attr_type_str) {
            globus_rls_client_free_list(lfn_list);
            continue;
          }
          // use only first lfn (TODO: all lfns)
          last_lfn = attr->val.s;
          globus_rls_client_free_list(lfn_list);
        }
        if (!last_lfn.empty()) {
          logger.msg(VERBOSE, "lfn: %s(%s) - %s",
                     last_lfn, last_guid, pfn.str());
          std::list<FileInfo>::iterator f;
          for (f = files.begin(); f != files.end(); ++f)
            if (f->GetName() == last_lfn)
              break;
          if (f == files.end()) {
            f = files.insert(files.end(), FileInfo(last_lfn.c_str()));
            if (resolve)
              get_attributes(h, last_guid, *f);
          }
          f->AddURL(pfn);
        }
      }
      else { // !guid_enabled
        logger.msg(VERBOSE, "lfn: %s - pfn: %s", lfn, pfn.str());
        std::list<FileInfo>::iterator f;
        for (f = files.begin(); f != files.end(); ++f)
          if (f->GetName() == lfn)
            break;
        if (f == files.end()) {
          f = files.insert(files.end(), FileInfo(lfn));
          if (resolve)
            get_attributes(h, lfn, *f);
        }
        f->AddURL(pfn);
      }
    }
    globus_rls_client_free_list(pfns);
    return true;
  }

  DataStatus DataPointRLS::Check() {
    // simply check that the file can be listed
    FileInfo file;
    DataStatus r = Stat(file,DataPoint::INFO_TYPE_MINIMAL);
    if(r == DataStatus::StatErrorRetryable) r = DataStatus::CheckErrorRetryable;
    if(r == DataStatus::StatError) r = DataStatus::CheckError;
    return r;
  }

  DataStatus DataPointRLS::Stat(FileInfo& file, DataPointInfoType verb) {
    std::list<URL> rlis;
    std::list<URL> lrcs;
    std::list<FileInfo> files;
    if (url.Host().empty()) {
      logger.msg(INFO, "RLS URL must contain host");
      return DataStatus::StatError;
    }
    rlis.push_back(url.ConnectionURL());
    lrcs.push_back(url.ConnectionURL());

    list_files_rls_t arg(*this, files, (verb | INFO_TYPE_NAME) != INFO_TYPE_NAME);
    rls_find_lrcs(rlis, lrcs, true, false,
                  &list_files_callback, (void*)&arg);
    if ((files.size() > 0) && (arg.success.Passed())) {
      file = files.front();
      // set some metadata
      if (file.CheckSize()) SetSize(file.GetSize());
      if (file.CheckCheckSum()) SetCheckSum(file.GetCheckSum());
      if (file.CheckCreated()) SetCreated(file.GetCreated());
      if (file.CheckValid()) SetValid(file.GetValid());
    }
    return arg.success;
  }

  DataStatus DataPointRLS::List(std::list<FileInfo>& /*files*/, DataPointInfoType /*verb*/) {
    // RLS has flat struture
    return DataStatus::ListError;
  }

} // namespace Arc

Arc::PluginDescriptor PLUGINS_TABLE_NAME[] = {
  { "rls", "HED:DMC", 0, &Arc::DataPointRLS::Instance },
  { NULL, NULL, 0, NULL }
};
