package ARC0mod;

#
# Loads ARC0.6 LRMS modules for use with ARC1
#

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT_OK = qw(get_lrms_info get_lrms_options_schema);

use LogUtils;
use strict;

our $log = LogUtils->getLogger(__PACKAGE__);

our %modnames = ( PBS    => "PBS",
                  SGE    => "SGE",
                  LL     => "LL",
                  LSF    => "LSF",
                  CONDOR => "Condor",
                  FORK   => "Fork"
                );

sub load_lrms($) {
    my $lrms_name = uc(shift);
    my $module = $modnames{$lrms_name};
    $log->error("No ARC0 module for $lrms_name") unless $module;
    eval { require "$module.pm" };
    $log->error("LRMS module $module not found") if $@;
    import $module qw(cluster_info queue_info jobs_info users_info);
}

# Just generic options, cannot assume anything LRMS specific here

sub get_lrms_options_schema {
    return {
        'lrms' => '',              # name of the LRMS module
        'queues' => {              # queue names are keys in this hash
            '*' => {
                'users' => [ '' ]  # list of user IDs to query in the LRMS
            }
        },
        'jobs' => [ '' ]           # list of jobs IDs to query in the LRMS
    }
}


sub get_lrms_info($) {
    my $options = shift;

    my %cluster_config = %$options;
    delete $cluster_config{queues};
    delete $cluster_config{jobs};

    my $lrms_info = {cluster => {}, queues => {}, jobs => {}};

    my $cluster_info = { cluster_info(\%cluster_config) };
    delete $cluster_info->{queue};
    $lrms_info->{cluster} = delete_empty($cluster_info);

    my %queue_config = %{$options->{queues}};

    for my $qname ( keys %queue_config ) {

        my %queue_config = (%cluster_config, %queue_config);
        delete $queue_config{users};

        my $jids = $options->{jobs};

        # TODO: interface change: jobs under each queue
        my $jobs_info = { jobs_info(\%queue_config, $qname, $jids) };
        for my $job ( values %$jobs_info ) {
            $job->{status} ||= 'EXECUTED';
            delete_empty($job);
        }
        $lrms_info->{jobs} = { %{$lrms_info->{jobs}}, %$jobs_info };

        my $queue_info = { queue_info(\%queue_config, $qname) };
        $lrms_info->{queues}{$qname} = delete_empty($queue_info);

        my $users = $queue_config{$qname}{users};

        $queue_info->{users} = { users_info(\%queue_config, $qname, $users) };
        for my $user ( values %{$queue_info->{users}} ) {
            my $freecpus = $user->{freecpus};
            $user->{freecpus} = split_freecpus($freecpus) if defined $freecpus;
            delete_empty($user);
        }
        $queue_info->{acl_users} = $queue_config{acl_users}
            if defined $queue_config{acl_users};

    }
    return $lrms_info;
}


sub delete_empty($) {
    my $hashref = shift;
    foreach my $k ( keys %$hashref) {
        delete $hashref->{$k}
            if $hashref->{$k} eq '';
    }
    return $hashref;
}

sub split_freecpus($) {
    my $freecpus_string = shift;
    my $freecpus_hash = {};
    for my $countsecs (split ' ', $freecpus_string) {
        if ($countsecs =~ /^(\d+)(?::(\d+))?$/) {
            $freecpus_hash->{$1} = $2 || 0; # 0 means unlimited
        } else {
            $log->warning("Bad freecpus string: $freecpus_string");
            return {};
        }
    }
    return $freecpus_hash;
}

1;
