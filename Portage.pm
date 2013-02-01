package Portage;

# Copyright 1999-2005 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-src/ufed/Portage.pm,v 1.3 2005/11/13 00:28:17 truedfx Exp $

use strict;
use warnings;

BEGIN {
    use Exporter ();
    
    our @ISA         = qw(Exporter);
    our %EXPORT_TAGS = ();
    our @EXPORT_OK   = ();
}


# --- public members ---

# $use_flags - hashref that represents the combined and
# consolidated data about all valid use flags
# Layout of $use_flags->{flag_name}:
# {count}  = number of different description lines
#  Note: +1 for the list of affected packages, and +1 for each descriptionless package with settings differing from global.
# {global} = hashref for the global paramters if the flag has a description in use.desc, otherwise undefined
#   ->{affected}  = List of packages that are affected but have no own description
#   ->{conf}      = The flag is disabled (-1), enabled (1) or not set (0) in make.conf
#   ->{default}   = The flag is disabled (-1), enabled (1) or not set (0) by default
#   ->{descr}     = Global description
#   ->{forced}    = The flag is globally force enabled (and masked) (0,1)
#   ->{installed} = At least one affected package is installed (0,1)
#   ->{masked}    = The flag is globally masked (0,1)
#     Note: When a flag is forced, {masked} is set to one, but can be reset to 0 by any later use.mask file.
# {"local"}->{package} = hashref for per package settings
#   ->{descr}     = Description from use.local.desc or empty if there is no individual description
#     Note: Packages without description are only listed here if their settings differ from the global
#   ->{forced}    = The flag is explicitly unforced (-1), default (0) or explicitly force enabled (1) for this package
#   ->{installed} = This package is installed
#   ->{masked}    = The flag is explicitly unmasked (-1), default (0) or explicitly masked (1) for this package
#   ->{package}   = The flag is explicitly disabled (-1), default (0) or explicitly enabled (1) for this package.
#     Note: This is a combination of the ebuilds IUSE and the installation PKGUSE and only set for installed packages.
our $use_flags;

# $used_make_conf - path of the used make.conf
our $used_make_conf = "";
our $EPREFIX        = "";

# --- private members ---
my %_environment  = ();
my @_profiles     = ();
# $_use_temp - hashref that represents the current state of
# all known flags. This is for data gathering, the public
# $use_flags is generated out of this by _gen_use_flags()
# Layout of $_use_temp->{flag_name}:
# {global}  = conf hash for global settings
# {"local"}-> {package} = conf hash for per package settings
# global and per package settings:
# ->{conf}      Is either disabled, left alone or enabled by make.conf (-1, 0, 1)
# ->{default}   Is either disabled, left alone or enabled by make.defaults (-1, 0, 1)
# ->{descr}     Description from use.desc ({global}) or use.local.desc {cat/pkg} (string)
# ->{forced}    Is force enabled (implies {masked}=1) in any *use.force
#               For packages this is only set to -1 (explicitly unforced) or +1 (explicitly forced). 0 means "left alone".
# ->{installed} Has one installed package ({global}) or is installed {cat/pkg} (0,1)
# ->{masked}    Is masked by any *use.mask (0,1)
#               For packages this is only set to -1 (explicitly unmasked) or +1 (explicitly masked). 0 means "left alone".
# ->{package}   Is either disabled, left alone or enabled by its ebuild (IUSE) or by user package.use (PKGUSE) (-1, 0, 1)

my $_use_temp = undef;
my $_use_template = {
	conf      => 0,
	"default" => 0,
	descr     => "",
	forced    => 0,
	installed => 0,
	masked    => 0,
	"package" => 0
};

# --- public methods ---

# --- private methods ---
sub _add_flag;
sub _add_temp;
sub _determine_eprefix;
sub _determine_make_conf;
sub _determine_profiles;
sub _final_cleaning;
sub _gen_use_flags;
sub _merge;
sub _merge_env;
sub _noncomments;
sub _norm_path;
sub _read_archs;
sub _read_descriptions;
sub _read_make_conf;
sub _read_make_defaults;
sub _read_make_globals;
sub _read_packages;
sub _read_sh;
sub _read_use_force;
sub _read_use_mask;

# --- Package initialization ---
INIT {
	$_environment{$_} = {} for qw{USE};
	_determine_eprefix;
	_determine_make_conf;
	_determine_profiles;
	_read_make_globals;  
	_read_make_defaults; 
	_read_make_conf;

	# Now with the defaults loaded, a check is in order
	# whether the set USE_ORDER is supported:
	defined($_environment{USE_ORDER})
		or die("Unable to determine USE_ORDER!\nSomething is seriously broken!\n");
	my $lastorder = "";
	my @use_order = reverse split /:/, $_environment{USE_ORDER};
	for my $order(@use_order) {
		if($order eq 'defaults') {
			_read_make_defaults
		} elsif($order eq 'conf') {
			_read_make_conf
		} else {
			next;
		}
		$lastorder = $order;
	}
	$lastorder eq 'conf'
		or die("Sorry, USE_ORDER without make.conf overriding global"
			. " USE flags are not currently supported by ufed.\n");

	_read_packages;
	_read_use_force; ## Must be before _read_use_mask to not
	_read_use_mask;  ## unintentionally unmask explicitly masked flags.
	_read_archs;
	_read_descriptions;
	_final_cleaning;
	_gen_use_flags;
}

# --- public methods implementations ---


# --- private methods implementations ---

# Add a flag to $use_flags and intialize it with the given
# description hash key.
# Parameter 1: flag
# Parameter 2: Keyword "global" or the package name
# Parameter 3: description hash key
sub _add_flag
{
	my ($flag, $pkg, $descKey) = @_;
	
	if ($descKey =~ /^\[(.*)\](-?\d+):(-?\d+):(-?\d+):(-?\d+):(-?\d+):(-?\d+)$/ ) {
		my ($descr, $conf, $default, $forced, $installed, $masked, $package)
			= ($1, $2, $3, $4, $5, $6, $7);
		my %data = ();

		$data{descr}     = $descr;
		$data{forced}    = $forced;
		$data{installed} = $installed;
		$data{masked}    = $masked;
		$data{"package"} = $package;
		if ("global" eq "$pkg") {
			$data{affected}  = [];
			$data{conf}      = $conf;
			$data{"default"} = $default;
			%{$use_flags->{$flag}{global}} = %data;
		} else {
			%{$use_flags->{$flag}{"local"}{$pkg}} = %data;
		}
		++$use_flags->{$flag}{count} if (length($descr));


	} else {
		die ("\n\"$flag\" ($pkg) Description Hash Key\n  \"$descKey\"\nis illegal!\n");
	}

	return;	
}


# Add a flag to $_use_temp if it does not exist
# Parameter 1: flag
# Parameter 2: Keyword "global" or "category/package"
sub _add_temp
{
	my ($flag, $pkg) = @_;
	my $isAdded = 0;
	
	(defined($flag) && length($flag))
		or return;

	if ("global" eq $pkg) {
		defined ($_use_temp->{$flag}{global})
			or %{$_use_temp->{$flag}{global}} = %$_use_template;
	} else {
		_add_temp($flag, "global"); ## This must exist!
		defined ($_use_temp->{$flag}{"local"}{$pkg})
			or %{$_use_temp->{$flag}{"local"}{$pkg}} = %$_use_template;
	}

	return;
}


# Determine the value for EPREFIX and save it
# in $EPREFIX. This is done using 'portageq'.
# Other output from portageq is printed on
# STDERR.
# No parameters accepted.
sub _determine_eprefix {
	my $tmp = "/tmp/ufed_$$.tmp";
	$EPREFIX = qx{portageq envvar EPREFIX 2>$tmp};
	die "Couldn't determine EPREFIX from Portage" if $? != 0;

	if ( -s $tmp ) {
		if (open (my $fTmp, "<", $tmp)) {
			print STDERR "$_" while (<$fTmp>);
			close $fTmp;
		}
	}
	-e $tmp and unlink $tmp;

	chomp($EPREFIX);
	return;
}


# determine which make.conf to use
# and save the result in $used_make_conf
sub _determine_make_conf
{
	$used_make_conf = "${EPREFIX}/etc/portage/make.conf";
	(! -r $used_make_conf)
		and $used_make_conf = "${EPREFIX}/etc/make.conf";
	return; 
}


# read /etc/make.profile and /etc/portage/make.profile and
# analyze the complete profile tree using the found parent
# files. Add all found paths to @profiles.
# No parameters accepted.
sub _determine_profiles
{
	my $mp = readlink "${EPREFIX}/etc/portage/make.profile";
	defined($mp)
		or $mp = readlink "${EPREFIX}/etc/make.profile";

	# make.profile is mandatory and must be a link
	defined($mp)
		or die "${EPREFIX}/etc\{,/portage\}/make.profile is not a symlink\n";

	# Start with the linked path, it is the deepest profile child.
	@_profiles = _norm_path('/etc', $mp);
	for (my $i = -1; $i >= -@_profiles; $i--) {
		for(_noncomments("${_profiles[$i]}/parent")) {
			splice(@_profiles, $i, 0, _norm_path(${_profiles[$i]}, $_));
		}
	}
	return;
}


# This method does a final cleanup of $_use_temp
# Everything that is to be done _after_ all
# configs are parsed goes in here.
# No parameters accepted
sub _final_cleaning
{
	# The "disable all" flag is truncated to '*' by the parsing, but it
	# has to read '-*'.
	_add_temp("-*", "global");

	$_use_temp->{'-*'}{global}{descr} = "{Never enable any flags other than those specified in make.conf}";
	$_use_temp->{'-*'}{global}{conf} = 0; ## Can never be -1
	
	# Set it from the truncated config:
	if (defined($_use_temp->{'*'}{global})) {
		$_use_temp->{'*'}{global}{conf} > 0
			and $_use_temp->{'-*'}{global}{conf} = 1;
	}
	
	# The following use flags are dangerous or internal only
	# and must no be available using ufed:
	defined($_use_temp->{"*"})         and delete($_use_temp->{"*"});
	defined($_use_temp->{"bootstrap"}) and delete($_use_temp->{"bootstrap"});
	defined($_use_temp->{"build"})     and delete($_use_temp->{"build"});
	defined($_use_temp->{"livecd"})    and delete($_use_temp->{"livecd"});
	defined($_use_temp->{"selinux"})   and delete($_use_temp->{"selinux"});

	return;
}


# Once $_use_temp  is ready, this method builds
# the final $use_flags hashref.
# No parameters accepted
sub _gen_use_flags
{
	for my $flag (keys %$_use_temp) {
		my %descCons = ();
		my $flagRef  = $_use_temp->{$flag}; ## Shortcut
		my $hasGlobal= defined($flagRef->{global}) ? 1 : 0;
		my $lCount   = ($hasGlobal && length($flagRef->{global}{descr})) ? 1 : 0;
		my $gDesc    = "";
		my $gKey     = "";
		my $gRef     = $flagRef->{global};
		my $pDesc    = "";
		my $pKey     = "";
		my $pRef     = undef;
		
		# Build the description consolidation hash
		if ($hasGlobal) {
			$gDesc = $gRef->{descr};
			$gKey  = sprintf("[%s]%d:%d:%d:%d:%d:%d", $gDesc, $gRef->{conf}, $gRef->{"default"},
							$gRef->{forced}, $gRef->{installed}, $gRef->{masked}, $gRef->{"package"});
			$descCons{$gKey}{global} = 1;
		}
		for my $pkg (sort keys %{$flagRef->{"local"}}) {
			$pRef  = $flagRef->{"local"}{$pkg};

			if (length($pRef->{descr})) {
				## This package has an individual description:
				$pDesc = "$pRef->{descr}";
				$pKey  = sprintf("[%s]%d:%d:%d:%d:%d:%d", $pDesc, $pRef->{conf}, $pRef->{"default"},
								$pRef->{forced}, $pRef->{installed}, $pRef->{masked}, $pRef->{"package"});
				$descCons{$pKey}{$pkg} = 1;
				++$lCount;
			}
			## TODO : Add affected packages that have no own description
			# once the interface can handle them. These can be used for
			# the package filtering per command line arguments later.
		} ## End of walking through a flags package list
		
		# Skip if there was nothing consolidated
		next unless($lCount);
		
		# Add the content of $descCons to $use_flags:
		$use_flags->{$flag}{count} = 0;
		
		# The global data has to be added first:
		if (length($gKey)) {
			_add_flag($flag, "global", $gKey);
			
			# TODO : Add affected packages once they can be hanlded. See above TODO.
		}
		
		# Then the "local" flag descriptions
		# TODO : Add affected packages that have diffeent settings once they can be handled.
		for my $key (sort keys %descCons) {
			next if ($key eq $gKey);

			# Generate the package list with same description and flags,
			# but not for more than 5 packages
			my $packages = "";
			my $pkgCount = 0;
			for my $pkg (sort keys %{$descCons{$key}}) {
				length($packages) and $packages .= ", ";
				$packages .= $pkg;
				if (++$pkgCount == 5) {
					_add_flag($flag, $packages, $key);
					$packages = "";
					$pkgCount = 0;
				}
			}
			
			# Add the last result if there is something left
			$pkgCount and _add_flag($flag, $packages, $key);
		}

		delete $_use_temp->{$flag};
	} ## End of walking through $_use_temp flags

	return;
}


# merges two hashes into the first.
# Parameter 1: reference of the destination hash
# Parameter 2: reference of the source hash
sub _merge {
	my ($dst, $src) = @_;
	%{$dst} = () if(exists $src->{'*'});
	$dst->{$_} = $src->{$_} for(keys %$src);
	return;
}


# Splits content of the source hash at spaces and
# merges its contents into %_environment.
# Parameter 1: reference of the hash to merge
sub _merge_env {
	my ($src) = @_;
	for(keys %_environment) {
		if(ref $_environment{$_} eq 'HASH') {
			if(exists $src->{$_}) {
				my %split;
				for(split ' ', $src->{$_}) {
					my $off = s/^-//;
					%split = () if($_ eq '*');
					$split{$_} = !$off;
				}
				$src->{$_} = { %split };
				_merge(\%{$_environment{$_}}, \%{$src->{$_}});
			}
		}
	}
	for(keys %$src) {
		if(ref $_environment{$_} ne 'HASH') {
			$_environment{$_} = $src->{$_};
		}
	}
	return;
}


# returns a list of all lines of a given file
# that are no pure comments
# Parameter 1: filename
sub _noncomments {
	my ($fname) = @_;
	my @result;
	local $/;
	if(open my $file, '<', $fname) {
		binmode( $file, ":encoding(UTF-8)" );
		@result = split /(?:[^\S\n]*(?:#.*)?\n)+/, <$file>."\n";
		shift @result if @result>0 && $result[0] eq '';
		close $file;
	}
	return @result;
}


# normalizes a given path behind a base
# Parameter 1: base path 
# Parameter 2: sub path
# return: normalized base path / normalized sub path
sub _norm_path {
	my ($base, $path) = @_;
	my @pathcomp = ($path !~ m!^/! && split(m!/!, $base), split(m!/!, $path));
	for(my $i=0;;$i++) {
		last if $i == @pathcomp; # don't want to skip this with redo
		if($pathcomp[$i] eq '' || $pathcomp[$i] eq '.') {
			splice @pathcomp, $i, 1;
			redo;
		}
		if($pathcomp[$i] eq '..') {
			if($i==0) {
				splice @pathcomp, 0, 1;
			} else {
				splice @pathcomp, --$i, 2;
			}
			redo;
		}
	}
	return '/'.join '/', @pathcomp;
}


# reads all found arch.list and erase all found archs
# from $_use_temp. Archs are not setable.
# No parameters accepted
sub _read_archs {
	for my $dir(@_profiles) {
		next unless (-r "$dir/arch.list");
		for my $arch (_noncomments("$dir/arch.list")) {
			defined($_use_temp->{$arch})
				and delete($_use_temp->{$arch});
		}
	}
	return;
}


# reads all use.desc and use.local.desc and updates
# $_use_temp accordingly.
# No parameters accepted
sub _read_descriptions
{
	for my $dir(@_profiles) {
		if (-r "$dir/use.desc") {
			for(_noncomments("$dir/use.desc")) {
				my ($flag, $desc) = /^(.*?)\s+-\s+(.*)$/ or next;
				
				_add_temp($flag, "global");

				$_use_temp->{$flag}{global}{descr} = $desc;
			}
		} ## End of having a use.desc file

		if (-r "$dir/use.local.desc") {
			for(_noncomments("$dir/use.local.desc")) {
				my ($pkg, $flag, $desc) = /^(.*?):(.*?)\s+-\s+(.*)$/ or next;

				# Here we do not explicitly add a {global} part,
				# some flags are local only.
				_add_temp($flag, $pkg);
				
				$_use_temp->{$flag}{"local"}{$pkg}{descr} = $desc;
			}
		} ## End of having a use.local.desc file
	} ## End of looping the profiles
	return;
}


# read make.conf and record the state of all set use
# flags.
# Additionally add all set portage directories (plus
# overlays) to @_profiles.
# The last added profile directory, if it exists, is
# /etc/portage/profile to allow recognition of user
# overrides.
# No parameters accepted.
sub _read_make_conf {
	my %oldEnv = _read_sh("${EPREFIX}/etc/make.conf");
	my %newEnv = _read_sh("${EPREFIX}/etc/portage/make.conf");
	_merge (\%oldEnv, \%newEnv);

	# Note the conf state of the read flags:
	for my $flag ( keys %{$oldEnv{USE}}) {
		_add_temp($flag, "global");

		$oldEnv{USE}{$flag}
			and $_use_temp->{$flag}{global}{conf} = 1
			or  $_use_temp->{$flag}{global}{conf} = -1;
	}
	
	# Add PORTDIR and overlays to @_profiles
	defined ($_environment{PORTDIR})
		and push @_profiles, "$_environment{PORTDIR}/profiles"
		or  die("Unable to determine PORTDIR!\nSomething is seriously broken here!\n");
	defined ($_environment{PORTDIR_OVERLAY})
		and push @_profiles,
				map { my $x=$_; $x =~ s/^\s*(\S+)\s*$/$1\/profiles/mg ; $x }
				split('\n', $_environment{PORTDIR_OVERLAY});
	-e "${EPREFIX}/etc/portage/profile"
		and push @_profiles, "${EPREFIX}/etc/portage/profile";
	return;
}


# read all found make.defaults and non-user package.use files
# and merge their values into env.
# TODO : use USE_EXPAND to add Expansion parsing. The most
#        important of these are set with sane defaults here,
#        too.
# No parameters accepted.
sub _read_make_defaults {
	for my $dir(@_profiles) {
		if (-r "$dir/make.defaults") {
			my %env = _read_sh("$dir/make.defaults");
	
			# Note the conf state of the read flags:
			for my $flag ( keys %{$env{USE}}) {
				_add_temp($flag, "global");

				$env{USE}{$flag}
					and $_use_temp->{$flag}{global}{"default"} = 1
					or  $_use_temp->{$flag}{global}{"default"} = -1;
			}
		}
	} ## End of looping through the profiles
	return
}


# read all found make.globals and merge their
# settings into %environment. This is done to
# get the final "PORTDIR" and "USE_ORDER"
# No parameters accepted
sub _read_make_globals {
	for my $dir(@_profiles, "${EPREFIX}/usr/share/portage/config") {
		_read_sh("$dir/make.globals");
	}
	return;
}


# Analyze EPREFIX/var/db/pkg and analyze all installed
# packages. We need to know what they use (IUSE) and what
# has been set when they where emerged (PKGUSE).
# No parameters accepted.
sub _read_packages {
	my $pkgdir = undef;
	opendir($pkgdir, "${EPREFIX}/var/db/pkg")
		or die "Couldn't read ${EPREFIX}/var/db/pkg\n";
		
	# loop through all categories in pkgdir
	while(my $cat = readdir $pkgdir) {
		next if $cat eq '.' or $cat eq '..';
		my $catdir = undef;
		opendir($catdir, "${EPREFIX}/var/db/pkg/$cat")
			or next;

		# loop through all openable directories in cat
		while(my $pkg = readdir $catdir) {
			next if $pkg eq '.' or $pkg eq '..';
			my @puse = ();
			my @iuse = ();
			
			# Load PROVIDE
			#Update: Deprecated, this file is gone
			#if(open my $provide, '<', "$eprefix/var/db/pkg/$cat/$pkg/PROVIDE") {
			#	local $/;
			#	@provide = split ' ', <$provide>;
			#	close $provide;
			#}
			
			# Load USE
			# Update: deprecated, this file is no longer useful. read IUSE and PKGUSE instead
			#if(open my $use, '<', "$eprefix/var/db/pkg/$cat/$pkg/USE") {
			#	local $/;
			#	@use = split ' ', <$use>;
			#	close $use;
			#}
			
			# Load IUSE to learn which use flags the package in this version knows
			my $fiuse = "${EPREFIX}/var/db/pkg/$cat/$pkg/IUSE";
			if(open my $use, '<', $fiuse) {
				local $/;
				@iuse = split ' ', <$use>;
				close $use;
			}

			# Load PKGUSE to learn which use flags have been set when this package was emerged
			my $fpuse = "${EPREFIX}/var/db/pkg/$cat/$pkg/PKGUSE";
			if(open my $use, '<', $fpuse) {
				local $/;
				@puse = split ' ', <$use>;
				close $use;
			}

			# could be shortened, but make sure not to strip off part of the name
			$pkg =~ s/-\d+(?:\.\d+)*\w?(?:_(?:alpha|beta|pre|rc|p)\d*)?(?:-r\d+)?$//;
			$pkg = $cat . "/" . $pkg;

			# Now save the knowledge gained (if any) in $_use_temp:
			for my $flag (@iuse) {
				my $eState = $flag =~ s/^\+// || 0; 
				my $dState = $flag =~ s/^-// || 0; 

				_add_temp($flag, "global");
				_add_temp($flag, $pkg);

				$_use_temp->{$flag}{"local"}{$pkg}{"package"}   = $eState ? 1 : $dState ? -1 : 0;
				$_use_temp->{$flag}{"local"}{$pkg}{installed}   = 1;
				$_use_temp->{$flag}{global}{installed} = 1;
			} ## End of looping IUSE
			for my $flag (@puse) {
				my $state = $flag =~ s/^-// || 0; 

				if ( defined($_use_temp->{$flag}{global})
				  && defined($_use_temp->{$flag}{$pkg})) {
				  	$state and $_use_temp->{$flag}{"local"}{$pkg}{"package"} = -1
				  	        or $_use_temp->{$flag}{"local"}{$pkg}{"package"} =  0;

				} # enable if output is wanted!
				#else {
				#	## This can happen if a package was installed with a flag
				#	## that is gone. (Seen with sys-fs/ntfs3g-2012.1.15-r1 to
				#	## sys-fs/ntfs3g-2012.1.15-r2 and use flag "extras". Gone,
				#	## but still listed in PKGUSE)
				#	printf STDERR "DEBUG: '%s' found in\n    '%s'\n  but not in\n    '%s'\n",
				#		$flag, $fpuse, $fiuse;
				#	## No need to break, though...
				#}
			} ## End of looping PKGUSE
			
		}
		closedir $catdir;
	}
	closedir $pkgdir;
	return;
}


# reads the given file and parses it for key=value pairs.
# "source" entries are added to the file and parsed as
# well. The results of the parsing are merged into
# %environment.
# Parameter 1: The path of the file to parse.
# In a non-scalar context the function returns the found values.
sub _read_sh {
	my ($fname) = @_;
	my $BLANK = qr{(?:[ \n\t]+|#.*)+};         # whitespace and comments
	my $IDENT = qr{([^ \\\n\t'"{}=#]+)};       # identifiers
	my $ASSIG = qr{=};                         # assignment operator
	my $UQVAL = qr{((?:[^ \\\n\t'"#]+|\\.)+)}s;# unquoted value
	my $SQVAL = qr{'([^']*)'};                 # singlequoted value
	my $DQVAL = qr{"((?:[^\\"]|\\.)*)"}s;      # doublequoted value

	my %env;
	if(open my $file, '<', $fname) {
		{ local $/; $_ = <$file> }
		close $file;
		eval {
			for(;;) {
				/\G$BLANK/gc;
				last if ((pos || 0) == (length || 0));
				/\G$IDENT/gc or die;
				my $name = $1;
				/\G$BLANK/gc;
				if($name ne 'source') {
				/\G$ASSIG/gc or die;
				/\G$BLANK/gc;
				}
				die if pos == length;
				my $value = '';
				for(;;) {
					if(/\G$UQVAL/gc || /\G$DQVAL/gc) {
						my $addvalue = $1;
						$addvalue =~ s[
							\\\n       | # backslash-newline
							\\(.)      | # other escaped characters
							\$({)?       # $
							$IDENT       # followed by an identifier
							(?(2)})      # optionally enclosed in braces
						][
							defined $3 ? $env{$3} || '' : # replace envvars
							defined $1 ? $1             : # unescape escaped characters
							             ''               # delete backslash-newlines
						]gex;
						$value .= $addvalue
					} elsif(/\G$SQVAL/gc) {
						$value .= $1
					} else {
						last
					}
				}
				if($name eq 'source') {
					open my $f, '<', $value or die "Unable to open $value\n$!\n";
					my $pos = pos;
					substr($_, pos, 0) = do {
						local $/;
						my $text = <$f>;
						die if not defined $text;
						$text;
					};
					pos = $pos;
					close $f or die "Unable to open $value\n$!\n";
				} else {
				$env{$name} = $value;
				}
			}
		};
		die "Parse error in $fname\n" if $@;
	}
	_merge_env(\%env);
	return %env if wantarray;
	return;
}


# read all enforced flags from all found use.force
# and package.use.force files. Save the found
# masks in %use_flags.
# No parameters accepted.
sub _read_use_force {
	for my $dir(@_profiles) {
		if (-r "$dir/use.force") {
			# use.force can enforce and mask specific flags
			for my $flag (_noncomments("$dir/use.force") ) {
				my $state = $flag =~ s/^-// || 0;
				
				_add_temp($flag, "global");
	
				$_use_temp->{$flag}{global}{masked} = !$state;
				$_use_temp->{$flag}{global}{forced} = !$state;
			}
		} ## End of having a use.force file
		
		if (-r "$dir/package.use.force") {
			# package.use.force can enforce or unforce flags per package
			for(_noncomments("$dir/package.use.force") ) {
				my($pkg, @flags) = split;
				for my $flag (@flags) {
					my $state = $flag =~ s/^-// || 0;
	
					_add_temp($flag, "global");
					_add_temp($flag, $pkg);
	
					if ($state) {
						$_use_temp->{$flag}{"local"}{$pkg}{masked} = -1; ## explicitly unmasked and
						$_use_temp->{$flag}{"local"}{$pkg}{forced} = -1; ## explicitly unforced
					} else {
						$_use_temp->{$flag}{"local"}{$pkg}{masked} =  1; ## explicitly masked and
						$_use_temp->{$flag}{"local"}{$pkg}{forced} =  1; ## explicitly enforced
					}
				}
			}
		} ## End of having a package.use.force file
	} ## End of looping through the profiles
	return;
}


# read all masked flags from all found use.mask
# and package.use.mask files. Save the found
# masks in %use_flags.
# No parameters accepted.
sub _read_use_mask {
	for my $dir(@_profiles) {
		if (-r "$dir/use.mask") {
			# use.mask can enable or disable masks
			for my $flag (_noncomments("$dir/use.mask") ) {
				my $state = $flag =~ s/^-// || 0;

				_add_temp($flag, "global");
	
				$_use_temp->{$flag}{global}{masked} = !$state;
			}
		} ## End of having a use.mask file
		
		if (-r "$dir/package.use.mask") {
		# package.use.mask can enable or disable masks per package
			for(_noncomments("$dir/package.use.mask") ) {
				my($pkg, @flags) = split;
				for my $flag (@flags) {
					my $state = $flag =~ s/^-// || 0;
	
					_add_temp($flag, "global");
					_add_temp($flag, $pkg);
	
					$state and $_use_temp->{$flag}{"local"}{$pkg}{masked} = -1; ## explicitly unmasked
					$state  or $_use_temp->{$flag}{"local"}{$pkg}{masked} =  1; ## explicitly masked
				}
			}
		} ## End of having a package.use.mask file
	} ## End of looping through the profiles
	return;
}

1;
