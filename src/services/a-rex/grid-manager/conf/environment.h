#ifndef __GM_ENVIRONMENT_H__
#define __GM_ENVIRONMENT_H__

#include <string>

class JobLog;
class JobsListConfig;
class ContinuationPlugins;
class RunPlugin;

namespace ARex {
  class DelegationStores;
}

class GMEnvironment {
  bool valid_;
  JobLog& job_log_;
  JobsListConfig& jobs_cfg_;
  ContinuationPlugins& plugins_;
  RunPlugin& cred_plugin_;
  // TODO: this should go away after proper locking in DelegationStore is implemented
  ARex::DelegationStores* delegations_;
 public:
  GMEnvironment(JobLog& job_log,JobsListConfig& jcfg,ContinuationPlugins& plugins,RunPlugin& cred_plugin,bool guess = false);
  operator bool(void) const { return valid_; };
  bool operator!(void) const { return !valid_; };

  /// ARC installation path - $ARC_LOCATION, executable path
  std::string nordugrid_loc(void) const;
  /// ARC system scripts - $ARC_LOCATION/share/arc, $ARC_LOCATION/share
  std::string nordugrid_data_loc(void) const;
  /// ARC system tools - $ARC_LOCATION/libexec/arc, $ARC_LOCATION/libexec
  std::string nordugrid_libexec_loc(void) const;
  // ARC libraries and plugins - $ARC_LOCATION/lib/arc, $ARC_LOCATION/lib
  std::string nordugrid_lib_loc(void) const;
  // ARC adminstrator tools - $ARC_LOCATION/sbin
  std::string nordugrid_sbin_loc(void) const;
  /// ARC configuration file 
  ///   /etc/arc.conf
  ///   $ARC_LOCATION/etc/arc.conf
  std::string nordugrid_config_loc(void) const;
  void nordugrid_config_loc(const std::string&);
  bool nordugrid_config_is_temp(void) const;
  void nordugrid_config_is_temp(bool);

  // Certificates directory location
  std::string cert_dir_loc() const;
  void cert_dir_loc(const std::string&) const;

  // VOMS lsc root directory location
  std::string voms_dir_loc() const;
  void voms_dir_loc(const std::string&) const;

  // RTE setup scripts
  std::string runtime_config_dir(void) const;
  void runtime_config_dir(const std::string&);

  /// Email address of person responsible for this ARC installation
  /// grid.manager@hostname, it can also be set from configuration file 
  std::string support_mail_address(void) const;
  void support_mail_address(const std::string&);

  JobLog& job_log() const;
  JobsListConfig& jobs_cfg() const;

  ARex::DelegationStores* delegations(void) const;
  void delegations(ARex::DelegationStores*);

  /// Scratch dir for job execution on node
  std::string scratch_dir() const;
  void scratch_dir(const std::string& dir);

  ContinuationPlugins& plugins() const;
  RunPlugin& cred_plugin() const;
};

///  Read environment, check files and set variables
///  Accepts:
///    guess - if false, default values are not allowed.
///  Returns:
///    true - success.
///    false - something is missing.
//bool read_env_vars(bool guess = false);

#endif // __GM_ENVIRONMENT_H__
