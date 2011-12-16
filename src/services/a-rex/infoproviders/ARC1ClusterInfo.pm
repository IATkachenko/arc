package ARC1ClusterInfo;

# This information collector combines the output of the other information collectors
# and prepares the GLUE2 information model of A-REX.

use Storable;
use FileHandle;
use File::Temp;
use POSIX qw(ceil);

use strict;

use Sysinfo qw(cpuinfo processid diskinfo diskspaces);
use LogUtils;
use InfoChecker;

our $log = LogUtils->getLogger(__PACKAGE__);


# the time now in ISO 8061 format
sub timenow {
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime(time);
    return sprintf "%4d-%02d-%02dT%02d:%02d:%02d%1s", $year+1900, $mon+1, $mday,$hour,$min,$sec,"Z";
}

# converts MDS-style time to ISO 8061 time ( 20081212151903Z --> 2008-12-12T15:19:03Z )
sub mdstoiso {
    return "$1-$2-$3T$4:$5:$6Z" if shift =~ /^(\d\d\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d(?:\.\d+)?)Z$/;
    return undef;
}

sub glue2bool {
    my $bool = shift;
    return undef unless defined $bool;
    return $bool ? "true" : "false";
}

# TODO: Stage-in and Stage-out are substates of what?
sub bes_state {
    my ($gm_state,$lrms_state) = @_;
    if      ($gm_state eq "ACCEPTED") {
        return [ "Pending", "Accepted" ];
    } elsif ($gm_state eq "PREPARING") {
        return [ "Running", "Stage-in" ];
    } elsif ($gm_state eq "SUBMIT") {
        return [ "Running", "Submitting" ];
    } elsif ($gm_state eq "INLRMS") {
        if (not defined $lrms_state) {
            return [ "Running" ];
        } elsif ($lrms_state eq 'Q') {
            return [ "Running", "Queuing" ];
        } elsif ($lrms_state eq 'R') {
            return [ "Running", "Executing" ];
        } elsif ($lrms_state eq 'EXECUTED'
              or $lrms_state eq '') {
            return [ "Running", "Executed" ];
        } elsif ($lrms_state eq 'S') {
            return [ "Running", "Suspended" ];
        } else {
            return [ "Running", "LRMSOther" ];
        }
    } elsif ($gm_state eq "FINISHING") {
        return [ "Running", "Stage-out" ];
    } elsif ($gm_state eq "CANCELING") {
        return [ "Running", "Cancelling" ];
    } elsif ($gm_state eq "KILLED") {
        return [ "Cancelled" ];
    } elsif ($gm_state eq "FAILED") {
        return [ "Failed" ];
    } elsif ($gm_state eq "FINISHED") {
        return [ "Finished" ];
    } elsif ($gm_state eq "DELETED") {
        # Cannot map to BES state
        return [ ];
    } else {
        return [ ];
    }
}

sub glueState {
    my @ng_status = @_;
    return [ "UNDEFINEDVALUE" ] unless $ng_status[0];
    my $status = [ "nordugrid:".join(':',@ng_status) ];
    my $bes_state = bes_state(@ng_status);
    push @$status, "bes:".$bes_state->[0] if @$bes_state;
    return $status;
}

sub getGMStatus {
    my ($controldir, $ID) = @_;
    foreach my $gmjob_status ("$controldir/accepting/job.$ID.status", "$controldir/processing/job.$ID.status", "$controldir/finished/job.$ID.status") {
        unless (open (GMJOB_STATUS, "<$gmjob_status")) {
            next;
        } else {
            chomp (my ($first_line) = <GMJOB_STATUS>);
            close GMJOB_STATUS;
            unless ($first_line) {
                next;
            }
            return $first_line;
        }
    }
    return undef;
}

# Helper function that assists the GLUE2 XML renderer handle the 'splitjobs' option
#   $config       - the config hash
#   $jobid        - job id from GM
#   $gmjob        - a job hash ref as returned by GMJobsInfo
#   $xmlGenerator - a function ref that returns a string (the job's GLUE2 XML description)
# Returns undef on error, 0 if the XML file was already up to date, 1 if it was written
sub jobXmlFileWriter {
    my ($config, $jobid, $gmjob, $xmlGenerator) = @_;
    # If this is defined, then it's a job managed by local A-REX.
    my $gmuser = $gmjob->{gmuser};
    # Skip for now jobs managed by remote A-REX.
    # These are still published in ldap-infosys. As long as
    # distributing jobs to remote grid-managers is only
    # implemented by gridftd, remote jobs are not of interest
    # for the WS interface.
    return 0 unless defined $gmuser;
    my $controldir = $config->{control}{$gmuser}{controldir};
    my $xml_file = $controldir . "/job." . $jobid . ".xml";

    # Here goes simple optimisation - do not write new
    # XML if status has not changed while in "slow" states
    my $xml_time = (stat($xml_file))[9];
    my $status_time = $gmjob->{statusmodified};
    return 0 if defined $xml_time
                and defined $status_time
                and $status_time < $xml_time
                and $gmjob->{status} =~ /ACCEPTED|FINISHED|FAILED|KILLED|DELETED/;

    my $xmlstring = &$xmlGenerator();
    return undef unless defined $xmlstring;

    # tempfile croaks on error
    my ($fh, $tmpnam) = File::Temp::tempfile("job.$jobid.xml.XXXXXXX", DIR => $controldir);
    binmode $fh, ':encoding(utf8)';
    print $fh $xmlstring and close $fh
        or $log->warning("Error writing to temporary file $tmpnam: $!")
        and close $fh
        and unlink $tmpnam
        and return undef;
    rename $tmpnam, $xml_file
        or $log->warning("Error moving $tmpnam to $xml_file: $!")
        and unlink $tmpnam
        and return undef;


    # Avoid .xml files created after job is deleted
    # Check if status file exists
    if(not defined getGMStatus($controldir,$jobid)) {
      unlink $xml_file;
      return undef;
    }

    # Set timestamp to the time when the status file was read in.
    # This is because the status file might have been updated by the time the
    # XML file gets written. This step ensures that the XML will be updated on
    # the next run of the infoprovider.
    my $status_read = $gmjob->{statusread};
    return undef unless defined $status_read;
    utime(time(), $status_read, $xml_file)
        or $log->warning("Couldn't touch $xml_file: $!")
        and return undef;

    # *.xml file was updated
    return 1;
};

# Intersection of two arrays that completes in linear time.  The input arrays
# are the keys of the two hashes passed as reference.  The intersection array
# consists of the keys of the returned hash reference.
sub intersection {
    my ($a, $b) = @_;
    my (%union, %xor, %isec);
    for (keys %$a) { $union{$_} = 1; $xor{$_} = 1 if exists $b->{$_} }
    for (keys %$b) { $union{$_} = 1; $xor{$_} = 1 if exists $a->{$_} }
    for (keys %union) { $isec{$_} = 1 if exists $xor{$_} }
    return \%isec;
}


# processes NodeSelection options and returns the matching nodes.
sub selectnodes {
    my ($nodes, %nscfg) = @_;
    return undef unless %$nodes and %nscfg;

    my @allnodes = keys %$nodes;

    my %selected = ();
    if ($nscfg{Regex}) {
        for my $re (@{$nscfg{Regex}}) {
            map { $selected{$_} = 1 if /$re/ } @allnodes;
        }
    }
    if ($nscfg{Tag}) {
        for my $tag (@{$nscfg{Tag}}) {
            for my $node (@allnodes) {
                my $tags = $nodes->{$node}{tags};
                next unless $tags;
                map { $selected{$node} = 1 if $tag eq $_ } @$tags; 
            }
        }
    }
    if ($nscfg{Command}) {
        $log->warning("Not implemented: NodeSelection: Command");
    }

    delete $nscfg{Regex};
    delete $nscfg{Tag};
    delete $nscfg{Command};
    $log->warning("Unknown NodeSelection option: @{[keys %nscfg]}") if %nscfg;

    $selected{$_} = $nodes->{$_} for keys %selected;

    return \%selected;
}

# Sums up ExecutionEnvironments attributes from the LRMS plugin
sub xestats {
    my ($xenv, $nodes) = @_;
    return undef unless %$nodes;

    my %continuous = (vmem => 'VirtualMemorySize', pmem => 'MainMemorySize');
    my %discrete = (lcpus => 'LogicalCPUs', pcpus => 'PhysicalCPUs', sysname => 'OSFamily', machine => 'Platform');

    my (%minval, %maxval);
    my (%minnod, %maxnod);
    my %distrib;

    my %stats = (total => 0, free => 0, available => 0);

    for my $host (keys %$nodes) {
        my %node = %{$nodes->{$host}};
        $stats{total}++;
        $stats{free}++ if $node{isfree};
        $stats{available}++ if $node{isavailable};

        # Also agregate values across nodes, check consistency
        for my $prop (%discrete) {
            my $val = $node{$prop};
            next unless defined $val;
            push @{$distrib{$prop}{$val}}, $host;
        }
        for my $prop (keys %continuous) {
            my $val = $node{$prop};
            next unless defined $val;
            if (not defined $minval{$prop} or (defined $minval{$prop} and $minval{$prop} > $val)) {
                $minval{$prop} = $val;
                $minnod{$prop} = $host;
            }
            if (not defined $maxval{$prop} or (defined $maxval{$prop} and $maxval{$prop} < $val)) {
                $maxval{$prop} = $val;
                $maxnod{$prop} = $host;
            }
        }
    }

   my $homogeneous = 1;
    while (my ($prop, $opt) = each %discrete) {
        my $values = $distrib{$prop};
        next unless $values;
        if (scalar keys %$values > 1) {
            my $msg = "ExecutionEnvironment $xenv is inhomogeneous regarding $opt:";
            while (my ($val, $hosts) = each %$values) {
                my $first = pop @$hosts;
                my $remaining = @$hosts;
                $val = defined $val ? $val : 'undef';
                $msg .= " $val($first";
                $msg .= "+$remaining more" if $remaining;
                $msg .= ")";
            }
            $log->info($msg);
            $homogeneous = 0;
        } else {
            my ($val) = keys %$values;
            $stats{$prop} = $val;
        }
    }
    if ($maxval{pmem}) {
        my $rdev = 2 * ($maxval{pmem} - $minval{pmem}) / ($maxval{pmem} + $minval{pmem});
        if ($rdev > 0.1) {
            my $msg = "ExecutionEnvironment $xenv has variability larger than 10% regarding MainMemorySize:";
            $msg .= " Min=$minval{pmem}($minnod{pmem}),";
            $msg .= " Max=$maxval{pmem}($maxnod{pmem})";
            $log->info($msg);
            $homogeneous = 0;
        }
        $stats{pmem} = $minval{pmem};
    }
    if ($maxval{vmem}) {
        my $rdev = 2 * ($maxval{vmem} - $minval{vmem}) / ($maxval{vmem} + $minval{vmem});
        if ($rdev > 0.5) {
            my $msg = "ExecutionEnvironment $xenv has variability larger than 50% regarding VirtualMemorySize:";
            $msg .= " Min=$minval{vmem}($minnod{vmem}),";
            $msg .= " Max=$maxval{vmem}($maxnod{vmem})";
            $log->debug($msg);
        }
        $stats{vmem} = $minval{vmem};
    }
    $stats{homogeneous} = $homogeneous;

    return \%stats;
}

# Combine info about ExecutionEnvironments from config options and the LRMS plugin
sub xeinfos {
    my ($config, $nodes) = @_;
    my $infos = {};
    my %nodemap = ();
    my @xenvs = keys %{$config->{xenvs}};
    for my $xenv (@xenvs) {
        my $xecfg = $config->{xenvs}{$xenv};
        my $info = $infos->{$xenv} = {};
        my $nscfg = $xecfg->{NodeSelection};
        if (ref $nodes eq 'HASH') {
            my $selected;
            if (not $nscfg) {
                $log->info("NodeSelection configuration missing for ExecutionEnvironment $xenv, implicitly assigning all nodes into it")
                    unless keys %$nodes == 1 and @xenvs == 1;
                $selected = $nodes;
            } else {
                $selected = selectnodes($nodes, %$nscfg);
            }
            $nodemap{$xenv} = $selected;
            $log->debug("Nodes in ExecutionEnvironment $xenv: ".join ' ', keys %$selected);
            $log->info("No nodes matching NodeSelection for ExecutionEnvironment $xenv") unless %$selected;
            my $stats = xestats($xenv, $selected);
            if ($stats) {
                $info->{ntotal} = $stats->{total};
                $info->{nbusy} = $stats->{available} - $stats->{free};
                $info->{nunavailable} = $stats->{total} - $stats->{available};
                $info->{pmem} = $stats->{pmem} if $stats->{pmem};
                $info->{vmem} = $stats->{vmem} if $stats->{vmem};
                $info->{pcpus} = $stats->{pcpus} if $stats->{pcpus};
                $info->{lcpus} = $stats->{lcpus} if $stats->{lcpus};
                $info->{slots} = $stats->{slots} if $stats->{slots};
                $info->{sysname} = $stats->{sysname} if $stats->{sysname};
                $info->{machine} = $stats->{machine} if $stats->{machine};
            }
        } else {
            $log->info("The LRMS plugin has no support for NodeSelection options, ignoring them") if $nscfg;
        }
        $info->{pmem} = $xecfg->{MainMemorySize} if $xecfg->{MainMemorySize};
        $info->{vmem} = $xecfg->{VirtualMemorySize} if $xecfg->{VirtualMemorySize};
        $info->{pcpus} = $xecfg->{PhysicalCPUs} if $xecfg->{PhysicalCPUs};
        $info->{lcpus} = $xecfg->{LogicalCPUs} if $xecfg->{LogicalCPUs};
        $info->{sysname} = $xecfg->{OSFamily} if $xecfg->{OSFamily};
        $info->{machine} = $xecfg->{Platform} if $xecfg->{Platform};
    }
    # Check for overlap of nodes
    if (ref $nodes eq 'HASH') {
        for (my $i=0; $i<@xenvs; $i++) {
            my $nodes1 = $nodemap{$xenvs[$i]};
            next unless $nodes1;
            for (my $j=0; $j<$i; $j++) {
                my $nodes2 = $nodemap{$xenvs[$j]};
                next unless $nodes2;
                my $overlap = intersection($nodes1, $nodes2);
                $log->warning("Overlap detected between ExecutionEnvironments $xenvs[$i] and $xenvs[$j]. "
                             ."Use NodeSelection options to select correct nodes") if %$overlap;
            }
        }
    }
    return $infos;
}

# For each duration, find the largest available numer of slots of any user
# Input: the users hash returned by thr LRMS module.
sub max_userfreeslots {
    my ($users) = @_;
    my %timeslots;

    for my $uid (keys %$users) {
        my $uinfo = $users->{$uid};
        next unless defined $uinfo->{freecpus};

        for my $nfree ( keys %{$uinfo->{freecpus}} ) {
            my $seconds = 60 * $uinfo->{freecpus}{$nfree};

            if ($timeslots{$seconds}) {
                $timeslots{$seconds} = $nfree > $timeslots{$seconds}
                                     ? $nfree : $timeslots{$seconds};
            } else {
                $timeslots{$seconds} = $nfree;
            }
        }
    }
    return %timeslots;
}


############################################################################
# Combine info from all sources to prepare the final representation
############################################################################

sub collect($) {
    my ($data) = @_;

    my $config = $data->{config};
    my $usermap = $data->{usermap};
    my $host_info = $data->{host_info};
    my $rte_info = $data->{rte_info};
    my $gmjobs_info = $data->{gmjobs_info};
    my $lrms_info = $data->{lrms_info};
    my $nojobs = $data->{nojobs};

    my $creation_time = timenow();
    my $validity_ttl = $config->{ttl};
    my $hostname = $config->{hostname} || $host_info->{hostname};

    my @allxenvs = keys %{$config->{xenvs}};
    my @allshares = keys %{$config->{shares}};

    my $homogeneous = 1;
    $homogeneous = 0 if @allxenvs > 1;
    $homogeneous = 0 if @allshares > 1 and @allxenvs == 0;
    for my $xeconfig (values %{$config->{xenvs}}) {
        $homogeneous = 0 if defined $xeconfig->{Homogeneous}
                            and not $xeconfig->{Homogeneous};
    }

    my $xeinfos = xeinfos($config, $lrms_info->{nodes});

    # Figure out total number of CPUs
    my ($totalpcpus, $totallcpus) = (0,0);

    # First, try to sum up cpus from all ExecutionEnvironments
    for my $xeinfo (values %$xeinfos) {
        unless (exists $xeinfo->{ntotal} and $xeinfo->{pcpus}) { $totalpcpus = 0; last }
        $totalpcpus += $xeinfo->{ntotal}  *  $xeinfo->{pcpus};
    }
    for my $xeinfo (values %$xeinfos) {
        unless (exists $xeinfo->{ntotal} and $xeinfo->{lcpus}) { $totallcpus = 0; last }
        $totallcpus += $xeinfo->{ntotal}  *  $xeinfo->{lcpus};
    }
    #$log->debug("Cannot determine total number of physical CPUs in all ExecutionEnvironments") unless $totalpcpus;
    $log->debug("Cannot determine total number of logical CPUs in all ExecutionEnvironments") unless $totallcpus;

    # Next, use value returned by LRMS in case the the first try failed.
    # OBS: most LRMSes don't differentiate between Physical and Logical CPUs.
    $totalpcpus ||= $lrms_info->{cluster}{totalcpus};
    $totallcpus ||= $lrms_info->{cluster}{totalcpus};



    # # # # # # # # # # # # # # # # # # #
    # # # # # Job statistics  # # # # # #
    # # # # # # # # # # # # # # # # # # #

    # total jobs in each GM state
    my %gmtotalcount;

    # jobs in each GM state, by share
    my %gmsharecount;

    # grid jobs in each lrms sub-state (queued, running, suspended), by share
    my %inlrmsjobs;

    # grid jobs in each lrms sub-state (queued, running, suspended)
    my %inlrmsjobstotal;

    # slots needed by grid jobs in each lrms sub-state (queued, running, suspended), by share
    my %inlrmsslots;

    # number of slots needed by all waiting jobs, per share
    my %requestedslots;

    # Jobs waiting to be prepared by GM (indexed by share)
    my %pending;

    # Jobs waiting to be prepared by GM
    my $pendingtotal;

    # Jobs being prepared by GM (indexed by share)
    my %share_prepping;

    # Jobs being prepared by GM (indexed by grid owner)
    my %user_prepping;
    # $user_prepping{$_} = 0 for keys %$usermap;

    for my $jobid (keys %$gmjobs_info) {

        my $job = $gmjobs_info->{$jobid};
        my $gridowner = $gmjobs_info->{$jobid}{subject};
        my $share = $job->{share};

        my $gmstatus = $job->{status} || '';

        $gmtotalcount{totaljobs}++;
        $gmsharecount{$share}{totaljobs}++;

        # count GM states by category

        my %states = ( 'UNDEFINED'        => [0, 'undefined'],
                       'ACCEPTING'        => [1, 'accepted'],
                       'ACCEPTED'         => [1, 'accepted'],
                       'PENDING:ACCEPTED' => [1, 'accepted'],
                       'PREPARING'        => [2, 'preparing'],
                       'PENDING:PREPARING'=> [2, 'preparing'],
                       'SUBMIT'           => [2, 'preparing'],
                       'SUBMITTING'       => [2, 'preparing'],
                       'INLRMS'           => [3, 'inlrms'],
                       'PENDING:INLRMS'   => [4, 'finishing'],
                       'FINISHING'        => [4, 'finishing'],
                       'CANCELING'        => [4, 'finishing'],
                       'FAILED'           => [5, 'finished'],
                       'KILLED'           => [5, 'finished'],
                       'FINISHED'         => [5, 'finished'],
                       'DELETED'          => [6, 'deleted']   );

        unless ($states{$gmstatus}) {
            $log->warning("Unexpected job status for job $jobid: $gmstatus");
            $gmstatus = $job->{status} = 'UNDEFINED';
        }
        my ($age, $category) = @{$states{$gmstatus}};

        $gmtotalcount{$category}++;
        $gmsharecount{$share}{$category}++;

        if ($age < 6) {
            $gmtotalcount{notdeleted}++;
            $gmsharecount{$share}{notdeleted}++;
        }
        if ($age < 5) {
            $gmtotalcount{notfinished}++;
            $gmsharecount{$share}{notfinished}++;
        }
        if ($age < 3) {
            $gmtotalcount{notsubmitted}++;
            $gmsharecount{$share}{notsubmitted}++;
            $requestedslots{$share} += $job->{count} || 1;
            $share_prepping{$share}++;
            $user_prepping{$gridowner}++;
        }
        if ($age < 2) {
            $pending{$share}++;
            $pendingtotal++;
        }

        # count grid jobs running and queued in LRMS for each share

        if ($gmstatus eq 'INLRMS') {
            my $lrmsid = $job->{localid};
            my $lrmsjob = $lrms_info->{jobs}{$lrmsid};
            my $slots = $job->{count} || 1;

            if (defined $lrmsjob) {
                if ($lrmsjob->{status} ne 'EXECUTED') {
                    $inlrmsslots{$share}{running} ||= 0;
                    $inlrmsslots{$share}{suspended} ||= 0;
                    $inlrmsslots{$share}{queued} ||= 0;
                    if ($lrmsjob->{status} eq 'R') {
                        $inlrmsjobstotal{running}++;
                        $inlrmsjobs{$share}{running}++;
                        $inlrmsslots{$share}{running} += $slots;
                    } elsif ($lrmsjob->{status} eq 'S') {
                        $inlrmsjobstotal{suspended}++;
                        $inlrmsjobs{$share}{suspended}++;
                        $inlrmsslots{$share}{suspended} += $slots;
                    } else {  # Consider other states 'queued'
                        $inlrmsjobstotal{queued}++;
                        $inlrmsjobs{$share}{queued}++;
                        $inlrmsslots{$share}{queued} += $slots;
                        $requestedslots{$share} += $slots;
                    }
                }
            } else {
                $log->warning("Info missing about lrms job $lrmsid");
            }
        }

    }

    my $admindomain = $config->{AdminDomain};
    my $servicename = $config->{service}{ClusterName};

    # TODO: Calculate endpoint URLs and check if they're enabled
    
    my $endpointsnum = 0;

    my $gridftphostport = '';
    # check if gridftd interface exists
    if ($config->{GridftpdEnabled} == 1) { 
	$gridftphostport = "$hostname:$config->{GridftpdPort}";
	$endpointsnum++;
    };
    
    # check if WS interface is actually running
    # done with netstat but I'd like to be smarter
    my $arexhostport = $config->{arexhostport};
    my $netstat=`netstat -antup`;
    if ($? == -1) {
      $log->warning("Checking if ARC WS interface is running: error in executing netstat. Infosys will assume it running on standard port");
    } else {
	# searches if arched is listed in netstat output
	# this commented line below is for testing. Better way would be mocking netstat
	# best way would be ask arched for its endpoint...
	# if (1){ 
	if ( $netstat =~ m/arched/){ 
	  # $log->info("arched found with netstat");
	  $endpointsnum++; 
	} else {
	    $log->warning("arched A-REX endpoint not found with netstat. No WS endpoint information will be published in the infosys. Check if arched process is running");
	    $arexhostport = '';
	};
    };
    
    # The following is for EMI-ES
    my $emieshostport = '';
    if ($arexhostport ne '' && $config->{enable_emies_interface}) {
        $emieshostport = $arexhostport;
        $endpointsnum++;
    }

    # The following is for the Stagein interface
    my $stageinhostport = '';

    # Global IDs
    my $adID = "urn:ogf:AdminDomain:$admindomain"; # AdminDomain ID
    my $udID = "urn:ogf:UserDomain:local:$admindomain" ; # UserDomain ID;
    my $csvID = "urn:ogf:ComputingService:$servicename"; # ComputingService ID
    my $cmgrID = "urn:ogf:ComputingManager:$servicename"; # ComputingManager ID
    my $ARCgftpjobcepID = "urn:ogf:ComputingEndpoint:gsiftp:$gridftphostport"; # ARCGridFTPComputingEndpoint ID
    my $ARCWScepID = "urn:ogf:ComputingEndpoint:$arexhostport"; # ARCWSComputingEndpoint ID
    my $EMIEScepID = "urn:ogf:ComputingEndpoint:emies:$emieshostport"; # EMIESComputingEndpoint ID
    my $StageincepID = "urn:ogf:ComputingEndpoint:$stageinhostport"; # StageinComputingEndpoint ID
    my $cactIDp = "urn:ogf:ComputingActivity:$arexhostport"; # ComputingActivity ID prefix
    my $cshaIDp = "urn:ogf:ComputingShare:$servicename"; # ComputingShare ID prefix
    my $xenvIDp = "urn:ogf:ExecutionEnvironment:$servicename"; # ExecutionEnvironment ID prefix
    my $aenvIDp = "urn:ogf:ApplicationEnvironment:$servicename"; # ApplicationEnvironment ID prefix
    my $apolID = "urn:ogf:AccessPolicy:$servicename"; # AccessPolicy ID prefix
    my $mpolIDp = "urn:ogf:MappingPolicy:$servicename"; # MappingPolicy ID prefix
    my %cactIDs; # ComputingActivity IDs
    my %cshaIDs; # ComputingShare IDs
    my %aenvIDs; # ApplicationEnvironment IDs
    my %xenvIDs; # ExecutionEnvironment IDs
    my $tseID = "urn:ogf:ToStorageElement:StorageService"; # ToStorageElement ID prefix
    
    # Other Service IDs
    my $ARISsvID = "urn:ogf:Service:ARIS"; # ARIS service ID
    my $ARISepID = "urn:ogf:Endpoint:ARIS"; # ARIS Endpoint ID
    my $CacheIndexsvID = "urn:ogf:Service:Cache-Index"; # Cache-Index service ID
    my $CacheIndexepID = "urn:ogf:Endpoint:Cache-Index"; # Cache-Index Endpoint ID
    my $HEDControlsvID = "urn:ogf:Service:HED-CONTROL"; # HED-CONTROL service ID
    my $HEDControlepID = "urn:ogf:Endpoint:HED-CONTROL"; # HED-CONTROL Endpoint ID

    # Generate ComputingShare IDs
    for my $share (keys %{$config->{shares}}) {
        $cshaIDs{$share} = "$cshaIDp:$share";
    }

    # Generate ApplicationEnvironment IDs
    for my $rte (keys %$rte_info) {
        $aenvIDs{$rte} = "$aenvIDp:$rte";
    }

    # Generate ExecutionEnvironment IDs
    $xenvIDs{$_} = "$xenvIDp:$_" for @allxenvs;

    # generate ComputingActivity IDs
    unless ($nojobs) {
        for my $jobid (keys %$gmjobs_info) {
            my $share = $gmjobs_info->{$jobid}{share};
            $cactIDs{$share}{$jobid} = "$cactIDp:$jobid";
        }
    }

    unless (@{$config->{accesspolicies}}) {
        $log->warning("No AccessPolicy configured");
    }


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    # # # # # # # # # # build information tree  # # # # # # # # # #
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

    my $callcount = 0;


    # function that generates ComputingService data

    my $getComputingService = sub {

        $callcount++;

        my $csv = {};

        $csv->{CreationTime} = $creation_time;
        $csv->{Validity} = $validity_ttl;

        $csv->{ID} = $csvID;

        $csv->{Name} = $config->{service}{ClusterName} if $config->{service}{ClusterName}; # scalar
        $csv->{OtherInfo} = $config->{service}{OtherInfo} if $config->{service}{OtherInfo}; # array
        $csv->{Capability} = [ 'executionmanagement.jobexecution' ];
        $csv->{Type} = 'org.nordugrid.execution.arex';

        # OBS: QualityLevel reflects the quality of the sotware
        # One of: development, testing, pre-production, production
        $csv->{QualityLevel} = 'pre-production';

        $csv->{StatusInfo} =  $config->{service}{StatusInfo} if $config->{service}{StatusInfo}; # array

        my $nshares = keys %{$config->{shares}};

        $csv->{Complexity} = "endpoint=$endpointsnum,share=$nshares,resource=".(scalar @allxenvs);

        $csv->{AllJobs} = $gmtotalcount{totaljobs} || 0;
        # OBS: Finished/failed/deleted jobs are not counted
        $csv->{TotalJobs} = $gmtotalcount{notfinished} || 0;

        $csv->{RunningJobs} = $inlrmsjobstotal{running} || 0;
        $csv->{SuspendedJobs} = $inlrmsjobstotal{suspended} || 0;
        $csv->{WaitingJobs} = $inlrmsjobstotal{queued} || 0;
        $csv->{StagingJobs} = ( $gmtotalcount{preparing} || 0 )
                            + ( $gmtotalcount{finishing} || 0 );
        $csv->{PreLRMSWaitingJobs} = $pendingtotal || 0;


        # Computing Endpoints ########
	  
	# Here comes a list of endpoints we support.
        # GridFTPd job execution endpoint
	# XBES A-REX WSRF job submission endpoint
	# EMI-ES job submission endpoint
	# EMI-ES information system endpoint
	# EMI-ES delegation endpoint
	# WS-LIDI A-REX WSRF information system endpoint
	# A-REX datastaging endpoint

	# this will contain only endpoints with URLs defined
	my @ceps = ();

	# Our different ComputingEndpoints
	
	# ARC XBES and WS-LIDI
      
	my $getARCWSComputingEndpoint = sub {

	    # don't publish if arched not listening
	    return undef unless $arexhostport ne '';

	    my $cep = {};

	    $cep->{CreationTime} = $creation_time;
	    $cep->{Validity} = $validity_ttl;

	    $cep->{ID} = $ARCWScepID;

	    # Name not necessary -- why? added back
	    $cep->{Name} = "ARC WSRF XBES submission interface and WSRF LIDI Information System";

	    # OBS: ideally HED should be asked for the URL
	    $cep->{URL} = $config->{endpoint};
	    $cep->{Capability} = [ 'executionmanagement.jobexecution', 'information.monitoring' ];
	    $cep->{Technology} = 'webservice';
	    $cep->{InterfaceName} = 'XBES';
	    $cep->{InterfaceVersion} = [ '1.0' ];
	    # InterfaceExtension should return the same as BESExtension attribute of BES-Factory.
	    # value is taken from services/a-rex/get_factory_attributes_document.cpp, line 56.
	    $cep->{InterfaceExtension} = [ 'http://www.nordugrid.org/schemas/a-rex' ];
	    $cep->{WSDL} = [ $config->{endpoint}."/?wsdl" ];
	    # Wrong type, should be URI
	    $cep->{SupportedProfile} = [ "http://www.ws-i.org/Profiles/BasicProfile-1.0.html",  # WS-I 1.0
					"http://schemas.ogf.org/hpcp/2007/01/bp"               # HPC-BP
				      ];
	    $cep->{Semantics} = [ "http://www.nordugrid.org/documents/arex.pdf" ];
	    $cep->{Implementor} = "NorduGrid";
	    $cep->{ImplementationName} = "ARC CE";
	    $cep->{ImplementationVersion} = $config->{arcversion};

	    $cep->{QualityLevel} = "development";

	    my %healthissues;

	    if ($config->{x509_user_cert} and $config->{x509_cert_dir}) {
		if (     $host_info->{hostcert_expired}
		      or $host_info->{issuerca_expired}) {
		    push @{$healthissues{critical}}, "Host credentials expired";
		} elsif (not $host_info->{hostcert_enddate}
		      or not $host_info->{issuerca_enddate}) {
		    push @{$healthissues{critical}}, "Host credentials missing";
		} elsif ($host_info->{hostcert_enddate} - time < 48*3600
		      or $host_info->{issuerca_enddate} - time < 48*3600) {
		    push @{$healthissues{warning}}, "Host credentials will expire soon";
		}
	    }

	    if ( $host_info->{gm_alive} ne 'all' ) {
		if ($host_info->{gm_alive} eq 'some') {
		    push @{$healthissues{warning}}, 'One or more grid managers are down';
		} else {
		    push @{$healthissues{critical}},
			  $config->{remotegmdirs} ? 'All grid managers are down'
						  : 'Grid manager is down';
		}
	    }

	    if (%healthissues) {
		my @infos;
		for my $level (qw(critical warning other)) {
		    next unless $healthissues{$level};
		    $cep->{HealthState} ||= $level;
		    push @infos, @{$healthissues{$level}};
		}
		$cep->{HealthStateInfo} = join "; ", @infos;
	    } else {
		$cep->{HealthState} = 'ok';
	    }

	    # OBS: Do 'queueing' and 'closed' states apply for a-rex?
	    # OBS: Is there an allownew option for a-rex?
	    #if ( $config->{GridftpdAllowNew} == 0 ) {
	    #    $cep->{ServingState} = 'draining';
	    #} else {
	    #    $cep->{ServingState} = 'production';
	    #}
	    $cep->{ServingState} = 'production';

	    # StartTime: get it from hed

	    $cep->{IssuerCA} = $host_info->{issuerca}; # scalar
	    $cep->{TrustedCA} = $host_info->{trustedcas}; # array

	    # TODO: Downtime, is this necessary, and how should it work?

	    $cep->{Staging} =  'staginginout';
	    $cep->{JobDescription} = [ 'ogf:jsdl:1.0', "nordugrid:xrsl" ];

	    $cep->{TotalJobs} = $gmtotalcount{notfinished} || 0;

	    $cep->{RunningJobs} = $inlrmsjobstotal{running} || 0;
	    $cep->{SuspendedJobs} = $inlrmsjobstotal{suspended} || 0;
	    $cep->{WaitingJobs} = $inlrmsjobstotal{queued} || 0;

	    $cep->{StagingJobs} = ( $gmtotalcount{preparing} || 0 )
				+ ( $gmtotalcount{finishing} || 0 );

	    $cep->{PreLRMSWaitingJobs} = $pendingtotal || 0;

	    if ($config->{accesspolicies}) {
		my @apconfs = @{$config->{accesspolicies}};
		$cep->{AccessPolicies} = sub {
		    return undef unless @apconfs;
		    my $apconf = pop @apconfs;
		    my $apol = {};
		    $apol->{ID} = "$apolID:".join(",", @{$apconf->{Rule}});
		    $apol->{Scheme} = "basic";
		    $apol->{Rule} = $apconf->{Rule};
		    $apol->{UserDomainID} = $apconf->{UserDomainID};
		    $apol->{EndpointID} = $ARCWScepID;
		    return $apol;
		};
	    }
	    
	    $cep->{OtherInfo} = $host_info->{EMIversion} if ($host_info->{EMIversion}); # array


	    # Associations

	    $cep->{ComputingShareID} = [ values %cshaIDs ];
	    $cep->{ComputingServiceID} = $csvID;

	    return $cep;
	};

	# don't publish if arched not listening
	if ($arexhostport ne '') { push(@ceps, $getARCWSComputingEndpoint); };

        # ARC GridFTPDd job submission interface 
      
	my $getARCGFTPdComputingEndpoint = sub {

	    # check if gridftpd interface is actually configured
	    return undef unless ( $gridftphostport ne '');
	    my $cep = {};

	    $cep->{CreationTime} = $creation_time;
	    $cep->{Validity} = $validity_ttl;

	    $cep->{ID} = $ARCgftpjobcepID;

	    # Name not necessary -- why? added back
	    $cep->{Name} = "ARC GridFTP job execution interface";

	    # OBS: ideally HED should be asked for the URL
	    $cep->{URL} = "gsiftp://$gridftphostport";
	    $cep->{Capability} = [ 'executionmanagement.jobexecution' ];
	    $cep->{Technology} = 'GridFTP';
	    $cep->{InterfaceName} = 'GridFTP-job';
	    $cep->{InterfaceVersion} = [ '1.0' ];
	    # InterfaceExtension should return the same as BESExtension attribute of BES-Factory.
	    # value is taken from services/a-rex/get_factory_attributes_document.cpp, line 56.
	    $cep->{InterfaceExtension} = [ 'http://www.nordugrid.org/schemas/gridftpd' ];
	    # Wrong type, should be URI
	    $cep->{Semantics} = [ "http://www.nordugrid.org/documents/gridfptd.pdf" ];
	    $cep->{Implementor} = "NorduGrid";
	    $cep->{ImplementationName} = "ARC CE Gridftpd";
	    $cep->{ImplementationVersion} = $config->{arcversion};

	    $cep->{QualityLevel} = "production";

	    my %healthissues;

	    if ($config->{x509_user_cert} and $config->{x509_cert_dir}) {
		if (     $host_info->{hostcert_expired}
		      or $host_info->{issuerca_expired}) {
		    push @{$healthissues{critical}}, "Host credentials expired";
		} elsif (not $host_info->{hostcert_enddate}
		      or not $host_info->{issuerca_enddate}) {
		    push @{$healthissues{critical}}, "Host credentials missing";
		} elsif ($host_info->{hostcert_enddate} - time < 48*3600
		      or $host_info->{issuerca_enddate} - time < 48*3600) {
		    push @{$healthissues{warning}}, "Host credentials will expire soon";
		}
	    }

	    if ( $host_info->{gm_alive} ne 'all' ) {
		if ($host_info->{gm_alive} eq 'some') {
		    push @{$healthissues{warning}}, 'One or more grid managers are down';
		} else {
		    push @{$healthissues{critical}},
			  $config->{remotegmdirs} ? 'All grid managers are down'
						  : 'Grid manager is down';
		}
	    }

	    if (%healthissues) {
		my @infos;
		for my $level (qw(critical warning other)) {
		    next unless $healthissues{$level};
		    $cep->{HealthState} ||= $level;
		    push @infos, @{$healthissues{$level}};
		}
		$cep->{HealthStateInfo} = join "; ", @infos;
	    } else {
		$cep->{HealthState} = 'ok';
	    }

	    if ( $config->{GridftpdAllowNew} == 0 ) {
	        $cep->{ServingState} = 'draining';
	    } else {
	        $cep->{ServingState} = 'production';
	    }

	    # StartTime: get it from hed

	    $cep->{IssuerCA} = $host_info->{issuerca}; # scalar
	    $cep->{TrustedCA} = $host_info->{trustedcas}; # array

	    # TODO: Downtime, is this necessary, and how should it work?

	    $cep->{Staging} =  'staginginout';
	    $cep->{JobDescription} = [ 'ogf:jsdl:1.0', "nordugrid:xrsl" ];

	    $cep->{TotalJobs} = $gmtotalcount{notfinished} || 0;

	    $cep->{RunningJobs} = $inlrmsjobstotal{running} || 0;
	    $cep->{SuspendedJobs} = $inlrmsjobstotal{suspended} || 0;
	    $cep->{WaitingJobs} = $inlrmsjobstotal{queued} || 0;

	    $cep->{StagingJobs} = ( $gmtotalcount{preparing} || 0 )
				+ ( $gmtotalcount{finishing} || 0 );

	    $cep->{PreLRMSWaitingJobs} = $pendingtotal || 0;

	    if ($config->{accesspolicies}) {
		my @apconfs = @{$config->{accesspolicies}};
		$cep->{AccessPolicies} = sub {
		    return undef unless @apconfs;
		    my $apconf = pop @apconfs;
		    my $apol = {};
		    $apol->{ID} = "$apolID:".join(",", @{$apconf->{Rule}});
		    $apol->{Scheme} = "basic";
		    $apol->{Rule} = $apconf->{Rule};
		    $apol->{UserDomainID} = $apconf->{UserDomainID};
		    $apol->{EndpointID} = $ARCgftpjobcepID;
		    return $apol;
		};
	    }
	    
	    $cep->{OtherInfo} = $host_info->{EMIversion} if ($host_info->{EMIversion}); # array


	    # Associations

	    $cep->{ComputingShareID} = [ values %cshaIDs ];
	    $cep->{ComputingServiceID} = $csvID;

	    return $cep;
	};

	# Don't publish if there is no endpoint URL
        if ($gridftphostport ne '') { push(@ceps, $getARCGFTPdComputingEndpoint); };

    # Placeholder for EMI-ES interface

    my $getEMIESComputingEndpoint = sub {

        # don't publish if no endpoint URL
        return undef unless $emieshostport ne '';

        my $cep = {};

        $cep->{CreationTime} = $creation_time;
        $cep->{Validity} = $validity_ttl;

        $cep->{ID} = $EMIEScepID;

        # Name not necessary -- why? added back
        $cep->{Name} = "EMI ES submission, delegation, information interface";

        # OBS: ideally HED should be asked for the URL
        $cep->{URL} = $config->{endpoint};
        $cep->{Capability} = [ 'executionmanagement.jobexecution', 'information.monitoring', 'security.delegation' ];
        $cep->{Technology} = 'webservice';
        $cep->{InterfaceName} = 'EMI-ES';
        $cep->{InterfaceVersion} = [ '1.0' ];
        # InterfaceExtension should return the same as BESExtension attribute of BES-Factory.
        # value is taken from services/a-rex/get_factory_attributes_document.cpp, line 56.
        #$cep->{InterfaceExtension} = [ 'http://www.nordugrid.org/schemas/a-rex' ];
        $cep->{WSDL} = [ $config->{endpoint}."/?wsdl" ];
        # Wrong type, should be URI
        $cep->{SupportedProfile} = [ "http://www.ws-i.org/Profiles/BasicProfile-1.0.html",  # WS-I 1.0
                    "http://schemas.ogf.org/hpcp/2007/01/bp"               # HPC-BP
                      ];
        #$cep->{Semantics} = [ "http://www.nordugrid.org/documents/arex.pdf" ];
        $cep->{Implementor} = "EMI";
        $cep->{ImplementationName} = "EMI-ES";
        $cep->{ImplementationVersion} = 'emiversion';

        $cep->{QualityLevel} = "development";

        my %healthissues;

        if ($config->{x509_user_cert} and $config->{x509_cert_dir}) {
        if (     $host_info->{hostcert_expired}
              or $host_info->{issuerca_expired}) {
            push @{$healthissues{critical}}, "Host credentials expired";
        } elsif (not $host_info->{hostcert_enddate}
              or not $host_info->{issuerca_enddate}) {
            push @{$healthissues{critical}}, "Host credentials missing";
        } elsif ($host_info->{hostcert_enddate} - time < 48*3600
              or $host_info->{issuerca_enddate} - time < 48*3600) {
            push @{$healthissues{warning}}, "Host credentials will expire soon";
        }
        }

        if ( $host_info->{gm_alive} ne 'all' ) {
        if ($host_info->{gm_alive} eq 'some') {
            push @{$healthissues{warning}}, 'One or more grid managers are down';
        } else {
            push @{$healthissues{critical}},
              $config->{remotegmdirs} ? 'All grid managers are down'
                          : 'Grid manager is down';
        }
        }

        if (%healthissues) {
        my @infos;
        for my $level (qw(critical warning other)) {
            next unless $healthissues{$level};
            $cep->{HealthState} ||= $level;
            push @infos, @{$healthissues{$level}};
        }
        $cep->{HealthStateInfo} = join "; ", @infos;
        } else {
        $cep->{HealthState} = 'ok';
        }

        # OBS: Do 'queueing' and 'closed' states apply for a-rex?
        # OBS: Is there an allownew option for a-rex?
        #if ( $config->{GridftpdAllowNew} == 0 ) {
        #    $cep->{ServingState} = 'draining';
        #} else {
        #    $cep->{ServingState} = 'production';
        #}
        $cep->{ServingState} = 'development';

        # StartTime: get it from hed

        $cep->{IssuerCA} = $host_info->{issuerca}; # scalar
        $cep->{TrustedCA} = $host_info->{trustedcas}; # array

        # TODO: Downtime, is this necessary, and how should it work?

        $cep->{Staging} =  'staginginout';
        $cep->{JobDescription} = [ 'ogf:jsdl:1.0', "nordugrid:xrsl" ];

        $cep->{TotalJobs} = $gmtotalcount{notfinished} || 0;

        $cep->{RunningJobs} = $inlrmsjobstotal{running} || 0;
        $cep->{SuspendedJobs} = $inlrmsjobstotal{suspended} || 0;
        $cep->{WaitingJobs} = $inlrmsjobstotal{queued} || 0;

        $cep->{StagingJobs} = ( $gmtotalcount{preparing} || 0 )
                + ( $gmtotalcount{finishing} || 0 );

        $cep->{PreLRMSWaitingJobs} = $pendingtotal || 0;

        if ($config->{accesspolicies}) {
        my @apconfs = @{$config->{accesspolicies}};
        $cep->{AccessPolicies} = sub {
            return undef unless @apconfs;
            my $apconf = pop @apconfs;
            my $apol = {};
            $apol->{ID} = "$apolID:".join(",", @{$apconf->{Rule}});
            $apol->{Scheme} = "basic";
            $apol->{Rule} = $apconf->{Rule};
            $apol->{UserDomainID} = $apconf->{UserDomainID};
            $apol->{EndpointID} = $EMIEScepID;
            return $apol;
        };
        }
        
        $cep->{OtherInfo} = $host_info->{EMIversion} if ($host_info->{EMIversion}); # array


        # Associations

        $cep->{ComputingShareID} = [ values %cshaIDs ];
        $cep->{ComputingServiceID} = $csvID;

        return $cep;
    };

    # don't publish if no endpoint URL
    if ($emieshostport ne '') {push(@ceps, $getEMIESComputingEndpoint); };

    # Placeholder for Stagein interface

    my $getStageinComputingEndpoint = sub {
        
        # don't publish if no Endpoint URL
        return undef unless $stageinhostport ne '';

        my $cep = {};

        $cep->{CreationTime} = $creation_time;
        $cep->{Validity} = $validity_ttl;

        $cep->{ID} = $StageincepID;

        # Name not necessary -- why? added back
        $cep->{Name} = "ARC WSRF XBES submission interface and WSRF LIDI Information System";

        # OBS: ideally HED should be asked for the URL
        #$cep->{URL} = $config->{endpoint};
        $cep->{Capability} = [ 'data.management.transfer' ];
        $cep->{Technology} = 'webservice';
        $cep->{InterfaceName} = 'Stagein';
        $cep->{InterfaceVersion} = [ '1.0' ];
        # InterfaceExtension should return the same as BESExtension attribute of BES-Factory.
        # value is taken from services/a-rex/get_factory_attributes_document.cpp, line 56.
        #$cep->{InterfaceExtension} = [ 'http://www.nordugrid.org/schemas/a-rex' ];
        $cep->{WSDL} = [ $config->{endpoint}."/?wsdl" ];
        # Wrong type, should be URI
        #$cep->{SupportedProfile} = [ "http://www.ws-i.org/Profiles/BasicProfile-1.0.html",  # WS-I 1.0
        #            "http://schemas.ogf.org/hpcp/2007/01/bp"               # HPC-BP
        #              ];
        #$cep->{Semantics} = [ "http://www.nordugrid.org/documents/arex.pdf" ];
        $cep->{Implementor} = "NorduGrid";
        $cep->{ImplementationName} = "Stagein";
        $cep->{ImplementationVersion} = $config->{arcversion};

        $cep->{QualityLevel} = "development";

        # How to calculate health for this interface?
        my %healthissues;

        if ($config->{x509_user_cert} and $config->{x509_cert_dir}) {
        if (     $host_info->{hostcert_expired}
              or $host_info->{issuerca_expired}) {
            push @{$healthissues{critical}}, "Host credentials expired";
        } elsif (not $host_info->{hostcert_enddate}
              or not $host_info->{issuerca_enddate}) {
            push @{$healthissues{critical}}, "Host credentials missing";
        } elsif ($host_info->{hostcert_enddate} - time < 48*3600
              or $host_info->{issuerca_enddate} - time < 48*3600) {
            push @{$healthissues{warning}}, "Host credentials will expire soon";
        }
        }

        if ( $host_info->{gm_alive} ne 'all' ) {
        if ($host_info->{gm_alive} eq 'some') {
            push @{$healthissues{warning}}, 'One or more grid managers are down';
        } else {
            push @{$healthissues{critical}},
              $config->{remotegmdirs} ? 'All grid managers are down'
                          : 'Grid manager is down';
        }
        }

        if (%healthissues) {
        my @infos;
        for my $level (qw(critical warning other)) {
            next unless $healthissues{$level};
            $cep->{HealthState} ||= $level;
            push @infos, @{$healthissues{$level}};
        }
        $cep->{HealthStateInfo} = join "; ", @infos;
        } else {
        $cep->{HealthState} = 'ok';
        }

        # OBS: Do 'queueing' and 'closed' states apply for a-rex?
        # OBS: Is there an allownew option for a-rex?
        #if ( $config->{GridftpdAllowNew} == 0 ) {
        #    $cep->{ServingState} = 'draining';
        #} else {
        #    $cep->{ServingState} = 'production';
        #}
        $cep->{ServingState} = 'production';

        # StartTime: get it from hed

        $cep->{IssuerCA} = $host_info->{issuerca}; # scalar
        $cep->{TrustedCA} = $host_info->{trustedcas}; # array

        # TODO: Downtime, is this necessary, and how should it work?

        $cep->{Staging} =  'staginginout';
        $cep->{JobDescription} = [ 'ogf:jsdl:1.0', "nordugrid:xrsl" ];

        $cep->{TotalJobs} = $gmtotalcount{notfinished} || 0;

        $cep->{RunningJobs} = $inlrmsjobstotal{running} || 0;
        $cep->{SuspendedJobs} = $inlrmsjobstotal{suspended} || 0;
        $cep->{WaitingJobs} = $inlrmsjobstotal{queued} || 0;

        $cep->{StagingJobs} = ( $gmtotalcount{preparing} || 0 )
                + ( $gmtotalcount{finishing} || 0 );

        $cep->{PreLRMSWaitingJobs} = $pendingtotal || 0;

        if ($config->{accesspolicies}) {
        my @apconfs = @{$config->{accesspolicies}};
        $cep->{AccessPolicies} = sub {
            return undef unless @apconfs;
            my $apconf = pop @apconfs;
            my $apol = {};
            $apol->{ID} = "$apolID:".join(",", @{$apconf->{Rule}});
            $apol->{Scheme} = "basic";
            $apol->{Rule} = $apconf->{Rule};
            $apol->{UserDomainID} = $apconf->{UserDomainID};
            $apol->{EndpointID} = $StageincepID;
            return $apol;
        };
        }
        
        $cep->{OtherInfo} = $host_info->{EMIversion} if ($host_info->{EMIversion}); # array


        # Associations

        $cep->{ComputingShareID} = [ values %cshaIDs ];
        $cep->{ComputingServiceID} = $csvID;

        return $cep;
    };

    # don't publish if no Endpoint URL
    if ($stageinhostport ne '') { push(@ceps, $getStageinComputingEndpoint); };
    

    # returns the endpoints one by one
    my $getComputingEndpoints = sub {
        return undef unless my $endpoint = pop(@ceps);
        #$log->verbose("this is the number of endpoints: ".@ceps);
	return &$endpoint;
    };

    $csv->{ComputingEndpoints} = $getComputingEndpoints;

    # Computing Activities, in the ComputingService and not in Endpoints

    my $getComputingActivities = sub {

	return undef unless my ($jobid, $gmjob) = each %$gmjobs_info;

	my $exited = undef; # whether the job has already run; 

	my $cact = {};

	$cact->{CreationTime} = $creation_time;
	$cact->{Validity} = $validity_ttl;

	my $share = $gmjob->{share};
	my $gridid = $config->{endpoint}."/$jobid";

	$cact->{Type} = 'single';
	$cact->{ID} = $cactIDs{$share}{$jobid};
	# TODO: check where is this taken
	$cact->{IDFromEndpoint} = $gmjob->{globalid} if $gmjob->{globalid};
	$cact->{Name} = $gmjob->{jobname} if $gmjob->{jobname};
	# TODO: properly set either ogf:jsdl:1.0 or nordugrid:xrsl
	$cact->{JobDescription} = $gmjob->{description} eq 'xml' ? "ogf:jsdl:1.0" : "nordugrid:xrsl" if $gmjob->{description};
	$cact->{RestartState} = glueState($gmjob->{failedstate}) if $gmjob->{failedstate};
	$cact->{ExitCode} = $gmjob->{exitcode} if defined $gmjob->{exitcode};
	# TODO: modify scan-jobs to write it separately to .diag. All backends should do this.
	$cact->{ComputingManagerExitCode} = $gmjob->{lrmsexitcode} if $gmjob->{lrmsexitcode};
	$cact->{Error} = [ @{$gmjob->{errors}} ] if $gmjob->{errors};
	# TODO: VO info, like <UserDomain>ATLAS/Prod</UserDomain>; check whether this information is available to A-REX
	$cact->{Owner} = $gmjob->{subject} if $gmjob->{subject};
	$cact->{LocalOwner} = $gmjob->{localowner} if $gmjob->{localowner};
	# OBS: Times are in seconds.
	$cact->{RequestedTotalWallTime} = $gmjob->{reqwalltime} if defined $gmjob->{reqwalltime};
	$cact->{RequestedTotalCPUTime} = $gmjob->{reqcputime} if defined $gmjob->{reqcputime};
	# OBS: Should include name and version. Exact format not specified
	$cact->{RequestedApplicationEnvironment} = $gmjob->{runtimeenvironments} if $gmjob->{runtimeenvironments};
	$cact->{RequestedSlots} = $gmjob->{count} || 1;
	$cact->{StdIn} = $gmjob->{stdin} if $gmjob->{stdin};
	$cact->{StdOut} = $gmjob->{stdout} if $gmjob->{stdout};
	$cact->{StdErr} = $gmjob->{stderr} if $gmjob->{stderr};
	$cact->{LogDir} = $gmjob->{gmlog} if $gmjob->{gmlog};
	$cact->{ExecutionNode} = $gmjob->{nodenames} if $gmjob->{nodenames};
	$cact->{Queue} = $gmjob->{queue} if $gmjob->{queue};
	# Times for finished jobs
	$cact->{UsedTotalWallTime} = $gmjob->{WallTime} * ($gmjob->{count} || 1) if defined $gmjob->{WallTime};
	$cact->{UsedTotalCPUTime} = $gmjob->{CpuTime} if defined $gmjob->{CpuTime};
	$cact->{UsedMainMemory} = ceil($gmjob->{UsedMem}/1024) if defined $gmjob->{UsedMem};
	$cact->{SubmissionTime} = mdstoiso($gmjob->{starttime}) if $gmjob->{starttime};
	# TODO: change gm to save LRMSSubmissionTime
	#$cact->{ComputingManagerSubmissionTime} = 'NotImplemented';
	# TODO: this should be queried in scan-job.
	#$cact->{StartTime} = 'NotImplemented';
	# TODO: scan-job has to produce this
	#$cact->{ComputingManagerEndTime} = 'NotImplemented';
	$cact->{EndTime} = mdstoiso($gmjob->{completiontime}) if $gmjob->{completiontime};
	$cact->{WorkingAreaEraseTime} = mdstoiso($gmjob->{cleanuptime}) if $gmjob->{cleanuptime};
	$cact->{ProxyExpirationTime} = mdstoiso($gmjob->{delegexpiretime}) if $gmjob->{delegexpiretime};
	if ($gmjob->{clientname}) {
	    # OBS: address of client as seen by the server is used.
	    my $dnschars = '-.A-Za-z0-9';  # RFC 1034,1035
	    my ($external_address, $port, $clienthost) = $gmjob->{clientname} =~ /^([$dnschars]+)(?::(\d+))?(?:;(.+))?$/;
	    $cact->{SubmissionHost} = $external_address if $external_address;
	}
	$cact->{SubmissionClientName} = $gmjob->{clientsoftware} if $gmjob->{clientsoftware};

	# Computing Activity Associations

	# TODO: add link
	#$cact->{ExecutionEnvironmentID} = ;
	$cact->{ActivityID} = $gmjob->{activityid} if $gmjob->{activityid};
	$cact->{ComputingShareID} = $cshaIDs{$share} || 'UNDEFINEDVALUE';

	if ( $gmjob->{status} eq "INLRMS" ) {
	    my $lrmsid = $gmjob->{localid};
	    if (not $lrmsid) {
		$log->warning("No local id for job $jobid") if $callcount == 1;
		next;
	    }
	    $cact->{LocalIDFromManager} = $lrmsid;

	    my $lrmsjob = $lrms_info->{jobs}{$lrmsid};
	    if (not $lrmsjob) {
		$log->warning("No local job for $jobid") if $callcount == 1;
		next;
	    }
	    $cact->{State} = glueState("INLRMS", $lrmsjob->{status});
	    $cact->{WaitingPosition} = $lrmsjob->{rank} if defined $lrmsjob->{rank};
	    $cact->{ExecutionNode} = $lrmsjob->{nodes} if $lrmsjob->{nodes};
	    unshift @{$cact->{OtherMessages}}, $_ for @{$lrmsjob->{comment}};

	    # Times for running jobs
	    $cact->{UsedTotalWallTime} = $lrmsjob->{walltime} * ($gmjob->{count} || 1) if defined $lrmsjob->{walltime};
	    $cact->{UsedTotalCPUTime} = $lrmsjob->{cputime} if defined $lrmsjob->{cputime};
	    $cact->{UsedMainMemory} = ceil($lrmsjob->{mem}/1024) if defined $lrmsjob->{mem};
	} else {
	    $cact->{State} = glueState($gmjob->{status});
	}
      
	# TODO: UserDomain association, how to calculate it?

	$cact->{jobXmlFileWriter} = sub { jobXmlFileWriter($config, $jobid, $gmjob, @_) };

	return $cact;
      };

      if ($nojobs) {
	  $csv->{ComputingActivities} = undef;
      } else {
	    $csv->{ComputingActivities} = $getComputingActivities;
      }


    # ComputingShares: multiple shares can share the same LRMS queue

    my @shares = keys %{$config->{shares}};

    my $getComputingShares = sub {

	return undef unless my ($share, $dummy) = each %{$config->{shares}};

	my $qinfo = $lrms_info->{queues}{$share};

	# Prepare flattened config hash for this share.
	my $sconfig = { %{$config->{service}}, %{$config->{shares}{$share}} };

	# List of all shares submitting to the current queue, including the current share.
	my $qname = $sconfig->{MappingQueue} || $share;

	if ($qname) {
	    my $siblings = $sconfig->{siblingshares} = [];
	    # Do NOT use 'keys %{$config->{shares}}' here because it would
	    # reset the iterator of 'each' and cause this function to
	    # always return the same result
	    for my $sn (@shares) {
		my $s = $config->{shares}{$sn};
		my $qn = $s->{MappingQueue} || $sn;
		push @$siblings, $sn if $qn eq $qname;
	    }
	}

	my $csha = {};

	$csha->{CreationTime} = $creation_time;
	$csha->{Validity} = $validity_ttl;

	$csha->{ID} = $cshaIDs{$share};

	$csha->{Name} = $share;
	$csha->{Description} = $sconfig->{Description} if $sconfig->{Description};
	$csha->{MappingQueue} = $qname if $qname;

	# use limits from LRMS
	$csha->{MaxCPUTime} = $qinfo->{maxcputime} if defined $qinfo->{maxcputime};
	# TODO: implement in backends
	$csha->{MaxTotalCPUTime} = $qinfo->{maxtotalcputime} if defined $qinfo->{maxtotalcputime};
	$csha->{MinCPUTime} = $qinfo->{mincputime} if defined $qinfo->{mincputime};
	$csha->{DefaultCPUTime} = $qinfo->{defaultcput} if defined $qinfo->{defaultcput};
	$csha->{MaxWallTime} =  $qinfo->{maxwalltime} if defined $qinfo->{maxwalltime};
	# TODO: MaxMultiSlotWallTime replaces MaxTotalWallTime, but has different meaning. Check that it's used correctly
	#$csha->{MaxMultiSlotWallTime} = $qinfo->{maxwalltime} if defined $qinfo->{maxwalltime};
	$csha->{MinWallTime} =  $qinfo->{minwalltime} if defined $qinfo->{minwalltime};
	$csha->{DefaultWallTime} = $qinfo->{defaultwallt} if defined $qinfo->{defaultwallt};

	my ($maxtotal, $maxlrms) = split ' ', ($config->{maxjobs} || '');
	$maxtotal = undef if defined $maxtotal and $maxtotal eq '-1';
	$maxlrms = undef if defined $maxlrms and $maxlrms eq '-1';

	# MaxWaitingJobs: use the maxjobs config option
	# OBS: An upper limit is not really enforced by A-REX.
	# OBS: Currently A-REX only cares about totals, not per share limits!
	$csha->{MaxTotalJobs} = $maxtotal if defined $maxtotal;

	# MaxWaitingJobs, MaxRunningJobs:
	my ($maxrunning, $maxwaiting);

	# use values from lrms if avaialble
	if (defined $qinfo->{maxrunning}) {
	    $maxrunning = $qinfo->{maxrunning};
	}
	if (defined $qinfo->{maxqueuable}) {
	    $maxwaiting = $qinfo->{maxqueuable};
	}

	# maxjobs config option sets upper limits
	if (defined $maxlrms) {
	    $maxrunning = $maxlrms
		if not defined $maxrunning or $maxrunning > $maxlrms;
	    $maxwaiting = $maxlrms
		if not defined $maxwaiting or $maxwaiting > $maxlrms;
	}

	$csha->{MaxRunningJobs} = $maxrunning if defined $maxrunning;
	$csha->{MaxWaitingJobs} = $maxwaiting if defined $maxwaiting;

	# MaxPreLRMSWaitingJobs: use GM's maxjobs option
	# OBS: Currently A-REX only cares about totals, not per share limits!
	# OBS: this formula is actually an upper limit on the sum of pre + post
	#      lrms jobs. A-REX does not have separate limit for pre lrms jobs
	$csha->{MaxPreLRMSWaitingJobs} = $maxtotal - $maxlrms
	    if defined $maxtotal and defined $maxlrms;

	$csha->{MaxUserRunningJobs} = $qinfo->{maxuserrun}
	    if defined $qinfo->{maxuserrun};

	# TODO: new return value from LRMS infocollector
	# TODO: see how LRMSs can detect the correct value
	$csha->{MaxSlotsPerJob} = $sconfig->{MaxSlotsPerJob} || $qinfo->{MaxSlotsPerJob}  || 1;

	# MaxStageInStreams, MaxStageOutStreams
	# OBS: A-REX does not have separate limits for up and downloads.
	# OBS: A-REX only cares about totals, not per share limits!
	my ($maxloaders, $maxemergency, $maxthreads) = split ' ', ($config->{maxload} || '');
	$maxloaders = undef if defined $maxloaders and $maxloaders eq '-1';
	$maxthreads = undef if defined $maxthreads and $maxthreads eq '-1';
	if ($maxloaders) {
	    # default is 5 (see MAX_DOWNLOADS defined in a-rex/grid-manager/loaders/downloader.cpp)
	    $maxthreads = 5 unless defined $maxthreads;
	    $csha->{MaxStageInStreams}  = $maxloaders * $maxthreads;
	    $csha->{MaxStageOutStreams} = $maxloaders * $maxthreads;
	}

	# TODO: new return value schedpolicy from LRMS infocollector.
	my $schedpolicy = $lrms_info->{schedpolicy} || undef;
	if ($sconfig->{SchedulingPolicy} and not $schedpolicy) {
	    $schedpolicy = 'fifo' if lc($sconfig->{SchedulingPolicy}) eq 'fifo';
	    $schedpolicy = 'fairshare' if lc($sconfig->{SchedulingPolicy}) eq 'maui';
	}
	$csha->{SchedulingPolicy} = $schedpolicy if $schedpolicy;


	# GuaranteedVirtualMemory -- all nodes must be able to provide this
	# much memory per job. Some nodes might be able to afford more per job
	# (MaxVirtualMemory)
	# TODO: implement check at job accept time in a-rex
	# TODO: implement in LRMS plugin maxvmem and maxrss.
	$csha->{MaxVirtualMemory} = $sconfig->{MaxVirtualMemory} if $sconfig->{MaxVirtualMemory};
	# MaxMainMemory -- usage not being tracked by most LRMSs

	# OBS: new config option (space measured in GB !?)
	# OBS: Disk usage of jobs is not being enforced.
	# This limit should correspond with the max local-scratch disk space on
	# clusters using local disks to run jobs.
	# TODO: implement check at job accept time in a-rex
	# TODO: see if any lrms can support this. Implement check in job wrapper
	$csha->{MaxDiskSpace} = $sconfig->{DiskSpace} if $sconfig->{DiskSpace};

	# DefaultStorageService:

	# OBS: Should be ExtendedBoolean_t (one of 'true', 'false', 'undefined')
	$csha->{Preemption} = glue2bool($qinfo->{Preemption}) if defined $qinfo->{Preemption};

	# ServingState: closed and queuing are not yet supported
	# OBS: Is there an allownew option for a-rex?
	#if ( $config->{GridftpdAllowNew} == 0 ) {
	#    $csha->{ServingState} = 'draining';
	#} else {
	#    $csha->{ServingState} = 'production';
	#}
	$csha->{ServingState} = 'production';

	# Count local jobs
	my $localrunning = $qinfo->{running};
	my $localqueued = $qinfo->{queued};
	my $localsuspended = $qinfo->{suspended} || 0;

	# Substract grid jobs submitted belonging to shares that submit to the same lrms queue
	$localrunning -= $inlrmsjobs{$_}{running} || 0 for @{$sconfig->{siblingshares}};
	$localqueued -= $inlrmsjobs{$_}{queued} || 0 for @{$sconfig->{siblingshares}};
	$localsuspended -= $inlrmsjobs{$_}{suspended} || 0 for @{$sconfig->{siblingshares}};

	# OBS: Finished/failed/deleted jobs are not counted
	my $totaljobs = $gmsharecount{$share}{notfinished} || 0;
	$totaljobs += $localrunning + $localqueued + $localsuspended;
	$csha->{TotalJobs} = $totaljobs;

	$csha->{RunningJobs} = $localrunning + ( $inlrmsjobs{$share}{running} || 0 );
	$csha->{WaitingJobs} = $localqueued + ( $inlrmsjobs{$share}{queued} || 0 );
	$csha->{SuspendedJobs} = $localsuspended + ( $inlrmsjobs{$share}{suspended} || 0 );

	# TODO: backends to count suspended jobs

	$csha->{LocalRunningJobs} = $localrunning;
	$csha->{LocalWaitingJobs} = $localqueued;
	$csha->{LocalSuspendedJobs} = $localsuspended;

	$csha->{StagingJobs} = ( $gmsharecount{$share}{preparing} || 0 )
			      + ( $gmsharecount{$share}{finishing} || 0 );

	$csha->{PreLRMSWaitingJobs} = $gmsharecount{$share}{notsubmitted} || 0;

	# TODO: investigate if it's possible to get these estimates from maui/torque
	$csha->{EstimatedAverageWaitingTime} = $qinfo->{averagewaitingtime} if defined $qinfo->{averagewaitingtime};
	$csha->{EstimatedWorstWaitingTime} = $qinfo->{worstwaitingtime} if defined $qinfo->{worstwaitingtime};

	# TODO: implement $qinfo->{freeslots} in LRMS plugins

	my $freeslots = 0;
	if (defined $qinfo->{freeslots}) {
	    $freeslots = $qinfo->{freeslots};
	} else {
	    $freeslots = $qinfo->{totalcpus} - $qinfo->{running};
	}

	# Local users have individual restrictions
	# FreeSlots: find the maximum freecpus of any local user mapped in this
	# share and use that as an upper limit for $freeslots
	# FreeSlotsWithDuration: for each duration, find the maximum freecpus
	# of any local user mapped in this share
	# TODO: is this the correct way to do it?

	my @durations;
	my %timeslots = max_userfreeslots($qinfo->{users});

	if (%timeslots) {

	    # find maximum free slots regardless of duration
	    my $maxuserslots = 0;
	    for my $seconds ( keys %timeslots ) {
		my $nfree = $timeslots{$seconds};
		$maxuserslots = $nfree if $nfree > $maxuserslots;
	    }
	    $freeslots = $maxuserslots < $freeslots
			? $maxuserslots : $freeslots;

	    # sort descending by duration, keping 0 first (0 for unlimited)
	    for my $seconds (sort { if ($a == 0) {1} elsif ($b == 0) {-1} else {$b <=> $a} } keys %timeslots) {
		my $nfree = $timeslots{$seconds} < $freeslots
			  ? $timeslots{$seconds} : $freeslots;
		unshift @durations, $seconds ? "$nfree:$seconds" : $nfree;
	    }
	}

	$freeslots = 0 if $freeslots < 0;

	$csha->{FreeSlots} = $freeslots;
	$csha->{FreeSlotsWithDuration} = join(" ", @durations) || 0;
	$csha->{UsedSlots} = $qinfo->{running};
	$csha->{RequestedSlots} = $requestedslots{$share} || 0;

	# TODO: detect reservationpolicy in the lrms
	$csha->{ReservationPolicy} = $qinfo->{reservationpolicy} if $qinfo->{reservationpolicy};

	# MappingPolicy: VOs mapped to this share.

	if (@{$config->{mappingpolicies}}) {
	    my @mpconfs = @{$config->{mappingpolicies}};
	    $csha->{MappingPolicies} = sub {
		return undef unless @mpconfs;
		my $mpconf = pop @mpconfs;
		my $mpol = {};
		$mpol->{ID} = "$mpolIDp:$share:".join(",", @{$mpconf->{Rule}});
		$mpol->{Scheme} = "basic";
		$mpol->{Rule} = $mpconf->{Rule};
		$mpol->{UserDomainID} = $mpconf->{UserDomainID};
		$mpol->{ShareID} = $cshaIDs{$share};
		return $mpol;
	    };
	}

	# Tag: skip it for now

	# Associations

	my $xenvs = $sconfig->{ExecutionEnvironmentName} || [];
	push @{$csha->{ExecutionEnvironmentID}}, $xenvIDs{$_} for @$xenvs;

	## check this association below. Which endpoint?
	# $csha->{ComputingEndpointID} = $cepID;
	$csha->{ComputingServiceID} = $csvID;

	return $csha;
    };

    $csv->{ComputingShares} = $getComputingShares;


    # ComputingManager

        my $getComputingManager = sub {

            my $cmgr = {};

            $cmgr->{CreationTime} = $creation_time;
            $cmgr->{Validity} = $validity_ttl;

            $cmgr->{ID} = $cmgrID;
            my $cluster_info = $lrms_info->{cluster}; # array

            # Name not needed

            $cmgr->{ProductName} = $cluster_info->{lrms_glue_type} || lc $cluster_info->{lrms_type};
            $cmgr->{ProductVersion} = $cluster_info->{lrms_version};
            # $cmgr->{Reservation} = "undefined";
            $cmgr->{BulkSubmission} = "false";

            #$cmgr->{TotalPhysicalCPUs} = $totalpcpus if $totalpcpus;
            $cmgr->{TotalLogicalCPUs} = $totallcpus if $totallcpus;

            # OBS: Assuming 1 slot per CPU
            $cmgr->{TotalSlots} = $cluster_info->{totalcpus};

            my $gridrunningslots = 0; $gridrunningslots += $_->{running} for values %inlrmsslots;
            my $localrunningslots = $cluster_info->{usedcpus} - $gridrunningslots;
            $cmgr->{SlotsUsedByLocalJobs} = $localrunningslots;
            $cmgr->{SlotsUsedByGridJobs} = $gridrunningslots;

            $cmgr->{Homogeneous} = $homogeneous ? "true" : "false";

            # NetworkInfo of all ExecutionEnvironments
            my %netinfo = ();
            for my $xeconfig (values %{$config->{xenvs}}) {
                $netinfo{$xeconfig->{NetworkInfo}} = 1 if $xeconfig->{NetworkInfo};
            }
            $cmgr->{NetworkInfo} = [ keys %netinfo ] if %netinfo;

            # TODO: this could also be cross-checked with info from ExecEnvs
            my $cpuistribution = $cluster_info->{cpudistribution} || '';
            $cpuistribution =~ s/cpu:/:/g;
            $cmgr->{LogicalCPUDistribution} = $cpuistribution if $cpuistribution;

            if (defined $host_info->{session_total}) {
                my $sharedsession = "true";
                $sharedsession = "false" if lc($config->{shared_filesystem}) eq "no"
                                         or lc($config->{shared_filesystem}) eq "false";
                $cmgr->{WorkingAreaShared} = $sharedsession;
                $cmgr->{WorkingAreaGuaranteed} = "false";

                my ($sessionlifetime) = (split ' ', $config->{defaultttl});
                $sessionlifetime ||= 7*24*60*60;
                $cmgr->{WorkingAreaLifeTime} = $sessionlifetime;

                my $gigstotal = ceil($host_info->{session_total} / 1024);
                my $gigsfree = ceil($host_info->{session_free} / 1024);
                $cmgr->{WorkingAreaTotal} = $gigstotal;
                $cmgr->{WorkingAreaFree} = $gigsfree;

                # OBS: There is no special area for MPI jobs, no need to advertize anything
                #$cmgr->{WorkingAreaMPIShared} = $sharedsession;
                #$cmgr->{WorkingAreaMPITotal} = $gigstotal;
                #$cmgr->{WorkingAreaMPIFree} = $gigsfree;
                #$cmgr->{WorkingAreaMPILifeTime} = $sessionlifetime;
            }
            if (defined $host_info->{cache_total}) {
                my $gigstotal = ceil($host_info->{cache_total} / 1024);
                my $gigsfree = ceil($host_info->{cache_free} / 1024);
                $cmgr->{CacheTotal} = $gigstotal;
                $cmgr->{CacheFree} = $gigsfree;
            }

            if ($config->{service}{Benchmark}) {
                my @bconfs = @{$config->{service}{Benchmark}};
                $cmgr->{Benchmarks} = sub {
                    return undef unless @bconfs;
                    my ($type, $value) = split " ", shift @bconfs;
                    my $bench = {};
                    $bench->{Type} = $type;
                    $bench->{Value} = $value;
                    $bench->{ID} = "urn:ogf:Benchmark:$servicename:$type";
                    return $bench;
                };
            }

            # Not publishing absolute paths
            #$cmgr->{TmpDir};
            #$cmgr->{ScratchDir};
            #$cmgr->{ApplicationDir};

            # ExecutionEnvironments

            my $getExecutionEnvironments = sub {

                return undef unless my ($xenv, $dummy) = each %{$config->{xenvs}};

                my $xeinfo = $xeinfos->{$xenv};

                # Prepare flattened config hash for this xenv.
                my $xeconfig = { %{$config->{service}}, %{$config->{xenvs}{$xenv}} };

                my $execenv = {};

                $execenv->{Name} = $xenv;
                $execenv->{CreationTime} = $creation_time;
                $execenv->{Validity} = $validity_ttl;

                $execenv->{ID} = $xenvIDs{$xenv};

                my $machine = $xeinfo->{machine};
                if ($machine) {
                    $machine =~ s/^x86_64/amd64/;
                    $machine =~ s/^ia64/itanium/;
                    $machine =~ s/^ppc/powerpc/;
                }
                my $sysname = $xeinfo->{sysname};
                if ($sysname) {
                    $sysname =~ s/^Linux/linux/;
                    $sysname =~ s/^Darwin/macosx/;
                    $sysname =~ s/^SunOS/solaris/;
                } elsif ($xeconfig->{OpSys}) {
                    $sysname = 'linux' if grep /linux/i, @{$xeconfig->{OpSys}};
                }

                $execenv->{Platform} = $machine if $machine;
                $execenv->{TotalInstances} = $xeinfo->{ntotal} if defined $xeinfo->{ntotal};
                $execenv->{UsedInstances} = $xeinfo->{nbusy} if defined $xeinfo->{nbusy};
                $execenv->{UnavailableInstances} = $xeinfo->{nunavailable} if defined $xeinfo->{nunavailable};
                $execenv->{VirtualMachine} = glue2bool($xeconfig->{VirtualMachine}) if defined $xeconfig->{VirtualMachine};

                $execenv->{PhysicalCPUs} = $xeinfo->{pcpus} if $xeinfo->{pcpus};
                $execenv->{LogicalCPUs} = $xeinfo->{lcpus} if $xeinfo->{lcpus};
                if ($xeinfo->{pcpus} and $xeinfo->{lcpus}) {
                    my $cpum = ($xeinfo->{pcpus} > 1) ? 'multicpu' : 'singlecpu';
                    my $corem = ($xeinfo->{lcpus} > $xeinfo->{pcpus}) ? 'multicore' : 'singlecore';
                    $execenv->{CPUMultiplicity} = "$cpum-$corem";
                }
                $execenv->{CPUVendor} = $xeconfig->{CPUVendor} if $xeconfig->{CPUVendor};
                $execenv->{CPUModel} = $xeconfig->{CPUModel} if $xeconfig->{CPUModel};
                $execenv->{CPUVersion} = $xeconfig->{CPUVersion} if $xeconfig->{CPUVersion};
                $execenv->{CPUClockSpeed} = $xeconfig->{CPUClockSpeed} if $xeconfig->{CPUClockSpeed};
                $execenv->{CPUTimeScalingFactor} = $xeconfig->{CPUTimeScalingFactor} if $xeconfig->{CPUTimeScalingFactor};
                $execenv->{WallTimeScalingFactor} = $xeconfig->{WallTimeScalingFactor} if $xeconfig->{WallTimeScalingFactor};
                $execenv->{MainMemorySize} = $xeinfo->{pmem} || "999999999999999999";  # placeholder value
                $execenv->{VirtualMemorySize} = $xeinfo->{vmem} if $xeinfo->{vmem};
                $execenv->{OSFamily} = $sysname || 'UNDEFINEDVALUE'; # placeholder value
                $execenv->{OSName} = $xeconfig->{OSName} if $xeconfig->{OSName};
                $execenv->{OSVersion} = $xeconfig->{OSVersion} if $xeconfig->{OSVersion};
                $execenv->{ConnectivityIn} = glue2bool($xeconfig->{ConnectivityIn}) || 'undefined';
                $execenv->{ConnectivityOut} = glue2bool($xeconfig->{ConnectivityOut}) || 'undefined';
                $execenv->{NetworkInfo} = [ $xeconfig->{NetworkInfo} ] if $xeconfig->{NetworkInfo};

                if ($callcount == 1) {
                    $log->warning("MainMemorySize not set for ExecutionEnvironment $xenv") unless $xeinfo->{pmem};
                    $log->warning("OSFamily not set for ExecutionEnvironment $xenv") unless $sysname;
                    $log->warning("ConnectivityIn not se for ExecutionEnvironment $xenv")
                        unless defined $xeconfig->{ConnectivityIn};
                    $log->warning("ConnectivityOut not se for ExecutionEnvironment $xenv")
                        unless defined $xeconfig->{ConnectivityOut};

                    my @missing;
                    for (qw(Platform CPUVendor CPUModel CPUClockSpeed OSFamily OSName OSVersion)) {
                        push @missing, $_ unless defined $execenv->{$_};
                    }
                    $log->info("Missing attributes for ExecutionEnvironment $xenv: ".join ", ", @missing) if @missing;
                }

                if ($xeconfig->{Benchmark}) {
                    my @bconfs = @{$xeconfig->{Benchmark}};
                    $execenv->{Benchmarks} = sub {
                        return undef unless @bconfs;
                        my ($type, $value) = split " ", shift @bconfs;
                        my $bench = {};
                        $bench->{Type} = $type;
                        $bench->{Value} = $value;
                        $bench->{ID} = "urn:ogf:Benchmark:$servicename:$xenv:$type";
                        return $bench;
                    };
                }

                # Associations

                for my $share (keys %{$config->{shares}}) {
                    my $sconfig = $config->{shares}{$share};
                    next unless $sconfig->{ExecutionEnvironmentName};
                    next unless grep { $xenv eq $_ } @{$sconfig->{ExecutionEnvironmentName}};
                    push @{$execenv->{ComputingShareID}}, $cshaIDs{$share};
                }

                $execenv->{ComputingManagerID} = $cmgrID;

                return $execenv;
            };

            $cmgr->{ExecutionEnvironments} = $getExecutionEnvironments;

            # ApplicationEnvironments

            my $getApplicationEnvironments = sub {

                return undef unless my ($rte, $rinfo) = each %$rte_info;

                my $appenv = {};

                # name and version is separated at the first dash (-) which is followed by a digit
                my ($name,$version) = ($rte, undef);
                ($name,$version) = ($1, $2) if $rte =~ m{^(.*?)-([0-9].*)$};

                $appenv->{AppName} = $name;
                $appenv->{AppVersion} = $version if defined $version;
                $appenv->{ID} = $aenvIDs{$rte};
                $appenv->{State} = $rinfo->{state} if $rinfo->{state};
                $appenv->{Description} = $rinfo->{description} if $rinfo->{description};
                #$appenv->{ParallelSupport} = 'none';

                return $appenv;
            };

            $cmgr->{ApplicationEnvironments} = $getApplicationEnvironments;

            # Associations

            $cmgr->{ComputingServiceID} = $csvID;

            return $cmgr;
        };

        $csv->{ComputingManager} = $getComputingManager;

        # Associations

        $csv->{AdminDomainID} = $adID;
        $csv->{ServiceID} = $csvID;

        return $csv;
    };

    my $getAdminDomain = sub {
        my $dom = { ID => $adID,
                    Name => $config->{AdminDomain}
                  };

        # Location and Contact goes here.
	# TODOFLO: remember to sync ForeignKeys
        if (my $lconfig = $config->{location}) {
            my $count = 1;
            $dom->{Location} = sub {
                return undef if $count-- == 0;
                my $loc = {};
                $loc->{ID} = "urn:ogf:Location:$admindomain";
                for (qw(Name Address Place PostCode Country Latitude Longitude)) {
                    $loc->{$_} = $lconfig->{$_} if defined $lconfig->{$_};
                }
                return $loc;
            }
        }
        if (my $cconfs = $config->{contacts}) {
            my $i = 0;
            $dom->{Contacts} = sub {
                return undef unless $i < length @$cconfs;
                my $cconfig = $cconfs->[$i++];
                my $detail = $cconfig->{Detail};
                my $cont = {};
                $cont->{ID} = "urn:ogf:Contact:$admindomain:$detail";
                for (qw(Name Detail Type)) {
                    $cont->{$_} = $cconfig->{$_} if $cconfig->{$_};
                }
                return $cont;
            };
        }

        return $dom;
    };

    # Other Services

    my @othersv = ();

    # Service:ARIS    

    my $getARISService = sub {

        #$callcount++;

        my $sv = {};

        $sv->{CreationTime} = $creation_time;
        $sv->{Validity} = $validity_ttl;

        $sv->{ID} = $ARISsvID;

        $sv->{Name} = "$config->{service}{ClusterName}:Service:ARIS" if $config->{service}{ClusterName}; # scalar
        $sv->{OtherInfo} = $config->{service}{OtherInfo} if $config->{service}{OtherInfo}; # array
        $sv->{Capability} = [ 'information.monitoring' ];
        $sv->{Type} = 'org.nordugrid.information.aris';

        # OBS: QualityLevel reflects the quality of the sotware
        # One of: development, testing, pre-production, production
        $sv->{QualityLevel} = 'production';

        $sv->{StatusInfo} =  $config->{service}{StatusInfo} if $config->{service}{StatusInfo}; # array

        $sv->{Complexity} = "endpoint=1,share=0,resource=0";

        my $getARISEndpoint = sub {

            # don't publish if slapd not running
            # or maybe is better to check health state.
            #return undef unless ( -e $config->{bdii_update_pid_file});

            my $ep = {};

            $ep->{CreationTime} = $creation_time;
            $ep->{Validity} = $validity_ttl;

            $ep->{ID} = $ARISepID;

            # Name not necessary -- why? added back
            $ep->{Name} = "ARC ARIS LDAP Information System";

            # Configuration parser does not contain ldap port!
            # must be updated
            # port hardcoded for tests 
            $ep->{URL} = "ldap://$hostname:$config->{SlapdPort}/";
            $ep->{Capability} = [ 'information.monitoring' ];
            $ep->{Technology} = 'LDAP';
            $ep->{InterfaceName} = 'ARIS';
            $ep->{InterfaceVersion} = [ '1.0' ];
            # Wrong type, should be URI
            #$ep->{SupportedProfile} = [ "http://www.ws-i.org/Profiles/BasicProfile-1.0.html",  # WS-I 1.0
            #            "http://schemas.ogf.org/hpcp/2007/01/bp"               # HPC-BP
            #              ];
            $ep->{Semantics} = [ "http://www.nordugrid.org/documents/arc_infosys.pdf" ];
            $ep->{Implementor} = "NorduGrid";
            $ep->{ImplementationName} = "ARIS";
            $ep->{ImplementationVersion} = $config->{arcversion};

            $ep->{QualityLevel} = "production";

            # How to calculate health for this interface?
            my %healthissues;

            if ($config->{x509_user_cert} and $config->{x509_cert_dir}) {
            if (     $host_info->{hostcert_expired}
                  or $host_info->{issuerca_expired}) {
                push @{$healthissues{critical}}, "Host credentials expired";
            } elsif (not $host_info->{hostcert_enddate}
                  or not $host_info->{issuerca_enddate}) {
                push @{$healthissues{critical}}, "Host credentials missing";
            } elsif ($host_info->{hostcert_enddate} - time < 48*3600
                  or $host_info->{issuerca_enddate} - time < 48*3600) {
                push @{$healthissues{warning}}, "Host credentials will expire soon";
            }
            }

            if (%healthissues) {
            my @infos;
            for my $level (qw(critical warning other)) {
                next unless $healthissues{$level};
                $ep->{HealthState} ||= $level;
                push @infos, @{$healthissues{$level}};
            }
            $ep->{HealthStateInfo} = join "; ", @infos;
            } else {
            $ep->{HealthState} = 'ok';
            }

            # OBS: Do 'queueing' and 'closed' states apply for a-rex?
            # OBS: Is there an allownew option for a-rex?
            #if ( $config->{GridftpdAllowNew} == 0 ) {
            #    $ep->{ServingState} = 'draining';
            #} else {
            #    $ep->{ServingState} = 'production';
            #}
            $ep->{ServingState} = 'production';

            # StartTime: get it from hed

            $ep->{IssuerCA} = $host_info->{issuerca}; # scalar
            $ep->{TrustedCA} = $host_info->{trustedcas}; # array

            # TODO: Downtime, is this necessary, and how should it work?

            if ($config->{accesspolicies}) {
            my @apconfs = @{$config->{accesspolicies}};
            $ep->{AccessPolicies} = sub {
                return undef unless @apconfs;
                my $apconf = pop @apconfs;
                my $apol = {};
                $apol->{ID} = "$apolID:".join(",", @{$apconf->{Rule}});
                $apol->{Scheme} = "basic";
                $apol->{Rule} = $apconf->{Rule};
                $apol->{UserDomainID} = $apconf->{UserDomainID};
                $apol->{EndpointID} = $ARISepID;
                return $apol;
            };
            }
            
            $ep->{OtherInfo} = $host_info->{EMIversion} if ($host_info->{EMIversion}); # array


            # Associations

            $ep->{ServiceID} = $ARISsvID;

            return $ep;
        };

        $sv->{Endpoint} = $getARISEndpoint;

        # Associations

        $sv->{AdminDomainID} = $adID;

        return $sv;
    };

    # ARIS is enabled by default.
    push(@othersv, $getARISService);

    # Service:Cache-Index

    my $getCacheIndexService = sub {
    
	my $sv = {};

	$sv->{CreationTime} = $creation_time;
	$sv->{Validity} = $validity_ttl;

	$sv->{ID} = $CacheIndexsvID;

	$sv->{Name} = "$config->{service}{ClusterName}:Service:Cache-Index" if $config->{service}{ClusterName}; # scalar
	$sv->{OtherInfo} = $config->{service}{OtherInfo} if $config->{service}{OtherInfo}; # array
	$sv->{Capability} = [ 'information.discovery' ];
	$sv->{Type} = 'org.nordugrid.information.cache-index';

	# OBS: QualityLevel reflects the quality of the sotware
	# One of: development, testing, pre-production, production
	$sv->{QualityLevel} = 'testing';

	$sv->{StatusInfo} =  $config->{service}{StatusInfo} if $config->{service}{StatusInfo}; # array

	$sv->{Complexity} = "endpoint=1,share=0,resource=0";

	#EndPoint here

	my $getCacheIndexEndpoint = sub {

	    #return undef unless ( -e $config->{bdii_update_pid_file});
	    return undef;

	    my $ep = {};

	    $ep->{CreationTime} = $creation_time;
	    $ep->{Validity} = $validity_ttl;

	    $ep->{ID} = $CacheIndexepID;

	    # Name not necessary -- why? added back
	    $ep->{Name} = "ARC Cache Index";

	    # Configuration parser does not contain ldap port!
	    # must be updated
	    # port hardcoded for tests 
	    # $ep->{URL} = "ldap://$hostname:$config->{SlapdPort}/";
	    $ep->{Capability} = [ 'information.discovery' ];
	    $ep->{Technology} = 'webservice';
	    $ep->{InterfaceName} = 'Cache-Index';
	    $ep->{InterfaceVersion} = [ '1.0' ];
	    # Wrong type, should be URI
	    #$ep->{SupportedProfile} = [ "http://www.ws-i.org/Profiles/BasicProfile-1.0.html",  # WS-I 1.0
	    #            "http://schemas.ogf.org/hpcp/2007/01/bp"               # HPC-BP
	    #              ];
	    $ep->{Semantics} = [ "http://www.nordugrid.org/documents/arc_infosys.pdf" ];
	    $ep->{Implementor} = "NorduGrid";
	    $ep->{ImplementationName} = "Cache-Index";
	    $ep->{ImplementationVersion} = $config->{arcversion};
	    $ep->{QualityLevel} = "testing";

	    # How to calculate health for this interface?
	    my %healthissues;

	    if ($config->{x509_user_cert} and $config->{x509_cert_dir}) {
	    if (     $host_info->{hostcert_expired}
		    or $host_info->{issuerca_expired}) {
		  push @{$healthissues{critical}}, "Host credentials expired";
	    } elsif (not $host_info->{hostcert_enddate}
		    or not $host_info->{issuerca_enddate}) {
		  push @{$healthissues{critical}}, "Host credentials missing";
	    } elsif ($host_info->{hostcert_enddate} - time < 48*3600
		    or $host_info->{issuerca_enddate} - time < 48*3600) {
		  push @{$healthissues{warning}}, "Host credentials will expire soon";
	    }
	    }

	    if (%healthissues) {
	    my @infos;
	    for my $level (qw(critical warning other)) {
		  next unless $healthissues{$level};
		  $ep->{HealthState} ||= $level;
		  push @infos, @{$healthissues{$level}};
	    }
	    $ep->{HealthStateInfo} = join "; ", @infos;
	    } else {
	    $ep->{HealthState} = 'ok';
	    }

	    # OBS: Do 'queueing' and 'closed' states apply for a-rex?
	    # OBS: Is there an allownew option for a-rex?
	    #if ( $config->{GridftpdAllowNew} == 0 ) {
	    #    $ep->{ServingState} = 'draining';
	    #} else {
	    #    $ep->{ServingState} = 'production';
	    #}
	    $ep->{ServingState} = 'production';

	    # StartTime: get it from hed

	    $ep->{IssuerCA} = $host_info->{issuerca}; # scalar
	    $ep->{TrustedCA} = $host_info->{trustedcas}; # array

	    # TODO: Downtime, is this necessary, and how should it work?

	    if ($config->{accesspolicies}) {
	    my @apconfs = @{$config->{accesspolicies}};
	    $ep->{AccessPolicies} = sub {
		  return undef unless @apconfs;
		  my $apconf = pop @apconfs;
		  my $apol = {};
		  $apol->{ID} = "$apolID:".join(",", @{$apconf->{Rule}});
		  $apol->{Scheme} = "basic";
		  $apol->{Rule} = $apconf->{Rule};
		  $apol->{UserDomainID} = $apconf->{UserDomainID};
		  $apol->{EndpointID} = $CacheIndexepID;
		  return $apol;
	    };
	    }
	    
	    $ep->{OtherInfo} = $host_info->{EMIversion} if ($host_info->{EMIversion}); # array


	    # Associations

	    $ep->{ServiceID} = $CacheIndexsvID;

	    return $ep;
	};

	$sv->{Endpoint} = $getCacheIndexEndpoint;

	# Associations

	$sv->{AdminDomainID} = $adID;

	return $sv;
    };
    
    # Disabled until I find a way to know if it's configured or not.
    # push(@othersv, $getCacheIndexService);
    
    # Service: HED-Control

    my $getHEDControlService = sub {

	my $sv = {};

	$sv->{CreationTime} = $creation_time;
	$sv->{Validity} = $validity_ttl;

	$sv->{ID} = $HEDControlsvID;

	$sv->{Name} = "$config->{service}{ClusterName}:Service:HED-CONTROL" if $config->{service}{ClusterName}; # scalar
	$sv->{OtherInfo} = $config->{service}{OtherInfo} if $config->{service}{OtherInfo}; # array
	$sv->{Capability} = [ 'information.discovery' ];
	$sv->{Type} = 'org.nordugrid.information.cache-index';

	# OBS: QualityLevel reflects the quality of the sotware
	# One of: development, testing, pre-production, production
	$sv->{QualityLevel} = 'pre-production';

	$sv->{StatusInfo} =  $config->{service}{StatusInfo} if $config->{service}{StatusInfo}; # array

	$sv->{Complexity} = "endpoint=1,share=0,resource=0";

	#EndPoint here

	my $getHEDControlEndpoint = sub {

	    #return undef unless ( -e $config->{bdii_update_pid_file});

	    my $ep = {};

	    $ep->{CreationTime} = $creation_time;
	    $ep->{Validity} = $validity_ttl;

	    $ep->{ID} = $HEDControlepID;

	    # Name not necessary -- why? added back
	    $ep->{Name} = "ARC HED WS Control Interface";

	    # Configuration parser does not contain ldap port!
	    # must be updated
	    # port hardcoded for tests 
	    $ep->{URL} = "$arexhostport/mgmt";
	    $ep->{Capability} = [ 'containermanagement.control' ];
	    $ep->{Technology} = 'webservice';
	    $ep->{InterfaceName} = 'HED-CONTROL';
	    $ep->{InterfaceVersion} = [ '1.0' ];
	    # Wrong type, should be URI
	    #$ep->{SupportedProfile} = [ "http://www.ws-i.org/Profiles/BasicProfile-1.0.html",  # WS-I 1.0
	    #            "http://schemas.ogf.org/hpcp/2007/01/bp"               # HPC-BP
	    #              ];
	    $ep->{Semantics} = [ "http://www.nordugrid.org/documents/" ];
	    $ep->{Implementor} = "NorduGrid";
	    $ep->{ImplementationName} = "HED-CONTROL";
	    $ep->{ImplementationVersion} = $config->{arcversion};
	    $ep->{QualityLevel} = "pre-production";

	    # How to calculate health for this interface?
	    my %healthissues;

	    if ($config->{x509_user_cert} and $config->{x509_cert_dir}) {
	    if (     $host_info->{hostcert_expired}
		    or $host_info->{issuerca_expired}) {
		  push @{$healthissues{critical}}, "Host credentials expired";
	    } elsif (not $host_info->{hostcert_enddate}
		    or not $host_info->{issuerca_enddate}) {
		  push @{$healthissues{critical}}, "Host credentials missing";
	    } elsif ($host_info->{hostcert_enddate} - time < 48*3600
		    or $host_info->{issuerca_enddate} - time < 48*3600) {
		  push @{$healthissues{warning}}, "Host credentials will expire soon";
	    }
	    }

	    if (%healthissues) {
	    my @infos;
	    for my $level (qw(critical warning other)) {
		  next unless $healthissues{$level};
		  $ep->{HealthState} ||= $level;
		  push @infos, @{$healthissues{$level}};
	    }
	    $ep->{HealthStateInfo} = join "; ", @infos;
	    } else {
	    $ep->{HealthState} = 'ok';
	    }

	    # OBS: Do 'queueing' and 'closed' states apply for a-rex?
	    # OBS: Is there an allownew option for a-rex?
	    #if ( $config->{GridftpdAllowNew} == 0 ) {
	    #    $ep->{ServingState} = 'draining';
	    #} else {
	    #    $ep->{ServingState} = 'production';
	    #}
	    $ep->{ServingState} = 'production';

	    # StartTime: get it from hed

	    $ep->{IssuerCA} = $host_info->{issuerca}; # scalar
	    $ep->{TrustedCA} = $host_info->{trustedcas}; # array

	    # TODO: Downtime, is this necessary, and how should it work?

	    if ($config->{accesspolicies}) {
	    my @apconfs = @{$config->{accesspolicies}};
	    $ep->{AccessPolicies} = sub {
		  return undef unless @apconfs;
		  my $apconf = pop @apconfs;
		  my $apol = {};
		  $apol->{ID} = "$apolID:".join(",", @{$apconf->{Rule}});
		  $apol->{Scheme} = "basic";
		  $apol->{Rule} = $apconf->{Rule};
		  $apol->{UserDomainID} = $apconf->{UserDomainID};
		  $apol->{EndpointID} = $HEDControlepID;
		  return $apol;
	      };
	    };
	    
	    $ep->{OtherInfo} = $host_info->{EMIversion} if ($host_info->{EMIversion}); # array


	    # Associations

	    $ep->{ServiceID} = $HEDControlsvID;

	    return $ep;
	};

	$sv->{Endpoint} = $getHEDControlEndpoint;

	# Associations

	$sv->{AdminDomainID} = $adID;

	return $sv;
    };

    # Disabled until I find a way to know if it's configured or not.
    # push(@othersv, $getHEDControlService);
    

    # aggregates services

    my $getServices = sub {
    
        return undef unless my $sub = pop(@othersv);
        # returns the hash for Entries. Odd, must understand this behaviour
        return &$sub;
             
    };
    
    # TODO: UserDomain
    
    my $getUserDomain = sub {

	my $ud = {};

	$ud->{CreationTime} = $creation_time;
	$ud->{Validity} = $validity_ttl;

	$ud->{ID} = $udID;

	$ud->{Name} = "";
	$ud->{OtherInfo} = $config->{service}{OtherInfo} if $config->{service}{OtherInfo}; # array
	$ud->{Description} = '';

	# Number of hops to reach the root
	$ud->{Level} = 0;
	# Endpoint of some service, such as VOMS server
	$ud->{UserManager} = 'org.nordugrid.information.cache-index';
	# List of users
	$ud->{Member} = [ 'users here' ];
	
	# TODO: Calculate Policies, ContactID and LocationID
	
	# Associations
	$ud->{UserDomainID} = $udID;

    };

    # TODO: ToStorageElement

    my $getToStorageElement = sub {
      
	my $tse = {};

	$tse->{CreationTime} = $creation_time;
	$tse->{Validity} = $validity_ttl;

	$tse->{ID} = $tseID;

	$tse->{Name} = "";
	$tse->{OtherInfo} = ''; # array
	
	# Local path on the machine to access storage, for example a NFS share
	$tse->{LocalPath} = 'String';
  
	# Remote path in the Storage Service associated with the local path above
	$tse->{RemotePath} = 'String';
	
	# Associations
	$tse->{ComputingService} = $csvID;
	$tse->{StorageService} = '';
    };


    # returns the two branches for =grid and =resoruce GroupID.
    # It's not optimal but it doesn't break recursion
    my $GLUE2InfoTreeRoot = sub {
        my $treeroot = { AdminDomain => $getAdminDomain,
			 UserDomain => $getUserDomain,
                         ComputingService => $getComputingService,
                         Services => $getServices
                       };
        return $treeroot;
    };

    return $GLUE2InfoTreeRoot;

}

1;

