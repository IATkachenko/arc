#ifndef __GM_ENVIRONMENT_H__
#define __GM_ENVIRONMENT_H__

#include <string>

/// Globus installation path - $GLOBUS_LOCATION, /opt/globus
extern std::string globus_loc;
// Various Globus scripts - $GLOBUS_LOCATION/libexec
extern std::string globus_scripts_loc;
/// ARC installation path - $ARC_LOCATION, executable path
extern std::string nordugrid_loc;
/// ARC system tools - $ARC_LOCATION/libexec/arc, $ARC_LOCATION/libexec
extern std::string nordugrid_libexec_loc;
// ARC libraries and plugins - $ARC_LOCATION/lib/arc, $ARC_LOCATION/lib
extern std::string nordugrid_lib_loc;
// ARC adminstrator tools - $ARC_LOCATION/sbin
extern std::string nordugrid_sbin_loc;
/// ARC configuration file 
///   /etc/arc.conf
///   $ARC_LOCATION/etc/arc.conf
extern std::string nordugrid_config_loc;
/// Email address of person responsible for this ARC installation
/// grid.manager@hostname, it can also be set from configuration file 
extern std::string support_mail_address;
/// Global gridmap files with welcomed users' DNs and UNIX names
/// $GRIDMAP, default /etc/grid-security/grid-mapfile
extern std::string globus_gridmap;

///  Read environment, check files and set variables
///  Accepts:
///    guess - if false, default values are not allowed.
///  Returns:
///    true - success.
///    false - something is missing.
bool read_env_vars(bool guess = false);

#endif // __GM_ENVIRONMENT_H__
