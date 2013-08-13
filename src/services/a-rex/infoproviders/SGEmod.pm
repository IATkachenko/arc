package SGEmod;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT_OK = qw(get_lrms_info get_lrms_options_schema);

use POSIX qw(floor ceil);
use LogUtils;
use XML::Simple qw(:strict);

use strict;

our $path;
our $options;
our $lrms_info = {};

# status of nodes and queues
our %node_stats = ();
# all running jobs, indexed by job-ID and task-ID
our %running_jobs = ();
# all waiting jobs, indexed by job-ID
our %waiting_jobs = ();

# Switch to choose between codepaths for SGE 6.x (the default) and SGE 5.x
our $compat_mode;

our $sge_type;
our $sge_version;
our $cpudistribution;
our @queue_names;

our $queuedjobs = 0;
our $queuedcpus = 0;

our $max_jobs;
our $max_u_jobs;
our %user_total_jobs;
our %user_waiting_jobs;

our $log = LogUtils->getLogger(__PACKAGE__);

##########################################
# Public interface
##########################################

sub get_lrms_options_schema {
    return {
            'sge_root'         => '',
            'sge_bin_path'     => '',
            'sge_cell'         => '*',
            'sge_qmaster_port' => '*',
            'sge_execd_port'   => '*',
            'queues' => {
                '*' => {
                    'users'       => [ '' ],
                    'sge_queues' => '*',
                    'sge_jobopts' => '*'
                }
            },
            'jobs' => [ '' ]
        };
}

sub get_lrms_info($) {

    $options = shift;

    lrms_init();
    type_and_version();
    run_qconf();
    run_qstat();

require Data::Dumper; import Data::Dumper qw(Dumper);
#print STDERR Dumper(\%node_stats);
#print STDERR Dumper(\%running_jobs);
#print STDERR Dumper(\%waiting_jobs);

    cluster_info();

    my %qconf = %{$options->{queues}};
    for my $qname ( keys %qconf ) {
        queue_info($qname);
    }

    my $jids = $options->{jobs};
    jobs_info($jids);

    for my $qname ( keys %qconf ) {
        my $users = $qconf{$qname}{users};
        users_info($qname,$users);
    }

    # recycle memory
    %running_jobs = ();
    %waiting_jobs = ();

    nodes_info();

    return $lrms_info
}

##########################################
# Private subs
##########################################

#
# Generic function to process the ouptut of an external program. The callback
# function will be invoked with a file descriptor receiving the standard output
# of the external program as it's first argument.
#   

sub run_callback {
    my ($command, $callback, @extraargs) = @_;
    my ($executable) = split ' ', $command;
    $log->error("Not an executable: $executable")
        unless (-x "$executable");
    local *QQ;
    $log->error("Failed creating pipe from: $command: $!")
        unless open QQ, "$command |";

    &$callback(*QQ, @extraargs);

    close QQ;
    my $exitcode = $? >> 8;
    $log->info("Failed running command (exit code $exitcode): $command")
        if $?;
    return ! $?;
}

#
# Generic function to process the ouptut of an external program. The callback
# function will be invoked for each line of output from the external program.
#

sub loop_callback {
    my ($command, $callback) = @_;
    return run_callback($command, sub {
            my $fh = shift;
            my $line;
            chomp $line and &$callback($line) while defined ($line = <$fh>);
    });
    
}

#
# Determine SGE variant and version.
# Set compatibility mode if necessary.
#

sub type_and_version {
    run_callback("$path/qstat -help", sub {
        my $fh = shift;
        my ($firstline) = <$fh>;
        ($sge_type, $sge_version) = split " ", $firstline;
    });
   
    $compat_mode = 0; # version 6.x assumed
   
    if ($sge_type !~ /GE/ or not $sge_version) {
        $log->error("Cannot indentify SGE version from output of '$path/qstat -help': $sge_type $sge_version");
    } elsif ($sge_version =~ /^5\./ or $sge_version =~ /^pre6.0/) {
        $compat_mode = 1;
        $log->info("Using SGE 5.x compatibility mode");
    }
}

#
# Processes an array task definition (i.e.: '3,4,6-8,10-20:2')
# and returns the number of individual tasks
#

sub count_array_spec($) {
    my $count = 0;
    for my $spec (split ',', shift) {
        # handles expressions like '6-10:2' and '6-10' and '6'
        return 0 unless $spec =~ '^(\d+)(?:-(\d+)(?::(\d+))?)?$';
        my ($lower,$upper,$step) = ($1,$2,$3);
        $upper = $lower unless defined $upper;
        $step = 1 unless $step;
        $count += 1 + floor(abs($upper-$lower)/$step);
    }
    return $count;
}

#
# this block contains the functions used to parse the output of qstat
#

{
    # shared variables

    my $line; # used to keep the most recently line read from qstat

    my $currentjobid = undef;
    my $currentqueue = undef;
    my $currentnode  = undef;

    #### Regular expression matching a queue line, like:
    # libero@compute-3-7.local       BPC   0/8       4.03     lx24-amd64    S
    # all.q@hyper.uio.no             BIP   0/0/1     0.00     lx24-x86
    # all.q@compute-14-19.local      BIPC  0/8       -NA-     lx24-amd64    Adu
    # corvus.q             BICP  0/16      99.99    solaris64 aAdu
    my $queue_regex = '^\s*(\S+)\s+\w+\s+(?:(\d+)/)?(\d+)/(\d+)\s+(\S+)\s+\S+(?:\s+(\w+))?\s*$';

    #### Regular expression matching complex lines from qstat -F
    #         hl:num_proc=1
    #         hl:mem_total=1009.523M
    #         qf:qname=shar
    #         qf:hostname=squark.uio.no
    my $complex_regex = '\s+(\w\w:\w+)=(.*)\s*';

    #### Regular expressions matching jobs (for SGE version 6.x), like:
    #  4518 2.71756 brkhrt_5ch whe042   r   08/20/2008 10:50:23  4
    #  1602 2.59942 runmain_4_ user1    r   08/13/2008 22:42:17  1 21
    #  1872 2.59343 test_GG1   otherusr Eqw 08/05/2008 17:36:45  1 4,6-8:1
    #  7539 7.86785 methane_i  user11   qw  06/26/2008 11:16:52  4
    #### Assume job name column is exactly 10 characters wide.
    my $jobid_prio_name_regex6 = '(\d+)\s+[.\d]+\s+\S.{9}';
    my $user_state_date_regex6 = '(\S+)\s+(\w+)\s+(\d\d/\d\d/\d{4} \d\d:\d\d:\d\d)';
    my $slots_tid_regex6 = '(\d+)(?:\s+(\d+))?'; # for running jobs
    my $slots_taskdef_regex6   = '(\d+)(?:\s+([:\-\d,]+))?';    # for queued jobs
    my $base_regex6 = '^\s*'.$jobid_prio_name_regex6.' '.$user_state_date_regex6;
    my $running_regex6 = $base_regex6.'\s+'.$slots_tid_regex6.'\s*$';
    my $waiting_regex6 = $base_regex6.'\s+'.$slots_taskdef_regex6.'\s*$';
    
    #### Regular expressions matching jobs (for SGE version 5.x), like:
    # 217  0 submit.tem lemlib r   07/21/2008 09:55:32 MASTER
    #      0 submit.tem lemlib r   07/21/2008 09:55:32 SLAVE
    #  27  0 exam.sh    c01p01 r   02/03/2006 16:40:49 MASTER 2
    #      0 exam.sh    c01p01 r   02/03/2006 16:40:49 SLAVE 2
    # 254  0 CPMD       baki   qw  08/14/2008 10:12:29
    # 207  0 STDIN      adi    qw  08/15/2008 17:23:37         2-10:2
    #### Assume job name column is exactly 10 characters wide.
    my $jobid_prio_name_regex5 = '(?:(\d+)\s+)?[.\d]+\s+\S.{9}';
    my $user_state_date_regex5 = '(\S+)\s+(\w+)\s+(\d\d/\d\d/\d{4} \d\d:\d\d:\d\d)';
    my $master_tid_regex5 = '(MASTER|SLAVE)(?:\s+(\d+))?'; # for running jobs
    my $taskdef_regex5   = '(?:\s+([:\-\d,]+))?';    # for queued jobs
    my $base_regex5 = '^\s*'.$jobid_prio_name_regex5.' '.$user_state_date_regex5;
    my $running_regex5 = $base_regex5.'\s+'.$master_tid_regex5.'\s*$';
    my $waiting_regex5 = $base_regex5.$taskdef_regex5.'\s*$';


    sub run_qstat {
        my $command = "$path/qstat -u '*'";
        $command .= $compat_mode ? " -F" : " -f";
        die unless run_callback($command, \&qstat_parser_callback);
    }


    sub qstat_parser_callback {
        my $fh = shift;

        # validate header line
        $line = <$fh>;
        return unless defined $line; # if there was no output
        my @hdr = split ' ',$line;
        $log->error("qstat header line not recognized")
            unless ($hdr[0] eq 'queuename');
        
        $line = <$fh>;
        while (defined $line and $line =~ /^--------------/) {
            handle_queue($fh);
            handle_running_jobs($fh);
        }
        return unless defined $line; # if there are no waiting jobs

        $line = <$fh>; $log->error("Unexpected line from qstat") unless $line =~ /############/;
        $line = <$fh>; $log->error("Unexpected line from qstat") unless $line =~ /PENDING JOBS/;
        $line = <$fh>; $log->error("Unexpected line from qstat") unless $line =~ /############/;
        $line = <$fh>;
        handle_waiting_jobs($fh);
    
        # there should be no lines left
        $log->error("Cannot parse qstat output line: $line")
            if defined $line;
    }


    sub handle_queue {
        my $fh = shift;
        $line = <$fh>;

        if (defined $line and $line =~ /$queue_regex/) {

            my ($qname,$used,$total,$load,$flags) = ($1,$3,$4,$5,$6||'');
            $line = <$fh>;

            if (not $compat_mode) {
                ($currentqueue, $currentnode) = split '@',$qname,2;
                unless ($currentnode) {
                    $log->error("Queue name of the form 'queue\@host' expected. Got: $qname");
                }
            } else {
                $currentqueue = $qname;
                # parse complexes to extract hostname
                while (defined $line and $line =~ /$complex_regex/) {
                    $currentnode = $2 if $1 eq 'qf:hostname';
                    $line = <$fh>;
                }
                $log->warning("Could not extract hostname for queue $qname") unless $currentnode;
            }
            if ($currentnode) {
                # Was this node not listed with qhost -xml ?
                if (not exists $node_stats{$currentnode} or
                    not exists $node_stats{$currentnode}{totalcpus}) {
                    # Node name may have been truncated by qstat -f
                    if (length $qname >= 30) {
                        # Try to match it with a node already listed by qhost -xml
                        my @fullnames = grep { length($_) >= length($currentnode)
                                                   and $_ =~ m/^\Q$currentnode\E/
                                        } grep { exists $node_stats{$_}{totalcpus}
                                        } keys %node_stats;
                        $currentnode = $fullnames[0] if @fullnames == 1;
                    }
                    # Node name may have been truncated by qhost -xml
                    for (my $name = $currentnode; length $name >= 24; chop $name) {
                        $currentnode = $name if exists $node_stats{$name}
                                            and exists $node_stats{$name}{totalcpus}
                    }
                }
                if (not exists $node_stats{$currentnode} or
                    not exists $node_stats{$currentnode}{totalcpus}) {
                    $log->warning("Queue $currentqueue\@$currentnode cannot be matched"
                                  ." with a hostname listed by qhost -xml");
                }
                $node_stats{$currentnode}{load} = $load unless $load eq '-NA-';
                $node_stats{$currentnode}{runningslots} ||= 0; # will be counted later
                $node_stats{$currentnode}{queues}{$currentqueue}
                         = {usedslots=>$used, totalslots=>$total, suspslots=>'0', flags=>$flags};
            }
        }
    }

    # Running jobs in a queue instance

    sub handle_running_jobs {
        my $fh = shift;

        my $regex = $compat_mode ? $running_regex5 : $running_regex6;

        while (defined $line and $line =~ /$regex/) {

            if (not $compat_mode) { ### SGE v 6.x ###

                my ($jobid,$user,$slots,$taskid) = ($1,$2,$5,$6);
                $taskid = 0 unless $taskid; # 0 is an invalid task id
                # Index running jobs by job-ID and task-ID
                my $task = $running_jobs{$jobid}{$taskid} || {};
                $running_jobs{$jobid}{$taskid} = $task;
                $user_total_jobs{$user}++;
                $task->{user} = $user;
                $task->{state} = $3;
                $task->{date} = $4;
                $task->{queue} = $currentqueue;
                $task->{nodes}{$currentnode} = $slots;
                $task->{slots} += $slots;
                if ($task->{state} =~ /[sST]/) {
                    $node_stats{$currentnode}{queues}{$currentqueue}{suspslots} += $slots;
                } else {
                    $node_stats{$currentnode}{runningslots} += $slots;
                }

            } else { ### SGE 5.x, pre 6.0 ###

                my ($jobid,$user,$role,$taskid) = ($1,$2,$5,$6);
                $taskid = 0 unless $taskid; # 0 is an invalid task id
                if ($role eq 'MASTER' and not defined $jobid) {
                    $log->error("Cannot parse qstat output line: $line");
                } elsif (not defined $jobid) {
                    $jobid = $currentjobid;
                } else {
                    $currentjobid = $jobid;
                }
                # Index running jobs by job-ID and task-ID
                my $task = $running_jobs{$jobid}{$taskid} || {};
                $running_jobs{$jobid}{$taskid} = $task;
                if ($role eq 'MASTER') {        # each job has one MASTER
                    $user_total_jobs{$user}++;
                    $task->{user} = $user;
                    $task->{state} = $3;
                    $task->{date} = $4;
                    $task->{queue} = $currentqueue;
                    $task->{slots}++;
                    $task->{nodes}{$currentnode}++;
                    $task->{is_parallel} = 0;
                    if ($task->{state} =~ /[sST]/) {
                        $node_stats{$currentnode}{queues}{$currentqueue}{suspslots}++;
                    } else {
                        $node_stats{$currentnode}{runningslots}++;
                    }
                } elsif (not $task->{is_parallel}) {  # Fist SLAVE following the MASTER
                    $task->{is_parallel} = 1;         # Don't count this SLAVE
                } else {                              # Other SLAVEs, resume counting
                    $task->{slots}++;
                    $task->{nodes}{$currentnode}++;
                    if ($task->{state} =~ /[sST]/) {
                        $node_stats{$currentnode}{queues}{$currentqueue}{suspslots}++;
                    } else {
                        $node_stats{$currentnode}{runningslots}++;
                    }
                }
            }
            last unless defined ($line = <$fh>);
        }
    }


    sub handle_waiting_jobs {
        my $fh = shift;

        my $rank = 1;
        my $regex = $compat_mode ? $waiting_regex5 : $waiting_regex6;

        while (defined $line and $line =~ /$regex/) {

            if (not $compat_mode) { ### SGE v 6.x ###

                my ($jobid,$user,$taskdef) = ($1,$2,$6);
                my $ntasks = $taskdef ? count_array_spec($taskdef) : 1;
                unless ($ntasks) {
                    $log->error("Failed parsing task definition: $taskdef");
                }
                $waiting_jobs{$jobid}{user} = $user;
                $waiting_jobs{$jobid}{state} = $3;
                $waiting_jobs{$jobid}{date} = $4;
                $waiting_jobs{$jobid}{slots} = $5;
                $waiting_jobs{$jobid}{tasks} += $ntasks;
                $waiting_jobs{$jobid}{rank} = $rank;
                $user_total_jobs{$user} += $ntasks;
                $user_waiting_jobs{$user} += $ntasks;
                $rank += $ntasks;

            } else { ### SGE 5.x, pre 6.0 ###

                my ($jobid,$user,$taskdef) = ($1,$2,$5);
                my $ntasks = $taskdef ? count_array_spec($taskdef) : 1;
                unless ($ntasks) {
                    $log->error("Failed parsing task definition: $taskdef");
                }
                # The number of slots is not available from qstat output.
                $waiting_jobs{$jobid}{user} = $user;
                $waiting_jobs{$jobid}{state} = $3;
                $waiting_jobs{$jobid}{date} = $4;
                $waiting_jobs{$jobid}{tasks} += $ntasks;
                $waiting_jobs{$jobid}{rank} = $rank;
                # SGE 5.x does not list number of slots. Assuming 1 slot per job!
                $waiting_jobs{$jobid}{slots} = 1;
                $user_total_jobs{$user} += $ntasks;
                $user_waiting_jobs{$user} += $ntasks;
                $rank += $ntasks;
            }
            last unless defined ($line = <$fh>);
        }
    }

} # end of qstat block



sub run_qconf {

    # cpu distribution
    $cpudistribution = '';

    # qconf -sep deprecated therefore we are using qhost -xml
    my $qhost_xml_output = `$path/qhost -xml` or $log->error("Failed listing licensed processors");
    use XML::Simple qw(:strict);
    my $xml = XMLin($qhost_xml_output, KeyAttr => { host => 'name' }, ForceArray => [ 'host' ]);
    for my $h ( keys %{$xml->{host}} ) {
            next if $h eq "global";
            $node_stats{$h}{arch} =$xml->{host}{$h}{"hostvalue"}[0]{content};
            $node_stats{$h}{totalcpus} = $xml->{host}{$h}{"hostvalue"}[1]{content};
    } 

    my %cpuhash;
    $cpuhash{$_->{totalcpus}}++ for values %node_stats;
    while ( my ($cpus,$count)  = each %cpuhash ) {
        $cpudistribution .= "${cpus}cpu:$count " if $cpus > 0;
    }
    chop $cpudistribution;

    # global limits
    loop_callback("$path/qconf -sconf global", sub {
        my $l = shift;
        $max_jobs = $1 if $l =~ /^max_jobs\s+(\d+)/;
        $max_u_jobs = $1 if $l =~ /^max_u_jobs\s+(\d+)/;
    }) or $log->error("Failed listing global configurations");

    # list all queues
    loop_callback("$path/qconf -sql", sub {
        push @queue_names, shift
    }) or $log->error("Failed listing all queues");
    chomp @queue_names;
}

sub req_limits ($) {
    my $line = shift;
    my ($reqcputime, $reqwalltime);
    while ($line =~ /[sh]_cpu=(\d+)/g) {
        $reqcputime = $1 if not $reqcputime or $reqcputime > $1;
    }
    while ($line =~ /[sh]_rt=(\d+)/g) {
        $reqwalltime = $1 if not $reqwalltime or $reqwalltime > $1;
    }
    return ($reqcputime, $reqwalltime);
}

sub lrms_init() {
    $ENV{SGE_ROOT} = $options->{sge_root} || $ENV{SGE_ROOT};
    $log->error("could not determine SGE_ROOT") unless $ENV{SGE_ROOT};

    $ENV{SGE_CELL} = $options->{sge_cell} || $ENV{SGE_CELL} || 'default';
    $ENV{SGE_QMASTER_PORT} = $options->{sge_qmaster_port} if $options->{sge_qmaster_port};
    $ENV{SGE_EXECD_PORT} = $options->{sge_execd_port} if $options->{sge_execd_port};

    for (split ':', $ENV{PATH}) {
        $ENV{SGE_BIN_PATH} = $_ and last if -x "$_/qsub";
    }
    $ENV{SGE_BIN_PATH} = $options->{sge_bin_path} || $ENV{SGE_BIN_PATH};

    $log->error("SGE executables not found") unless -x "$ENV{SGE_BIN_PATH}/qsub";

    $path = $ENV{SGE_BIN_PATH};
}


sub cluster_info () {

    my $lrms_cluster = {};

    # add this cluster to the info tree
    $lrms_info->{cluster} = $lrms_cluster;

    # Figure out SGE type and version

    $lrms_cluster->{lrms_glue_type} = "sungridengine";
    $lrms_cluster->{lrms_type} = $sge_type;
    $lrms_cluster->{lrms_version} = $sge_version;

    $lrms_cluster->{cpudistribution} = $cpudistribution;
    $lrms_cluster->{totalcpus} += $_->{totalcpus} || 0 for values %node_stats;

    # Count used/free CPUs and queued jobs in the cluster
    
    # Note: SGE has the concept of "slots", which roughly corresponds to
    # concept of "cpus" in ARC (PBS) LRMS interface.

    my $usedcpus = 0;
    my $runningjobs = 0;
    for my $tasks (values %running_jobs) {
        for my $task (values %$tasks) {
            $runningjobs++;
            # Skip suspended jobs
            $usedcpus += $task->{slots}
                unless $task->{state} =~ /[sST]/;
         }
    }

    for my $job (values %waiting_jobs) {
        $queuedjobs += $job->{tasks};
        $queuedcpus += $job->{tasks} * $job->{slots};
    }

    $lrms_cluster->{usedcpus} = $usedcpus;
    $lrms_cluster->{queuedcpus} = $queuedcpus;
    $lrms_cluster->{queuedjobs} = $queuedjobs;
    $lrms_cluster->{runningjobs} = $runningjobs;

    # List LRMS queues
    #$lrms_cluster->{queue} = [ @queue_names ];
}


sub queue_info ($) {
    my $qname = shift;

    my $lrms_queue = {};

    # add this queue to the info tree
    $lrms_info->{queues}{$qname} = $lrms_queue;

    # multiple (even overlapping) queues are supported.

    my @qnames = ($qname);

    # This code prepares for a scenario where grid jobs in a ComputingShare are
    # submitted by a-rex to thout requesting specific queue. Jobs in can end up
    # in several possible queues. This function should then try to agregate
    # values over all the queues in a list.
    # OBS: more work is needed to make this work
    if ($options->{queues}{$qname}{sge_queues}) {
        @qnames = split ' ', $options->{queues}{$qname}{sge_queues};
    }

    # NOTE:
    # In SGE the relation between CPUs and slots is quite elastic. Slots is
    # just one of the complexes that the SGE scheduler takes into account. It
    # is quite possible (depending on configuration permits) to have more slots
    # used by jobs than total CPUs on a node.  On the other side, even if there
    # are unused slots in a queue, jobs might be prevented to start because of
    # some other constraints.

    # queuestatus - will be negative only if all queue instances have a status
    #               flag set other than 'a'.
    # queueused   - sum of slots used by jobs, not including suspended jobs.
    # queuetotal  - sum of slots limited by the number of cpus on each node.
    # queuefree   - attempt to calculate free slots.

    my $queuestatus = -1;
    my $queuetotal = 0;
    my $queuefree = 0;
    my $queueused = 0;
    for my $nodename (keys %node_stats) {
        my $node = $node_stats{$nodename};
        my $queues = $node->{queues};
        next unless defined $queues;
        my $nodetotal = 0; # number of slots on this node in the selected queues
        my $nodemax = 0; # largest number of slots in any of the selected queues
        my $nodefree = 0;
        my $nodeused = 0;
        for my $name (keys %$queues) {
            next unless grep {$name eq $_} @qnames;
            my $q = $queues->{$name};
            $nodetotal += $q->{totalslots};
            $nodemax = $q->{totalslots} if $nodemax < $q->{totalslots};
            $nodeused += $q->{usedslots} - $q->{suspslots};
            # Any flag on the queue implies that the queue is not taking more jobs.
            $nodefree += $q->{totalslots} - $q->{usedslots} unless $q->{flags};
            # The queue is healty if there is an instance in any other states
            # than normal or (a)larm. See man qstat for the meaning of the flags.
            $queuestatus = 1 unless $q->{flags} =~ /[dosuACDE]/;
        }
        # Cheating a bit here. SGE's scheduler would consider load averages
        # among other things to decide if there are free slots.
        if (defined $node->{totalcpus}) {
            my $maxslots = $node->{totalcpus};
            if ($nodetotal > $maxslots) {
                $log->debug("Capping nodetotal ($nodename): $nodetotal > ".$maxslots);
                $nodetotal = $maxslots;
            }
            if ($nodefree > $maxslots - $node->{runningslots}) {
                $log->debug("Capping nodefree ($nodename): $nodefree > ".$maxslots." - ".$node->{runningslots});
                $nodefree = $maxslots - $node->{runningslots};
                $nodefree = 0 if $nodefree < 0;
            }
        } else {
            $log->info("Node not listed by qhost -xml: $nodename");
        }
        $queuetotal += $nodetotal;
        $queuefree += $nodefree;
        $queueused += $nodeused;
    }

    $lrms_queue->{totalcpus} = $queuetotal;
    #$lrms_queue->{freecpus} = $queuefree;
    $lrms_queue->{running} = $queueused;
    $lrms_queue->{status} = $queuestatus;
    $lrms_queue->{MaxSlotsPerJob} = $queuetotal;

    # settings in the config file override
    my $qopts = $options->{queues}{$qname};
    $lrms_queue->{totalcpus} = $qopts->{totalcpus} if $qopts->{totalcpus};

    # reserve negative numbers for error states
    $log->warning("Negative status for queue $qname: $lrms_queue->{status}")
        if $lrms_queue->{status} < 0;

    # Grid Engine has hard and soft limits for both CPU time and
    # wall clock time. Nordugrid schema only has CPU time.
    # The lowest of the 2 limits is returned by this code.

    # This code breaks if there are some nodes with separate limits:
    # h_rt                  48:00:00,[cpt.uio.no=24:00:00]

    my $command = "$path/qconf -sq @qnames";
    loop_callback($command, sub {
        my $l = shift;
        if ($l =~ /^[sh]_rt\s+(\S+)/) {
            return if $1 eq 'INFINITY';
            my $timelimit;
            if ($1 =~ /^(\d+):(\d+):(\d+)$/) {
                my ($h,$m,$s) = ($1,$2,$3);
                $timelimit = $s + 60 * ($m + 60 * $h);
            } else {
                $log->warning("Error extracting time limit from line: $l");
                return;
            }
            if (not defined $lrms_queue->{maxwalltime}
                         or $lrms_queue->{maxwalltime} > $timelimit) {
                $lrms_queue->{maxwalltime} = $timelimit;
            }
        }
        elsif ($l =~ /^[sh]_cpu\s+(\S+)/) {
            return if $1 eq 'INFINITY';
            my $timelimit;
            if ($1 =~ /^(\d+):(\d+):(\d+)$/) {
                my ($h,$m,$s) = ($1,$2,$3);
                $timelimit = $s + 60 * ($m + 60 * $h);
            } else {
                $log->warning("Error extracting time limit from line: $l");
                return;
            }
            if (not defined $lrms_queue->{maxcputime}
                         or $lrms_queue->{maxcputime} > $timelimit) {
                $lrms_queue->{maxcputime} = $timelimit;
            }
        }
    }) or $log->error("Failed listing named queues");

    # Grid Engine puts queueing jobs in single "PENDING" state pool,
    # so here we report the total number queueing jobs in the cluster.

    $lrms_queue->{queued} = $queuedjobs;

    # nordugrid-queue-maxrunning
    # nordugrid-queue-maxqueuable
    # nordugrid-queue-maxuserrun

    # The total max running jobs is the number of slots for this queue
    $lrms_queue->{maxrunning} = $lrms_queue->{totalcpus};

    # SGE has a global limit on total number of jobs, but not per-queue limit.
    # This global limit gives an upper bound for maxqueuable and maxrunning
    if ($max_jobs) {
        $lrms_queue->{maxqueuable} = $max_jobs if $max_jobs;
        $lrms_queue->{maxrunning} = $max_jobs if $lrms_queue->{maxrunning} > $max_jobs;
    }

    if (defined $max_u_jobs and defined $lrms_queue->{maxuserrun} and $lrms_queue->{maxuserrun} > $max_u_jobs) {
        $lrms_queue->{maxuserrun} = $max_u_jobs;
    }
}


sub jobs_info ($) {

    # LRMS job IDs from Grid Manager
    my $jids = shift;

    my $lrms_jobs = {};

    # add jobs to the info tree
    $lrms_info->{jobs} = $lrms_jobs;

    my ($job, @running, @queueing);

    # loop through all requested jobs
    for my $jid (@$jids) {

        if (defined $running_jobs{$jid} and not defined $running_jobs{$jid}{0}) {
            $log->warning("SGE job $jid is an array job. Unable to handle it");

        } elsif (defined ($job = $running_jobs{$jid}{0})) {
            push @running, $jid;

            my $user = $job->{user};
            $user_total_jobs{$user}++;

            # OBS: it's assumed that jobs in this loop are not part of array
            # jobs, which is true for grid jobs (non-array jobs have taskid 0)

            if ($job->{state} =~ /[rt]/) {
                # running or transfering
                $lrms_jobs->{$jid}{status} = 'R';
            } elsif ($job->{state} =~ /[sST]/) {
                # suspended
                $lrms_jobs->{$jid}{status} = 'S';
            } else {
                # Shouldn't happen
                $lrms_jobs->{$jid}{status} = 'O';
                push @{$lrms_jobs->{$jid}{comment}}, "Unexpected SGE state: $job->{state}";
                $log->warning("SGE job $jid is in an unexpected state: $job->{state}");
            }
            # master node for parallel runs
            my ($cluster_queue, $exec_host) = split '@', $job->{queue};
            $lrms_jobs->{$jid}{nodes} = [ $exec_host ] if $exec_host;
            $lrms_jobs->{$jid}{cpus} = $job->{slots};

        } elsif (defined ($job = $waiting_jobs{$jid})) {
            push @queueing, $jid;

            $lrms_jobs->{$jid}{rank} = $job->{rank};

            # Old SGE versions do not list the number of slots for queing jobs
            $lrms_jobs->{$jid}{cpus} = $job->{slots} if not $compat_mode;
    
            if ($job->{state} =~ /E/) {
                # DRMAA: SYSTEM_ON_HOLD ?
                # TODO: query qacct for error msg
                $lrms_jobs->{$jid}{status} = 'O';
            } elsif ($job->{state} =~ /h/) {
                # Job is on hold
                $lrms_jobs->{$jid}{status} = 'H';
            } elsif ($job->{state} =~ /w/ and $job->{state} =~ /q/) {
                # Normally queued
                $lrms_jobs->{$jid}{status} = 'Q';
            } else {
                # Shouldn't happen
                $lrms_jobs->{$jid}{status} = 'O';
                push @{$lrms_jobs->{$jid}{comment}}, "Unexpected SGE state: $job->{state}";
                $log->warning("SGE job $jid is in an unexpected state: $job->{state}");
            }
        } else {

            # The job has finished.
            # Querying accounting system is slow, so we skip it for now.
            # That will be done by scan-sge-jobs.
        
            $log->debug("SGE job $jid has finished");
            $lrms_jobs->{$jid}{status} = 'EXECUTED';
            $lrms_jobs->{$jid}{comment} = [];
        }
    }

    my $jid;
    
    # Running jobs

    $jid = undef;
    my ($jidstr) = join ',', @running;
    loop_callback("$path/qstat -j $jidstr", sub {
        my $l = shift;
        if ($l =~ /^job_number:\s+(\d+)/) {
            $jid=$1;
        }
        elsif ($l =~ /^usage/) {
            # OBS: array jobs have multiple 'usage' lines, one per runnig task

            # Memory usage in kB
            # SGE reports vmem and maxvmem.
            # maxvmem chosen here
            if ($l =~ /maxvmem=(\d+(?:\.\d+)?)\s*(\w)/) {
                my $mult = 1024; 
                if ($2 eq "M") {$mult = 1024} 
                if ($2 eq "G") {$mult = 1024*1024} 
                $lrms_jobs->{$jid}{mem} = int($mult*$1);
            }
            # used cpu time in minutes
            if ($l =~ /cpu=(?:(\d+):)?(\d+):(\d\d):(\d\d)/) {
                my ($d,$h,$m,$s) = ($1||0,$2,$3,$4);
                my $cputime = $s + 60*($m + 60*($h + 24*$d));
                $lrms_jobs->{$jid}{cputime} = $cputime;
            }
        }
        elsif ($l =~ /^hard resource_list/) {
            my ($reqcputime, $reqwalltime) = req_limits($l);
            $lrms_jobs->{$jid}{reqcputime} = $reqcputime if $reqcputime;
            $lrms_jobs->{$jid}{reqwalltime} = $reqwalltime if $reqwalltime;
        }
    }) or $log->warning("Failed listing named jobs");

    # Waiting jobs

    $jidstr = join ',', @queueing;
    $jid = undef;
    loop_callback("$path/qstat -j $jidstr", sub {
        my $l = shift;
        if ($l =~ /^job_number:\s+(\d+)/) {
            $jid=$1;
        }
        elsif ($l =~ /^hard resource_list/) {
            my ($reqcputime, $reqwalltime) = req_limits($l);
            $lrms_jobs->{$jid}{reqcputime} = $reqcputime if $reqcputime;
            $lrms_jobs->{$jid}{reqwalltime} = $reqwalltime if $reqwalltime;
        }
        elsif ($l =~ /^\s*(cannot run because.*)/) {
            # Reason for being held in queue
            push @{$lrms_jobs->{$jid}{comment}}, "LRMS: $1";
        }
        # Look for error messages, often jobs pending in error state 'Eqw'
        elsif ($l =~ /^error reason\s*\d*:\s*(.*)/) {
            # for SGE version 6.x. Examples:
            # error reason  1:  can't get password entry for user "grid". Either the user does not exist or NIS error!
            # error reason  1:  08/20/2008 13:40:27 [113794:25468]: error: can't chdir to /some/dir: No such file or directory
            # error reason  1:          fork failed: Cannot allocate memory
            #               1:          fork failed: Cannot allocate memory
            push @{$lrms_jobs->{$jid}{comment}}, "SGE job state was Eqw. LRMS error message was: $1";
            loop_callback("$path/qdel -fj $jidstr", sub {})
        }
        elsif ($l =~ /(job is in error state)/) {
            # for SGE version 5.x.
            push @{$lrms_jobs->{$jid}{comment}}, "SGE job state was Eqw. LRMS error message was: $1";
            loop_callback("$path/qdel -fj $jidstr", sub {})

            # qstat is not informative. qacct would be a bit more helpful with
            # messages like:
            # failed   1  : assumedly before job
            # failed   28 : changing into working directory
        }
    }) or $log->warning("Failed listing named jobs");
}


sub users_info($$) {
    my ($qname, $accts) = @_;

    my $lrms_users = {};

    # add users to the info tree
    my $lrms_queue = $lrms_info->{queues}{$qname};
    $lrms_queue->{users} = $lrms_users;

    # freecpus
    # queue length
    #
    # This is hard to implement correctly for a complex system such as SGE.
    # Using simple estimate.

    my $freecpus = 0;
    foreach my $u ( @{$accts} ) {
        if ($max_u_jobs) {
            $user_total_jobs{$u} = 0 unless $user_total_jobs{$u};
            $freecpus = $max_u_jobs - $user_total_jobs{$u};
            $freecpus = $lrms_queue->{status}
                if $lrms_queue->{status} < $freecpus;
        } else {
            $freecpus = $lrms_queue->{status};
        }
   $lrms_queue->{minwalltime} = 0;
   $lrms_queue->{mincputime} = 0;

        $lrms_users->{$u}{queuelength} = $user_waiting_jobs{$u} || 0;
        $freecpus = 0 if $freecpus < 0;
        if ($lrms_queue->{maxwalltime}) {
            $lrms_users->{$u}{freecpus} = { $freecpus => $lrms_queue->{maxwalltime} };
        } else {
            $lrms_users->{$u}{freecpus} = { $freecpus => 0 }; # unlimited
        }
    }
}

sub run_qhost {
   my ($host) = @_;
   my $result = {};

   #require Data::Dumper; import Data::Dumper qw(Dumper);
   #print STDERR Dumper($host);

   loop_callback("$path/qhost -F -h `echo $host | cut -d . -f 1` | grep '='", sub {
        my $l = shift;
        my ($prefix, $value ) = split ":", $l;
        if ( $value =~ /^mem_total=(\d+(?:\.\d+)?)\s*(\w)/) {
           my $mult = 1;
           if ($2 eq "M") {$mult = 1024}
           if ($2 eq "G") {$mult = 1024*1024}
           $result->{$host}{pmem} = int($mult*$1);
        }
        elsif ( $value =~ /^virtual_total=(\d+(?:\.\d+)?)\s*(\w)/) {
           my ($mult) = 1;
           if ($2 eq "M") {$mult = 1024}
           if ($2 eq "G") {$mult = 1024*1024}
           $result->{$host}{vmem} = int($mult*$1);
        }
        elsif ( $value =~ /^m_socket=(\d+(?:\.\d+)?)/) {
           $result->{$host}{nsock} = int($1);
        }
   }) or $log->error("Failed listing host attributes");

   return $result;
}

sub nodes_info {

   require Data::Dumper; import Data::Dumper qw(Dumper);

   my $lrms_nodes = {};

   # add nodes to the info tree
   $lrms_info->{nodes} = $lrms_nodes;

   for my $host (keys %node_stats) {
      my $node = $node_stats{$host};
      my $queues = $node->{queues};
      next unless defined $queues;
      my $arc_queue = 0;
      for my $qname1 (keys %$queues) {
          for my $qname2 ( keys %{$options->{queues}}) {
             if ($qname1 =~ $qname2 ) {$arc_queue = 1;}
          }
      }

      if ($arc_queue == 0) {next;}

      $lrms_nodes->{$host}{lcpus} = $node_stats{$host}{totalcpus};
      $lrms_nodes->{$host}{slots} = $node_stats{$host}{totalcpus};

      my $pmem = run_qhost($host);
      my $vmem = run_qhost($host);
      my $nsock = run_qhost($host);

      $lrms_nodes->{$host}{pmem} = $pmem->{$host}{pmem};
      $lrms_nodes->{$host}{vmem} = $vmem->{$host}{vmem};
      $lrms_nodes->{$host}{nsock} = $nsock->{$host}{nsock};

      # TODO
      $lrms_nodes->{$host}{isfree} = 1;
      $lrms_nodes->{$host}{isavailable} = 1;
      # $lrms_nodes->{$host}{tags} =
      # $lrms_nodes->{$host}{release} =
      #my %system = qw(lx Linux sol SunOS darwin Darwin);
      #my %machine = qw(amd64 x86_64 x86 i686 ia64 ia64 ppc ppc sparc sparc sparc64 sparc64);
      #if ($node_stats{$host}{arch} =~ /^(lx|sol|darwin)-(amd64|x86|ia64|ppc|sparc|sparc64)$/) {
      #   $lrms_nodes->{$host}{sysname} = $system{$1};
      #   $lrms_nodes->{$host}{machine} = $machine{$2};
      #}
   }

   #print STDERR Dumper($lrms_nodes);
   #print STDERR Dumper(%{$options->{queues}});
}

sub test {
    LogUtils::level("VERBOSE");
    require Data::Dumper; import Data::Dumper qw(Dumper);

    $path = shift;
    (%running_jobs,%waiting_jobs) = ();
    get_lrms_info("");
    print Dumper(\%node_stats,\%running_jobs,\%waiting_jobs);
}


#test('./test/6.0');
#test('./test/5.3');
#test($ARGV[0]);

1;
