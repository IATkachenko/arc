package PBS;

@ISA = ('Exporter');

# Module implements these subroutines for the LRMS interface

@EXPORT_OK = ('cluster_info',
	      'queue_info',
	      'jobs_info',
	      'users_info');

use LogUtils ( 'start_logging', 'error', 'warning', 'debug' ); 
use strict;

##########################################
# Saved private variables
##########################################

our(%lrms_queue);
my (%user_jobs_running, %user_jobs_queued);

# the queue passed in the latest call to queue_info, jobs_info or users_info
my $currentqueue = undef;

# Resets queue-specific global variables if
# the queue has changed since the last call
sub init_globals($) {
    my $qname = shift;
    if (not defined $currentqueue or $currentqueue ne $qname) {
        $currentqueue = $qname;
        %lrms_queue = ();
        %user_jobs_running = ();
        %user_jobs_queued = ();
    }
}

##########################################
# Private subs
##########################################

sub read_pbsnodes ($) { 

    #processing the pbsnodes output by using a hash of hashes
    # %hoh_pbsnodes (referrenced by $hashref)
 
    my ( $path ) = shift;
    my ( %hoh_pbsnodes);
    my ($nodeid,$node_var,$node_value);

    unless (open PBSNODESOUT,  "$path/pbsnodes -a 2>/dev/null |") {
	error("error in executing pbsnodes");
    }
    while (my $line= <PBSNODESOUT>) {	    
	if ($line =~ /^$/) {next}; 
	if ($line =~ /^([\w\-]+)/) {		 
   	    $nodeid= $1 ;
   	    next;	    
	}
	if ($line =~ / = /)  {
	    ($node_var,$node_value) = split (/ = /, $line);
	    $node_var =~ s/\s+//g;
	    chop $node_value;  	     
	}	     
	$hoh_pbsnodes{$nodeid}{$node_var} = $node_value;     
    } 
    close PBSNODESOUT;
    return %hoh_pbsnodes;
}


############################################
# Public subs
#############################################

sub cluster_info ($) {

    # Path to LRMS commands
    my ($config) = shift;
    my ($path) = $$config{pbs_bin_path};

    # Return data structure %lrms_cluster{$keyword}
    # should contain the keywords listed in LRMS.pm.
    #
    # All values should be defined, empty values "" are ok if field
    # does not apply to particular LRMS.

    my (%lrms_cluster);

    #determine the flavour and version of PBS"
    my $qmgr_string=`$path/qmgr -c "list server"`;
    if ( $? != 0 ) {    
	warning("Can't run qmgr");
    }
    if ($qmgr_string =~ /pbs_version = \b(\D+)_(\d\S+)\b/) {
      $lrms_cluster{lrms_type} = $1;
      $lrms_cluster{lrms_glue_type}=lc($1);
      $lrms_cluster{lrms_version} = $2;
    }
    else {
	$qmgr_string =~ /pbs_version = \b(\d\S+)\b/;
	$lrms_cluster{lrms_type}="torque";
	if (lc($$config{scheduling_policy}) eq "maui") {
	    $lrms_cluster{lrms_glue_type}="torquemaui"
	} else {
	    $lrms_cluster{lrms_glue_type}="torque";
	}
	$lrms_cluster{lrms_version}=$1;
    }

    # processing the pbsnodes output by using a hash of hashes %hoh_pbsnodes
    my ( %hoh_pbsnodes ) = read_pbsnodes( $path );

    error("The given flavour of PBS $lrms_cluster{lrms_type} is not supported")
        unless grep {$_ eq lc($lrms_cluster{lrms_type})}
            qw(openpbs spbs torque pbspro);

    $lrms_cluster{totalcpus} = 0;
    my ($number_of_running_jobs) = 0;
    $lrms_cluster{cpudistribution} = "";
    my (@cpudist) = 0;

    foreach my $node (keys %hoh_pbsnodes){

	if ( exists $$config{dedicated_node_string} ) {
	    unless ( $hoh_pbsnodes{$node}{"properties"} =~
		     m/$$config{dedicated_node_string}/) {
		next;
	    }
	}

	my ($nodestate) = $hoh_pbsnodes{$node}{"state"};      

	my $nodecpus;
        if ($hoh_pbsnodes{$node}{'np'}) {
	    $nodecpus = $hoh_pbsnodes{$node}{'np'};
        } elsif ($hoh_pbsnodes{$node}{'resources_available.ncpus'}) {
	    $nodecpus = $hoh_pbsnodes{$node}{'resources_available.ncpus'};
        }

	if ($nodestate=~/down/ or $nodestate=~/offline/) {
	    next;
	}
        if ($nodestate=~/(?:,|^)busy/) {
           $lrms_cluster{totalcpus} += $nodecpus;
	   $cpudist[$nodecpus] +=1;
	   $number_of_running_jobs += $nodecpus;
	   next;
        }
	

	$lrms_cluster{totalcpus} += $nodecpus;

	$cpudist[$nodecpus] += 1;

	if ($hoh_pbsnodes{$node}{"jobs"}){
	    $number_of_running_jobs++;
	    my ( @comma ) = ($hoh_pbsnodes{$node}{"jobs"}=~ /,/g);
	    $number_of_running_jobs += @comma;
	} 
    }      

    for (my $i=0; $i<=$#cpudist; $i++) {
	next unless ($cpudist[$i]);  
	$lrms_cluster{cpudistribution} .= " ".$i."cpu:".$cpudist[$i];
    }

    # read the qstat -n information about all jobs
    # queued cpus, total number of cpus in all jobs

    $lrms_cluster{usedcpus} = 0;
    my ($totalcpus) = 0;
    $lrms_cluster{queuedjobs} = 0;
    $lrms_cluster{runningjobs} = 0;
    unless (open QSTATOUTPUT,  "$path/qstat -n 2>/dev/null |") {
	error("Error in executing qstat");
    }
    #This variable is used so we know what state the job is in when
    #counting nodes
    my $statusline;
    while (my $line= <QSTATOUTPUT>) {       
	if ( $line =~ m/^\d+/ ) {
	    my ($jid, $user, $queue, $jname, $sid, $nds, $tsk, $reqmem,
		$reqtime, $state, $etime) = split " ", $line;
	    if ( $state eq "R" ) {
		$lrms_cluster{runningjobs}++;
	    } elsif( $state =~ /^(W|T|Q)$/){
		$lrms_cluster{queuedjobs}++;
	    }
	    $statusline=$line;
	}
	#NOTE This does not take dedicated  nodes into account, is it possible to do that?
	#     Should it do that?
	#TODO Lines that are split funny will not be counted
	#nodelist is of the format: nodename/cpunumber+nodename/cpunumber
	elsif ( $line =~ m/\w*[a-zA-Z0-9]+\/[0-9]+/ ){
	    #Count slashes to decide how many cpus are used.
	    #Alternative way would be to get resource_list.nodes
	    #from qstat -f, but that is also a free-format string
	    #which can contain cumbersome attributes.
	    my $count=0;
	    $count++ while $line =~ /\//g;

	    my ($jid, $user, $queue, $jname, $sid, $nds, $tsk, $reqmem,
		$reqtime, $state, $etime) = split " ", $statusline;
	    if ( $state eq "R" ){
		$lrms_cluster{usedcpus} += $count;
		$totalcpus+=$count;
	    }
	    elsif ($state =~ /^(W|T|Q)$/){
		$totalcpus+=$count;
	    }

	}
    }

    close QSTATOUTPUT;
    $lrms_cluster{queuedcpus} = $totalcpus - $lrms_cluster{usedcpus};



    # Names of all LRMS queues

    @{$lrms_cluster{queue}} = ();
    unless (open QSTATOUTPUT,  "$path/qstat -Q 2>/dev/null |") {
	error("Error in executing qstat");
    }
    while (my $line= <QSTATOUTPUT>) {
	if ( $. == 1 or $. == 2 ) {next} # Skip header lines
	my (@a) = split " ", $line;
	push @{$lrms_cluster{queue}}, $a[0]; 
    }
    close QSTATOUTPUT;

    return %lrms_cluster;
}

sub queue_info ($$) {

    # Path to LRMS commands
    my ($config) = shift;
    my ($path) = $$config{pbs_bin_path};

    # Name of the queue to query
    my ($qname) = shift;

    init_globals($qname);

    # The return data structure is %lrms_queue.
    # In this template it is defined as persistent module data structure,
    # because it is later used by jobs_info(), and we wish to avoid
    # re-construction of it. If it were not needed later, it would be defined
    # only in the scope of this subroutine, as %lrms_cluster previously.

    # Return data structure %lrms_queue{$keyword}
    # should contain the keyvords listed in LRMS.pm.
    #
    # All values should be defined, empty values "" are ok if field
    # does not apply to particular LRMS.

    # read the queue information for the queue entry from the qstat

    my (%qstat);
    unless (open QSTATOUTPUT,   "$path/qstat -f -Q $qname 2>/dev/null |") {
	error("Error in executing qstat: $path/qstat -f -Q $qname");
    }
    while (my $line= <QSTATOUTPUT>) {       
	if ($line =~ m/ = /) {
	    chomp($line);	 
	    my ($qstat_var,$qstat_value) = split("=", $line);	
	    $qstat_var =~ s/\s+//g; 
	    $qstat_value =~ s/\s+//g;    	 
	    $qstat{$qstat_var}=$qstat_value;
	}
    }	    
    close QSTATOUTPUT;

    my (%keywords) = ( 'max_running' => 'maxrunning',
		       'max_user_run' => 'maxuserrun',
		       'max_queuable' => 'maxqueuable' );

    foreach my $k (keys %keywords) {
	if (defined $qstat{$k} ) {
	    $lrms_queue{$keywords{$k}} = $qstat{$k};
	} else {
	    $lrms_queue{$keywords{$k}} = "";
	}
    }

    %keywords = ( 'resources_max.cput' => 'maxcputime',
		  'resources_min.cput' => 'mincputime',
		  'resources_default.cput' => 'defaultcput',
		  'resources_max.walltime' => 'maxwalltime',
		  'resources_min.walltime' => 'minwalltime',
		  'resources_default.walltime' => 'defaultwallt' );

    foreach my $k (keys %keywords) {
	if ( defined $qstat{$k} ) {
	    my @time=split(":",$qstat{$k});
	    $lrms_queue{$keywords{$k}} = ($time[0]*60+$time[1]+($k eq 'resources_min.cput'?1:0));
	} else {
	    $lrms_queue{$keywords{$k}} = "";
	}
    }

    # determine the queue status from the LRMS
    # Used to be set to 'active' if the queue can accept jobs
    # Now lists the number of available processors, "0" if no free
    # cpus. Negative number signals some error state of PBS
    # (reserved for future use).

    $lrms_queue{status} = -1;
    $lrms_queue{running} = 0;
    $lrms_queue{queued} = 0;
    $lrms_queue{totalcpus} = 0;
    if ( ($qstat{"enabled"} =~ /True/) and ($qstat{"started"} =~ /True/)) {
	unless (open QSTATOUTPUT,   "$path/qstat -f -Q $qname 2>/dev/null |") {
	    error("Error in executing qstat: $path/qstat -f -Q $qname");
	}

	my %qstat;
	while (my $line= <QSTATOUTPUT>) {
	    if ($line =~ m/ = /) {
		chomp($line);
		my ($qstat_var,$qstat_value) = split("=", $line);
		$qstat_var =~ s/\s+//g;
		$qstat_value =~ s/\s+//g;
		$qstat{$qstat_var}=$qstat_value;
	    }
	}
	close QSTATOUTPUT;


	my (%keywords) = ('max_running' => 'totalcpus',
			  'total_jobs' => 'total',
			  'resources_assigned.nodect' => 'running');
	
	foreach my $k (keys %keywords) {
	    if (defined $qstat{$k} ) {
		$lrms_queue{$keywords{$k}} = $qstat{$k};
	    } else {
		$lrms_queue{$keywords{$k}} = "";
	    }
	}

	if(defined $$config{totalcpus}){
	    if ( $$config{totalcpus} < $lrms_queue{totalcpus} ) {
		$lrms_queue{totalcpus}=$$config{totalcpus};
	    }
	}

	$lrms_queue{status}=$lrms_queue{totalcpus}-$lrms_queue{running};
	$lrms_queue{status}=0 if $lrms_queue{status} < 0;

	if ( $qstat{state_count} =~ m/.*Queued:([0-9]*).*/ ){
	    $lrms_queue{queued}=$1;
	} else {
	    $lrms_queue{queued}=0;
	}

    }

    return %lrms_queue;
}


sub jobs_info ($$@) {

    # Path to LRMS commands
    my ($config) = shift;
    my ($path) = $$config{pbs_bin_path};

    # Name of the queue to query
    my ($qname) = shift;

    init_globals($qname);

    # LRMS job IDs from Grid Manager (jobs with "INLRMS" GM status)
    my ($jids) = shift;

    # Return data structure %lrms_jobs{$lrms_local_job_id}{$keyword}
    # should contain the keywords listed in LRMS.pm.
    #
    # All values should be defined, empty values "" are ok if field
    # does not apply to particular LRMS.

    my (%lrms_jobs);

    # Fill %lrms_jobs here (da implementation)

    # rank is treated separately as it does not have an entry in
    # qstat output, comment because it is an array, and mem
    # because "kB" needs to be stripped from the value
    my (%skeywords) = ('job_state' => 'status');
    
    my (%tkeywords) = ( 'resources_used.walltime' => 'walltime',
			'resources_used.cput' => 'cputime',
			'Resource_List.walltime' => 'reqwalltime',
			'Resource_List.cputime' => 'reqcputime');

    # Keywords that should not be translated
    my (%nkeywords) = ('Resource_List.nodect' => 'cpus');

    my ($alljids) = join ' ', @{$jids};
    my ($jid) = 0;
    my ($rank) = 0;
    # better rank for maui
    my %showqrank;
    if (lc($$config{scheduling_policy}) eq "maui") {
	my $showq = (defined $$config{maui_bin_path}) ? $$config{maui_bin_path}."/showq" : "showq";
	unless (open SHOWQOUTPUT, " $showq |"){
	    error("error in executing $showq ");
	}
	my $idle=-1;
	while(my $line=<SHOWQOUTPUT>) {
	    if($line=~/^IDLE.+/) {
		$idle=0;
		$line=<SHOWQOUTPUT>;$line=<SHOWQOUTPUT>;
	    }
	    next if $idle == -1;
	    if ($line=~/^(\d+).+/) {
		$idle++;
		$showqrank{$1}=$idle;	
	    } 
	}
	close SHOWQOUTPUT;
    }
    unless (open QSTATOUTPUT,   "$path/qstat -f 2>/dev/null |") {
	error("Error in executing qstat: $path/qstat -f ");
    }

    my %job_owner;
    while (my $line = <QSTATOUTPUT>) {       
	if ($line =~ /^Job Id:\s+(\d+.*)/) {
	    $jid = 0;
	    foreach my $k ( @{$jids}) {
		if ( $1 =~ /^$k/ ) { 
		    $jid = $k;
		    last;
		}
	    }
	    next;
	}

	if ( ! $jid ) {next}

	my ( $k, $v );
	( $k, $v ) = split ' = ', $line;

	$k =~ s/\s+(.*)/$1/;
	chomp $v;

	if ( defined $skeywords{$k} ) {
	    $lrms_jobs{$jid}{$skeywords{$k}} = $v;
	    if($k eq "job_state") {
	    	if( $v eq "U" ) {
		  $lrms_jobs{$jid}{status} = "S";
		}
		if ( $v ne "R" and $v ne "Q" and $v ne "U" and $v ne "S" and 
		     $v ne "E" ) {
		  $lrms_jobs{$jid}{status} = "O";
		}
	    }
	} elsif ( defined $tkeywords{$k} ) {
	    my ( @t ) = split ':',$v;
	    $lrms_jobs{$jid}{$tkeywords{$k}} = $t[0]*60+$t[1];
	} elsif (defined $nkeywords{$k} ) {
	    $lrms_jobs{$jid}{$nkeywords{$k}} = $v;
	} elsif ( $k eq 'exec_host' ) {
	    #move hostnames from n12/3 syntax to n12
	    $v =~ s/\/\d+//g;
	    #nodes expected in list, split on +, \Q is necessary 
	    #because + is a special character.
	    @{$lrms_jobs{$jid}{nodes}} = split("\Q+",$v) ;
	} elsif ( $k eq 'comment' ) {
            $lrms_jobs{$jid}{comment} = []
                unless $lrms_jobs{$jid}{comment};
            push @{$lrms_jobs{$jid}{comment}}, "LRMS: $v";
	} elsif ($k eq 'resources_used.vmem') {
	    $v =~ s/(\d+).*/$1/;
	    $lrms_jobs{$jid}{mem} = $v;
	}
	
	if ( $k eq 'Job_Owner' ) {
	    $v =~ /(\S+)@/;
	    $job_owner{$jid} = $1;
	}
	if ( $k eq 'job_state' ) {
	    if ($v eq 'R') {
		$lrms_jobs{$jid}{rank} = "";
	    } else {
		$rank++;
		$lrms_jobs{$jid}{rank} = $rank;
	 	$jid=~/^(\d+).+/;
		if (defined $showqrank{$1}) {
			$lrms_jobs{$jid}{rank} = $showqrank{$1};
		}
	    }
	    if ($v eq 'R' or 'E'){
		++$user_jobs_running{$job_owner{$jid}};
	    }
	    if ($v eq 'Q'){ 
		++$user_jobs_queued{$job_owner{$jid}};
	    }
	}
    }
    close QSTATOUTPUT;

    my (@scalarkeywords) = ('status', 'rank', 'mem', 'walltime', 'cputime',
			    'reqwalltime', 'reqcputime');

    foreach $jid ( @$jids ) {
	foreach my $k ( @scalarkeywords ) {
	    if ( ! defined $lrms_jobs{$jid}{$k} ) {
		$lrms_jobs{$jid}{$k} = "";
	    }
	}
	$lrms_jobs{$jid}{comment} = [] unless $lrms_jobs{$jid}{comment};
	$lrms_jobs{$jid}{nodes} = [] unless $lrms_jobs{$jid}{nodes};
    }

    return %lrms_jobs;
}


sub users_info($$@) {

    # Path to LRMS commands
    my ($config) = shift;
    my ($path) = $$config{pbs_bin_path};

    # Name of the queue to query
    my ($qname) = shift;

    init_globals($qname);

    # Unix user names mapped to grid users
    my ($accts) = shift;

    # Return data structure %lrms_users{$unix_local_username}{$keyword}
    # should contain the keywords listed in LRMS.pm.
    #
    # All values should be defined, empty values "" are ok if field
    # does not apply to particular LRMS.

    my (%lrms_users);


    # Check that users have access to the queue
    unless (open QSTATOUTPUT,   "$path/qstat -f -Q $qname 2>/dev/null |") {
	error("Error in executing qstat: $path/qstat -f -Q $qname");
    }
    my $acl_user_enable = 0;
    my @acl_users;
    my $more_acls = 0;
    while (my $line= <QSTATOUTPUT>) {   
        chomp $line;

	# is this a continuation of the acl line?
	if ($more_acls) {
	    $line =~ s/^\s*//;  # strip leading spaces
	    push @acl_users, split ',', $line;
	    $more_acls = 0 unless $line =~ /,\s*$/;
	    next;
	}

	if ( $line =~ /\s*acl_user_enable/ ) {
	    my ( $k ,$v ) = split ' = ', $line;
	    unless ( $v eq 'False' ) {
	      $acl_user_enable = 1;
	    }
	}
	if ( $line =~ /\s*acl_users/ ) {
	    my ( $k ,$v ) = split ' = ', $line;
	    unless ( $v eq 'False' ) {
	      # This condition is kept here in case the reason
	      # for it being there in the first place was that some
	      # version or flavour of PBS really has False as an alternative
	      # to usernames to indicate the absence of user access control
	      # A Corrallary: Dont name your users 'False' ...
		push @acl_users, split ',', $v;
		$more_acls = 1 if $v =~ /,\s*$/;
	    }
	}
    }
    close QSTATOUTPUT;

    # acl_users is only in effect when acl_user_enable is true
    if ($acl_user_enable) {
	foreach my $a ( @{$accts} ) {
	    if (  grep { $a eq $_ } @acl_users ) {
	      # The acl_users list has to be sent back to the caller.
	      # This trick works because the config hash is passed by
	      # reference.
	      push @{$$config{acl_users}}, $a;
	    }
	    else {		       
		warning("Local user $a does not ".
			"have access in queue $qname.");
	    }
	}
    } else {
	delete $$config{acl_users};
    }

    # Uses saved module data structure %lrms_queue, which
    # exists if queue_info is called before
    if ( ! exists $lrms_queue{status} ) {
	%lrms_queue = queue_info( $config, $qname );
    }

    foreach my $u ( @{$accts} ) {
	if (lc($$config{scheduling_policy}) eq "maui") {
	    my $maui_freecpus;
	    my $showbf = (defined $$config{maui_bin_path}) ? $$config{maui_bin_path}."/showbf" : "showbf";
	    if (exists $$config{dedicated_node_string}) {
	    	$showbf.=" -f ".$$config{dedicated_node_string};
	    }
	    unless (open SHOWBFOUTPUT, " $showbf -u $u |"){
		error("error in executing $showbf -u $u ");
	    }
	    while (my $line= <SHOWBFOUTPUT>) {				    
		if ($line =~ /^partition/) {  				    
		    last;
		}
		if ($line =~ /no procs available/) {
		    $maui_freecpus= " 0";
		    last;
		}
		if ($line =~ /(\d+).+available for\s+([\w:]+)/) {
		    my @tmp= reverse split /:/, $2;
		    my $minutes=$tmp[1] + 60*$tmp[2] + 24*60*$tmp[3];
		    $maui_freecpus .= " ".$1.":".$minutes;
		}
		if ($line =~ /(\d+).+available with no timelimit/) {  	    
		    $maui_freecpus.= " ".$1;
		    last; 
		}
	    }
	    #TODO free cpus are actually free jobs
	    $lrms_users{$u}{freecpus} = $maui_freecpus;
	    $lrms_users{$u}{queuelength} = $user_jobs_queued{$u} || 0;
	}
	else {
	    if ($lrms_queue{maxuserrun} and ($lrms_queue{maxuserrun} - $user_jobs_running{$u}) < $lrms_queue{status} ) {
		$lrms_users{$u}{freecpus} = $lrms_queue{maxuserrun} - $user_jobs_running{$u};
	    }
	    else {
		#TODO free cpus are actually free jobs
		$lrms_users{$u}{freecpus} = $lrms_queue{status};
	    }
	    $lrms_users{$u}{queuelength} = "$lrms_queue{queued}";
	    #TODO free cpus are actually free jobs
	    if ($lrms_users{$u}{freecpus} < 0) {
		$lrms_users{$u}{freecpus} = 0;
	    }
	    if ($lrms_queue{maxcputime} and $lrms_users{$u}{freecpus} > 0) {
		$lrms_users{$u}{freecpus} .= ':'.$lrms_queue{maxcputime};
	    }
	}
    }
    return %lrms_users;
}
	      


1;
