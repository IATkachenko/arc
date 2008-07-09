#!/usr/bin/perl -w

package ClusterInfo;

use File::Basename;
use lib dirname($0);

use Getopt::Long;
use XML::Simple;
use Data::Dumper;

########################################################
# Driver for information collection
# Reads old style arc.config and prints out XML
########################################################

use base "InfoCollector";

use HostInfo;
use GMJobsInfo;
use LRMSInfo;

use ARC0ClusterInfo;
use ARC1ClusterInfo;

use ConfigParser;
use LogUtils; 

use strict;

our $log = LogUtils->getLogger(__PACKAGE__);


sub main {

    LogUtils->getLogger()->level($LogUtils::INFO);

    ########################################################
    # Config defaults
    ########################################################
    
    my %config = (
                   ttl            =>  600,
                   gm_mount_point => "/jobs",
                   gm_port        => 2811,
                   homogeneity    => "TRUE",
                   providerlog    => "/var/log/grid/infoprovider.log",
                   defaultttl     => "604800",
                   x509_user_cert => "/etc/grid-security/hostcert.pem",
                   x509_cert_dir  => "/etc/grid-security/certificates/",
                   ng_location    => $ENV{ARC_LOCATION} ||= "/usr/local",
                   gridmap        => "/etc/grid-security/grid-mapfile",
                   processes      => [qw(gridftpd grid-manager arched)]
    
    );


    ########################################################
    # Parse command line options
    ########################################################
    
    my $configfile;
    my $print_help;
    GetOptions("config:s" => \$configfile,
               "help|h"   => \$print_help ); 
        
    if ($print_help) { 
        print "Usage: $0 <options>
        --config   - location of arc.conf
        --help     - this help\n";
        exit 1;
    }
    
    unless ( $configfile ) {
        $log->error("a command line argument is missing, see --help ") and die;
    }
    
    ##################################################
    # Read ARC configuration
    ##################################################
    
    my $parser = ConfigParser->new($configfile)
        or $log->error("Cannot open $configfile") and die;
    %config = (%config, $parser->get_section('common'));
    %config = (%config, $parser->get_section('cluster'));
    %config = (%config, $parser->get_section('grid-manager'));
    %config = (%config, $parser->get_section('infosys'));
    
    $config{queues} = {};
    for my $qname ($parser->list_subsections('queue')) {
        my %queue_config = $parser->get_section("queue/$qname");
        $config{queues}{$qname} = \%queue_config;
    
        # put this option in the proper place
        if ($queue_config{maui_bin_path}) {
            $config{maui_bin_path} = $queue_config{maui_bin_path};
            delete $queue_config{maui_bin_path};
        }
        # maui should not be a per-queue option
        if (lc($queue_config{scheduling_policy}) eq 'maui') {
            $config{pbs_scheduler} = 'maui';
        }
    }
    
    # throw away default queue string following lrms
    ($config{lrms}) = split " ", $config{lrms};

    ##################################################
    # Collect information & print XML
    ##################################################

    my $cluster_info = ClusterInfo->new()->get_info(\%config);
    my $xml = new XML::Simple(NoAttr => 0, ForceArray => 1, RootName => 'arc:InfoRoot', KeyAttr => ['name']);
    print $xml->XMLout($cluster_info);
}
    

##################################################
# Unified information collector for NG and GLUE2
##################################################

my $config_schema = {
    lrms => '',              # name of the LRMS module
    gridmap => '',
    x509_user_cert => '',
    x509_cert_dir => '',
    sessiondir => '',
    cachedir => '*',
    ng_location => '',
    runtimedir => '*',
    processes => [ '' ],
    queues => {              # queue names are keys in this hash
        '*' => {
            name => '*'
        }
    }
};

# override InfoCollector base class methods

# only inputs will be validated
sub _get_options_schema {
    return $config_schema;
}
# results are not validated
sub _check_results($) {
}

sub _collect($$) {
    my ($self,$config) = @_;

    # get all local users from grid-map. Sort unique
    my %saw = ();
    my $usermap = read_grid_mapfile($config->{gridmap});
    my @localusers = grep !$saw{$_}++, values %$usermap;

    my $gmjobs_info = get_gmjobs_info($config);

    my @jobids;
    for my $job (values %$gmjobs_info) {
        # TODO: Uncomment later! Query all jobs for now
        #next unless $job->{status} eq 'INLRMS';
        next unless $job->{localid};
        push @jobids, $job->{localid};
    }

    my $rawdata = {};
    $rawdata->{config} = $config;
    $rawdata->{usermap} = $usermap;
    $rawdata->{host_info} = get_host_info($config,\@localusers);
    $rawdata->{gmjobs_info} = $gmjobs_info;
    $rawdata->{lrms_info} = get_lrms_info($config,\@localusers,\@jobids);

    my $cluster_info;
    $cluster_info->{'xmlns:arc'} = "urn:knowarc";
    $cluster_info->{'n:nordugrid'} = ARC0ClusterInfo->new()->get_info($rawdata);
    $cluster_info->{'Domains'}->{'xmlns'} = "http://schemas.ogf.org/glue/2008/05/spec_2.0_d41_r01";
    $cluster_info->{'Domains'}->{'xmlns:xsi'} = "http://www.w3.org/2001/XMLSchema-instance";
    $cluster_info->{'Domains'}->{'xsi:schemaLocation'} = "http://schemas.ogf.org/glue/2008/05/spec_2.0_d41_r01 pathto/GLUE2.xsd";
    $cluster_info->{'Domains'}->{'AdminDomain'}->{'xmlns'} = "http://schemas.ogf.org/glue/2008/05/spec_2.0_d41_r01";
    $cluster_info->{'Domains'}->{'AdminDomain'}->{'Services'}->{'xmlns'} = "http://schemas.ogf.org/glue/2008/05/spec_2.0_d41_r01";
    $cluster_info->{'Domains'}->{'AdminDomain'}->{'Services'}->{'Service'} = ARC1ClusterInfo->new()->get_info($rawdata);

    return $cluster_info;
}


##################################################
# Calling other InfoCollectors
##################################################

sub get_host_info($$) {
    my ($config,$localusers) = @_;

    my $host_opts = Storable::dclone($config);
    $host_opts->{localusers} = $localusers;

    return HostInfo->new()->get_info($host_opts);
}

sub get_gmjobs_info($) {
    my $config = shift;
    my $gmjobs_opts = { controldir => $config->{controldir} };
    return GMJobsInfo->new()->get_info($gmjobs_opts);
}

sub get_lrms_info($$$) {
    my ($config,$localusers,$jobids) = @_;

    # possibly any options from config are needed, so just clone it all
    my $lrms_opts = Storable::dclone($config);
    $lrms_opts->{jobs} = $jobids;

    my @queues = keys %{$lrms_opts->{queues}};
    for my $queue ( @queues ) {
        $lrms_opts->{queues}{$queue}{users} = $localusers;
    }
    return LRMSInfo->new()->get_info($lrms_opts);
}

#### reading grid-mapfile #####

sub read_grid_mapfile($) {
    my $gridmapfile = shift;
    my $usermap = {};

    unless (open MAPFILE, "<$gridmapfile") {
        $log->warning("can't open gridmapfile at $gridmapfile");
        return;
    }
    while (my $line = <MAPFILE>) {
        chomp($line);
        if ( $line =~ m/\"([^\"]+)\"\s+(\S+)/ ) {
            $usermap->{$1} = $2;
        }
    }
    close MAPFILE;

    return $usermap;
}


#### TEST ##### TEST ##### TEST ##### TEST ##### TEST ##### TEST ##### TEST ####

sub test {

    my $config = {
          #'lrms' => 'sge',
          'lrms' => 'fork',
          'pbs_log_path' => '/var/spool/pbs/server_logs',
          'pbs_bin_path' => '/opt/pbs',
          'sge_cell' => 'cello',
          'sge_root' => '/opt/n1ge6',
          'sge_bin_path' => '/opt/n1ge6/bin/lx24-x86',
          'll_bin_path' => '/opt/ibmll/LoadL/full/bin',
          'lsf_profile_path' => '/usr/share/lsf/conf',

          'hostname' => 'squark.uio.no',
          'opsys' => 'Linux-2.4.21-mypatch[separator]glibc-2.3.1[separator]Redhat-7.2',
          'architecture' => 'adotf',
          'nodememory' => '512',
          'middleware' => 'Cheese and Salad[separator]nordugrid-old-and-busted',

          'ng_location' => '/opt/nordugrid',
          'gridmap' => '/etc/grid-security/grid-mapfile',
          'controldir' => '/tmp/arc1/control',
          'sessiondir' => '/tmp/arc1/session',
          'runtimedir' => '/home/grid/runtime',
          'cachedir' => '/home/grid/cache',
          'cachesize' => '10000000000 8000000000',
          'authorizedvo' => 'developer.nordugrid.org',
          'x509_user_cert' => '/etc/grid-security/hostcert.pem',
          'x509_user_key' => '/etc/grid-security/hostkey.pem',
          'x509_cert_dir' => '/scratch/adrianta/arc1/etc/grid-security/certificates',

          'processes' => [ 'gridftpd', 'grid-manager', 'arched' ],
          'nodeaccess' => 'inbound[separator]outbound',
          'homogeneity' => 'TRUE',
          'gm_mount_point' => '/jobs',
          'gm_port' => '2811',
          'defaultttl' => '259200',
          'ttl' => '600',

          'mail' => 'v.a.taga@fys.uio.no',
          'cluster_alias' => 'Big Blue Cluster in Nowhere',
          'clustersupport' => 'v.a.taga@fys.uio.no',
          'cluster_owner' => 'University of Oslo',
          'cluster_location' => 'NO-xxxx',
          'comment' => 'test bed cluster',
          'benchmark' => 'SPECFP2000 333[separator]BOGOMIPS 1000',

          'queues' => {
                        'all.q' => {
                                     'name' => 'all.q',
                                     'sge_jobopts' => '-r yes',
                                     'nodecpu' => 'adotf',
                                     'opsys' => 'Mandrake 7.0',
                                     'architecture' => 'adotf',
                                     'nodememory' => '512',
                                     'comment' => 'Dedicated queue for ATLAS users'
                                   }
                      },
        };

    LogUtils->getLogger()->level($LogUtils::DEBUG);
    #$log->debug("From config file:\n". Dumper $config);
    my $cluster_info = ClusterInfo->new()->get_info($config);
    my $xml = new XML::Simple(NoAttr => 0, ForceArray => 1, RootName => 'arc:InfoRoot', KeyAttr => ['name']);
    print $xml->XMLout($cluster_info);
    #$log->debug("Cluster Info:\n". Dumper $cluster_info);
}


#test;
main;

1;
