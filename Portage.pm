package Portage;

# Copyright 1999-2005 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-src/ufed/Portage.pm,v 1.3 2005/11/13 00:28:17 truedfx Exp $

use strict;
use warnings;

my %environment;
$environment{$_}={} for qw(USE); # INCREMENTALS, except we only need USE

our @portagedirs;
our %packages;
our @profiles;
our %use_masked_flags;
our %make_defaults_flags;
our %default_flags;
our %make_conf_flags;
our %archs;
our %all_flags;
our $eprefix;

sub get_eprefix;
sub have_package;
sub merge;
sub merge_env;
sub noncomments;
sub norm_path;
sub read_archs;
sub read_make_conf;
sub read_make_defaults;
sub read_make_globals;
sub read_packages;
sub read_profiles;
sub read_sh;
sub read_use_mask;

get_eprefix;
read_packages;
read_profiles;
read_use_mask;
read_make_globals;
read_make_defaults;
read_make_conf;
read_archs;

my $lastorder;
for(reverse split /:/, $environment{USE_ORDER} || "env:pkg:conf:defaults:pkginternal:env.d") {
	if($_ eq 'defaults') {
		merge(\%default_flags, \%make_defaults_flags);
		merge(\%all_flags, \%make_defaults_flags);
	} elsif($_ eq 'conf') {
		merge(\%all_flags, \%make_conf_flags);
	} else {
		next;
	}
	$lastorder = $_;
}
if($lastorder ne 'conf') {
	die "Sorry, USE_ORDER without make.conf overriding global USE flags are not currently supported by ufed.\n";
}


# Determine the value for EPREFIX and save it
# in $eprefix. This is done using 'portageq'.
# Other output from portageq is printed on
# STDERR.
# No parameters accepted.
sub get_eprefix {
	my $tmp = "/tmp/ufed_$$.tmp";
	$eprefix = qx{portageq envvar EPREFIX 2>$tmp};
	die "Couldn't determine EPREFIX from Portage" if $? != 0;

	if ( -s $tmp ) {
		if (open (my $fTmp, "<", $tmp)) {
			print STDERR "$_" while (<$fTmp>);
			close $fTmp;
		}
	}
	-e $tmp and unlink $tmp;

	chomp($eprefix);
	return;
}


# returns the content of %packages for the given
# scalar or undef.
# Parameter 1: Package to test of the form <category>/<name>
sub have_package {
	my ($cp) = @_;
	return $packages{$cp};
}


# merges two hashes into the first.
# Parameter 1: reference of the destination hash
# Parameter 2: reference of the source hash
sub merge {
	my ($dst, $src) = @_;
	%{$dst} = () if(exists $src->{'*'});
	$dst->{$_} = $src->{$_} for(keys %$src);
	return;
}


# Splits content of the source hash at spaces and
# merges its contents into %environment.
# Parameter 1: reference of the hash to merge
sub merge_env {
	my ($src) = @_;
	for(keys %environment) {
		if(ref $environment{$_} eq 'HASH') {
			if(exists $src->{$_}) {
				my %split;
				for(split ' ', $src->{$_}) {
					my $off = s/^-//;
					%split = () if($_ eq '*');
					$split{$_} = !$off;
				}
				$src->{$_} = { %split };
				merge(\%{$environment{$_}}, \%{$src->{$_}});
			}
		}
	}
	for(keys %$src) {
		if(ref $environment{$_} ne 'HASH') {
			$environment{$_} = $src->{$_};
		}
	}
	return;
}


# returns a list of all lines of a given file
# that are no pure comments
# Parameter 1: filename
sub noncomments {
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
sub norm_path {
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


# reads all arch.list in all profiles sub directories
# of found @portagedirs and saves the found architectures
# in %arch
# No parameters accepted
sub read_archs {
	for my $dir(@portagedirs) {
		for(noncomments "$dir/profiles/arch.list") {
			$archs{$_} = 1;
		}
	}
	return;
}


# read /etc/make.conf and/or /etc/portage/make.conf and
# merge set USE flags into %make_conf_flags. Additionally
# all set portage directories (plus overlays) are recorded
# in @portagedirs.
# No parameters accepted.
sub read_make_conf {
	my %oldEnv = read_sh("$eprefix/etc/make.conf");
	my %newEnv = read_sh("$eprefix/etc/portage/make.conf");
	merge (\%oldEnv, \%newEnv);
	merge (\%make_conf_flags,\ %{$oldEnv{USE}}) if exists $oldEnv{USE};
	@portagedirs = $environment{PORTDIR};
	push @portagedirs, split ' ', $environment{PORTDIR_OVERLAY} if defined $environment{PORTDIR_OVERLAY};
	return;
}


# read all found make.defaults files and merge their flags
# into %make_default_flags.
# No parameters accepted.
sub read_make_defaults {
	for my $dir(@profiles) {
		my %env = read_sh "$dir/make.defaults";
		merge (\%make_defaults_flags, \%{$env{USE}}) if exists $env{USE};
	}
	return
}


# read all found make.globals and merge their
# settings into %environment.
# No parameters accepted
sub read_make_globals {
	for my $dir(@profiles, "$eprefix/usr/share/portage/config") {
		read_sh "$dir/make.globals";
	}
	return;
}


# Analyze EPREFIX/var/db/pkg and note all installed
# packages in %packages.
# No parameters accepted.
sub read_packages {
	die "Couldn't read $eprefix/var/db/pkg\n" unless opendir my $pkgdir, "$eprefix/var/db/pkg";
	while(my $cat = readdir $pkgdir) {
		next if $cat eq '.' or $cat eq '..';
		next unless opendir my $catdir, "$eprefix/var/db/pkg/$cat";
		while(my $pkg = readdir $catdir) {
			next if $pkg eq '.' or $pkg eq '..';
			my @provide = ();
			my @use = ();
			
			# Load PROVIDE
			## FIXME: There is no file "PROVIDE" anywhere, at least on my system!
			if(open my $provide, '<', "$eprefix/var/db/pkg/$cat/$pkg/PROVIDE") {
				local $/;
				@provide = split ' ', <$provide>;
				close $provide;
			}
			
			# Load USE
			if(open my $use, '<', "$eprefix/var/db/pkg/$cat/$pkg/USE") {
				local $/;
				@use = split ' ', <$use>;
				close $use;
			}

			# could be shortened, but make sure not to strip off part of the name
			$pkg =~ s/-\d+(?:\.\d+)*\w?(?:_(?:alpha|beta|pre|rc|p)\d*)?(?:-r\d+)?$//;
			$packages{"$cat/$pkg"} = 1;
			
			# FIXME: What is this supposed to achieve?
			# Currently this does nothing as there is no PROVIDE anywhere,
			# but even if it were, there is nothing really done at all with
			# @use.
			for(my $i=0; $i<@provide; $i++) {
				my $pkg = $provide[$i];
				next if $pkg eq '(' || $pkg eq ')';
				if($pkg !~ s/\?$//) {
					$pkg =~ s/-\d+(?:\.\d+)*\w?(?:_(?:alpha|beta|pre|rc|p)\d*)?(?:-r\d+)?$//;
					$packages{$pkg} = 1;
				} else {
					my $musthave = $pkg !~ s/^!//;
					my $have = 0;
					for(@use) {
						if($pkg eq $_)
						{ $have = 1; last }
					}
					if($musthave != $have) {
						my $level = 0;
						for($i++;$i<@provide;$i++) {
							$level++ if $provide[$i] eq '(';
							$level-- if $provide[$i] eq ')';
							last if $level==0;
						}
					}
				}
			}
		}
		closedir $catdir;
	}
	closedir $pkgdir;
	return;
}


# read /etc/make.profile and /etc/portage/make.profile
# and analyze the complete profile tree using the found
# parent files. Add all found paths to @profiles.
# No parameters accepted.
sub read_profiles {
	$_ = readlink "$eprefix/etc/make.profile";
	$_ = readlink "$eprefix/etc/portage/make.profile" if not defined $_;
	die "$eprefix/etc\{,/portage\}/make.profile is not a symlink\n" if not defined $_;
	@profiles = norm_path '/etc', $_;
	for (my $i = -1; $i >= -@profiles; $i--) {
		for(noncomments "$profiles[$i]/parent") {
			splice @profiles, $i, 0, norm_path $profiles[$i], $_;
		}
	}
	push @profiles, "$eprefix/etc/portage/profile";
	return;
}


# reads the given file and parses it for key=value pairs.
# "source" entries are added to the file and parsed as
# well. The results of the parsing are merged into
# %environment.
# Parameter 1: The path of the file to parse.
# In a non-scalar context the function returns the found values.
sub read_sh {
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
				last if pos == length;
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
	merge_env(\%env);
	return %env if wantarray;
	return;
}


# read all masked flags from all found use.mask
# and package.use.mask files. Save the found
# masks in %use_masked_flags.
# No parameters accepted.
sub read_use_mask {
	for my $dir(@profiles) {
		-r "$dir/use.mask" or next;
		for(noncomments "$dir/use.mask") {
			my $off = s/^-//;
			$use_masked_flags{$_} = { '' => !$off };
		}
		for(noncomments "$dir/package.use.mask") {
			my($pkg, @flags) = split;
			for(@flags) {
				my $off = s/^-//;

				$use_masked_flags{$_}{''} ||= 0;
				$use_masked_flags{$_}{$pkg} = !$off;
			}
		}
	}
	return;
}

1;
