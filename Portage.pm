package Portage;

# Copyright 1999-2005 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-src/ufed/Portage.pm,v 1.3 2005/11/13 00:28:17 truedfx Exp $

my %environment;
$environment{$_}={} for qw(USE); # INCREMENTALS, except we only need USE

our %packages;
our @profiles;
our %use_masked_flags;
our %make_defaults_flags;
our %default_flags;
our %make_conf_flags;
our %archs;
our %all_flags;

sub have_package($);
sub merge(\%%);
sub merge_env(\%);
sub noncomments($);
sub norm_path($$);
sub read_archs();
sub read_make_conf();
sub read_make_defaults();
sub read_make_globals();
sub read_packages();
sub read_profiles();
sub read_sh($);
sub read_use_mask();

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
		merge %default_flags, %make_defaults_flags;
		merge %all_flags, %make_defaults_flags;
	} elsif($_ eq 'conf') {
		merge %all_flags, %make_conf_flags;
	} else {
		next;
	}
	$lastorder = $_;
}
if($lastorder ne 'conf') {
	die "Sorry, USE_ORDER without make.conf overriding global USE flags are not currently supported by ufed.\n";
}

sub have_package($) {
	my ($cp) = @_;
	return $packages{$cp};
}

sub merge(\%%) {
	my ($env, %env) = @_;
	%{$env} = () if(exists $env{'*'});
	$env->{$_} = $env{$_} for(keys %env);
}

sub merge_env(\%) {
	my ($env) = @_;
	for(keys %environment) {
		if(ref $environment{$_} eq 'HASH') {
			if(exists $env->{$_}) {
				my %split;
				for(split ' ', $env->{$_}) {
					my $off = s/^-//;
					%split = () if($_ eq '*');
					$split{$_} = !$off;
				}
				$env->{$_} = { %split };
				merge %{$environment{$_}}, %{$env->{$_}};
			}
		}
	}
	for(keys %{$env}) {
		if(ref $environment{$_} ne 'HASH') {
			$environment{$_} = $env->{$_};
		}
	}
}

sub noncomments($) {
	my ($fname) = @_;
	my @result;
	local $/;
	if(open my $file, '<', $fname) {
		@result = split /(?:[^\S\n]*(?:#.*)?\n)+/, <$file>."\n";
		shift @result if @result>0 && $result[0] eq '';
		close $file;
	}
	return @result;
}

sub norm_path($$) {
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

sub read_archs() {
	for my $dir(@portagedirs) {
		for(noncomments "$dir/profiles/arch.list") {
			$archs{$_} = 1;
		}
	}
}

sub read_make_conf() {
	my %env = read_sh "/etc/make.conf";
	merge %make_conf_flags, %{$env{USE}} if exists $env{USE};
	@portagedirs = $environment{PORTDIR};
	push @portagedirs, split ' ', $environment{PORTDIR_OVERLAY} if defined $environment{PORTDIR_OVERLAY};
}

sub read_make_defaults() {
	for my $dir(@profiles) {
		my %env = read_sh "$dir/make.defaults";
		merge %make_defaults_flags, %{$env{USE}} if exists $env{USE};
	}
}

sub read_make_globals() {
	for my $dir(@profiles, '/etc') {
		read_sh "$dir/make.globals";
	}
}

sub read_packages() {
	die "Couldn't read /var/db/pkg\n" unless opendir my $pkgdir, '/var/db/pkg';
	while(my $cat = readdir $pkgdir) {
		next if $cat eq '.' or $cat eq '..';
		next unless opendir my $catdir, "/var/db/pkg/$cat";
		while(my $pkg = readdir $catdir) {
			next if $pkg eq '.' or $pkg eq '..';
			if(open my $provide, '<', "/var/db/pkg/$cat/$pkg/PROVIDE") {
				if(open my $use, '<', "/var/db/pkg/$cat/$pkg/USE") {
					# could be shortened, but make sure not to strip off part of the name
					$pkg =~ s/-\d+(?:\.\d+)*\w?(?:_(?:alpha|beta|pre|rc|p)\d*)?(?:-r\d+)?$//;
					$packages{"$cat/$pkg"} = 1;
					local $/;
					my @provide = split ' ', <$provide>;
					my @use = split ' ', <$use>;
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
					close $use;
				}
				close $provide;
			}
		}
		closedir $catdir;
	}
	closedir $pkgdir;
}

sub read_profiles() {
	$_ = readlink '/etc/make.profile';
	die "/etc/make.profile is not a symlink\n" if not defined $_;
	@profiles = norm_path '/etc', $_;
	for (my $i = -1; $i >= -@profiles; $i--) {
		for(noncomments "$profiles[$i]/parent") {
			splice @profiles, $i, 0, norm_path $profiles[$i], $_;
		}
	}
	push @profiles, '/etc/portage/profile';
}

sub read_sh($) {
	my $BLANK = qr{(?:[ \n\t]+|#.*)+};         # whitespace and comments
	my $IDENT = qr{([^ \\\n\t'"{}=#]+)};       # identifiers
	my $ASSIG = qr{=};                         # assignment operator
	my $UQVAL = qr{((?:[^ \\\n\t'"#]+|\\.)+)}s;# unquoted value
	my $SQVAL = qr{'([^']*)'};                 # singlequoted value
	my $DQVAL = qr{"((?:[^\\"]|\\.)*)"}s;      # doublequoted value

	my ($fname) = @_;
	my %env;
	if(open my $file, '<', $fname) {
		{ local $/; $_ = <$file> }
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
					open my $f, '<', $value or die;
					my $pos = pos;
					substr($_, pos, 0) = do {
						local $/;
						my $text = <$f>;
						die if not defined $text;
						$text;
					};
					pos = $pos;
					close $f or die;
				} else {
				$env{$name} = $value;
				}
			}
		};
		die "Parse error in $fname\n" if $@;
		close $file;
	}
	merge_env %env;
	return %env if wantarray;
}

sub read_use_mask() {
	for my $dir(@profiles) {
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
}

1;
