#
# set environment variables for the desktop grid bridge:
#

##############################################################
# Reading configuration from $ARC_CONFIG
##############################################################

if [ -z "$pkgdatadir" ]; then echo 'pkgdatadir must be set' 1>&2; exit 1; fi

. "$pkgdatadir/config_parser.sh" || exit $?

ARC_CONFIG=${ARC_CONFIG:-/etc/arc.conf}
config_parse_file $ARC_CONFIG 1>&2 || exit $?

config_import_section "common"
config_import_section "infosys"
config_import_section "grid-manager"
config_import_section "lrms"

dgbridge_stage_dir=$CONFIG_dgbridge_stage_dir
dgbridge_stage_prepend=$CONFIG_dgbridge_stage_prepend

#set 3G values
EDGES_3G_TIMEOUT=24
EDGES_3G_RETRIES=3

# Script returned ok
true
