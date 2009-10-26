#!/usr/bin/perl -I/usr/lib/ufed
use strict;
use warnings;

# Copyright 1999-2005 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-src/ufed/ufed.pl,v 1.45 2005/10/18 11:14:33 truedfx Exp $

use Portage;

my $version = '0.40';

my %use_descriptions;

sub finalise(@);
sub flags_dialog();
sub read_use_descs();
sub save_flags(@);

delete $Portage::all_flags{'*'};

read_use_descs;

delete @use_descriptions{qw(bootstrap build)};

$Portage::all_flags{'-*'} = 1 if defined $Portage::make_conf_flags{'*'} && !$Portage::make_conf_flags{'*'};

Portage::merge %Portage::use_masked_flags, %Portage::archs;

for(keys %Portage::all_flags) {
	@{$use_descriptions{$_}} = "(Unknown)"
	if not exists $use_descriptions{$_};
}
@{$use_descriptions{'-*'}} = 'Never enable any flags other than those specified in /etc/make.conf';

for(keys %Portage::use_masked_flags)
{ delete $use_descriptions{$_} if $Portage::use_masked_flags{$_} }

flags_dialog;

sub finalise(@) {
	my %flags;
	@flags{@_} = ();
	if(exists $flags{'-*'}) {
		return sort keys %flags;
	} else {
		my(@enabled, @disabled);
		my %all_flags;
		@all_flags{keys %flags, keys %Portage::default_flags} = ();
		for(sort keys %all_flags) {
			next if $_ eq '*';
			push @enabled,    $_  if exists $flags{$_} && !$Portage::default_flags{$_};
			push @disabled, "-$_" if $Portage::default_flags{$_} && !exists $flags{$_};
		}
		return @enabled, @disabled;
	}
}

sub flags_dialog() {
	use POSIX ();
	POSIX::dup2 1, 3;
	POSIX::dup2 1, 4;
	my ($iread, $iwrite) = POSIX::pipe;
	my ($oread, $owrite) = POSIX::pipe;
	my $child = fork;
	die "fork() failed\n" if not defined $child;
	if($child == 0) {
		POSIX::close $iwrite;
		POSIX::close $oread;
		POSIX::dup2 $iread, 3;
		POSIX::close $iread;
		POSIX::dup2 $owrite, 4;
		POSIX::close $owrite;
		my $interface = 'ufed-curses';
		exec { "/usr/lib/ufed/$interface" } $interface or
		do { print STDERR "Couldn't launch $interface\n"; exit 3 }
	}
	POSIX::close $iread;
	POSIX::close $owrite;
	if(open my $fh, '>&=', $iwrite) {
		my @flags = sort { uc $a cmp uc $b } keys %use_descriptions;
		my %descriptions;
		for(my $flag=0; $flag<@flags; $flag++) {
			my $flag = $flags[$flag];
			print $fh $flag, $Portage::all_flags{$flag} ? ' on ' : ' off ';
			print $fh exists $Portage::make_defaults_flags{$flag} ? $Portage::make_defaults_flags{$flag} ? '(+' :'(-' :'( ' ;
			print $fh exists  $Portage::use_defaults_flags{$flag} ?  $Portage::use_defaults_flags{$flag} ?  '+' : '-' : ' ' ;
			print $fh exists     $Portage::make_conf_flags{$flag} ?     $Portage::make_conf_flags{$flag} ?  '+)': '-)': ' )';
			print $fh ' ', scalar(@{$use_descriptions{$flag}}), "\n";
			print $fh $_, "\n" for(@{$use_descriptions{$flag}});
		}
		close $fh;
	} else {
		die "Couldn't let interface know of flags\n";
	}
	POSIX::close $iwrite;
	wait;
	open my $fh, '<&=', $oread or die "Couldn't read output.\n";
	if(POSIX::WIFEXITED($?)) {
		my $rc = POSIX::WEXITSTATUS($?);
		if($rc==0) {
			my @flags = do { local $/; split /\n/, <$fh> };
			save_flags finalise sort @flags;
		} elsif($rc==1)
		{ print "Cancelled, not saving changes.\n" }
		exit $rc;
	} elsif(POSIX::WIFSIGNALED($?))
	{ kill POSIX::WTERMSIG($?), $$ }
	else
	{ exit 127 }
}

sub read_use_descs() {
	my %_use_descriptions;
	for my $dir(@Portage::portagedirs) {
		for(Portage::noncomments "$dir/profiles/use.desc") {
			my ($flag, $desc) = /^(.*?)\s+-\s+(.*)$/ or next;
			$_use_descriptions{$flag}{$desc} = 1;
		}
	}
	my %_use_local_descriptions;
	for my $dir(@Portage::portagedirs) {
		for(Portage::noncomments "$dir/profiles/use.local.desc") {
			my ($pkg, $flag, $desc) = /^(.*?):(.*?)\s+-\s+(.*)$/ or next;
			$_use_local_descriptions{$flag}{$desc}{$pkg} = 1;
		}
	}
	local $"=", ";
	for(sort keys %_use_descriptions)
	{ @{$use_descriptions{$_}} = sort keys %{$_use_descriptions{$_}} }
	for(sort keys %_use_local_descriptions) {
		for my $desc(sort keys %{$_use_local_descriptions{$_}})
		{ push @{$use_descriptions{$_}}, "Local flag: $desc (@{[sort keys %{$_use_local_descriptions{$_}{$desc}}]})" }
	}
}

sub save_flags(@) {
	my $BLANK = qr{(?:[ \n\t]+|#.*)+};              # whitespace and comments
	my $UBLNK = qr{(?:                              # as above, but scan for #USE=
		[ \n\t]+ |
		\#[ \t]*USE[ \t]*=.*(\n?) | # place capture after USE=... line
		\#.*)+}x;
	my $IDENT = qr{([^ \\\n\t'"{}=]+)};             # identifiers
	my $ASSIG = qr{=};                              # assignment operator
	my $UQVAL = qr{(?:[^ \\\n\t'"]+|\\.)+}s;        # unquoted value
	my $SQVAL = qr{'[^']*'};                        # singlequoted value
	my $DQVAL = qr{"(?:[^\\"]|\\.)*"}s;             # doublequoted value
	my $BNUQV = qr{(?:[^ \\\n\t'"]+|\\\n()|\\.)+}s; # unquoted value (scan for \\\n)
	my $BNDQV = qr{"(?:[^\\"]|\\\n()|\\.)*"}s;      # doublequoted value (scan for \\\n)

	my (@flags) = @_;
	my $contents;

	{
		open my $makeconf, '<', '/etc/make.conf' or die "Couldn't open /etc/make.conf\n";
		open my $makeconfold, '>', '/etc/make.conf.old' or die "Couldn't open /etc/make.conf.old\n";
		local $/;
		$_ = <$makeconf>;
		print $makeconfold $_;
		close $makeconfold;
		close $makeconf;
	}

	eval {
		# USE comment start/end (start/end of newline character at the end, specifically)
		# default to end of make.conf, to handle make.confs without #USE=
		my($ucs, $uce) = (length, length);
		my $flags = '';
		pos = 0;
		for(;;) {
			if(/\G$UBLNK/gc) {
				($ucs, $uce) = ($-[1], $+[1]) if defined $1;
			}
			last if pos == length;
			my $flagatstartofline = do {
				my $linestart = 1+rindex $_, "\n", pos()-1;
				my $line = substr($_, $linestart, pos()-$linestart);
				$line !~ /[^ \t]/;
			};
			/\G$IDENT/gc or die;
			my $name = $1;
			/\G$BLANK/gc;
			/\G$ASSIG/gc or die;
			/\G$BLANK/gc;
			die if pos == length;
			if($name ne 'USE') {
				/\G(?:$UQVAL|$SQVAL|$DQVAL)+/gc or die;
			} else {
				my $start = pos;
				/\G(?:$BNUQV|$SQVAL|$BNDQV)+/gc or die;
				my $end = pos;
				# save whether user uses backslash-newline
				my $bsnl = defined $1 || defined $2;
				# start of the line is one past the last newline; also handles first line
				my $linestart = 1+rindex $_, "\n", $start-1;
				# everything on the current line before the USE flags, plus one for the "
				my $line = substr($_, $linestart, $start-$linestart).' ';
				# only indent if USE starts a line
				my $blank = $flagatstartofline ? $line : "";
				$blank =~ s/[^ \t]/ /g;
				# word wrap
				if(@flags != 0) {
					my $length = 0;
					while($line =~ /(.)/g) {
						if($1 ne "\t") {
							$length++;
						} else {
							# no best tab size discussions, please. terminals use ts=8.
							$length&=~8;
							$length+=8;
						}
					}
					my $blanklength = $blank ne '' ? $length : 0;
					# new line, using backslash-newline if the user did that
					my $nl = ($bsnl ? " \\\n" : "\n").$blank;
					my $linelength = $bsnl ? 76 : 78;
					my $flag = $flags[0];
					if($blanklength != 0 || length $flag <= $linelength) {
						$flags   = $flag;
						$length += length $flag;
					} else {
						$flags   = $nl.$flag;
						$length  = length $flag;
					}
					for $flag(@flags[1..$#flags]) {
						if($length + 1 + length $flag <= $linelength) {
							$flags  .= " $flag";
							$length += 1+length $flag;
						} else {
							$flags  .= $nl.$flag;
							$length  = $blanklength + length $flag;
						}
					}
				}
				# replace the current USE flags with the modified ones
				substr($_, $start, $end-$start) = "\"$flags\"";
				# and have the next search start after our new flags
				pos = $start + 2 + length $flags;
				# and end this
				undef $flags;
				last;
			}
		}
		if(defined $flags) { # if we didn't replace the flags, tack them after the last #USE= or at the end
			$flags = '';
			if(@flags != 0) {
				$flags = $flags[0];
				my $length = 5 + length $flags[0];
				for my $flag(@flags[1..$#flags]) {
					if($length + 1 + length $flag <= 78) {
						$flags  .= " $flag";
						$length += 1+length $flag;
					} else {
						$flags  .= "\n     $flag";
						$length  = 5+length $flag;
					}
				}
			}
			substr($_, $ucs, $uce-$ucs) = "\nUSE=\"$flags\"\n";
		} else { # if we replaced the flags, delete any further overrides
			for(;;) {
				my $start = pos;
				/\G$BLANK/gc;
				last if pos == length;
				/\G$IDENT/gc or die;
				my $name = $1;
				/\G$BLANK/gc;
				/\G$ASSIG/gc or die;
				/\G$BLANK/gc;
				/\G(?:$UQVAL|$SQVAL|$DQVAL)+/gc or die;
				my $end = pos;
				if($name eq 'USE') {
					substr($_, $start, $end-$start) = '';
					pos = $start;
				}
			}
		}
	};
	die "Parse error when writing make.conf - did you modify it while ufed was running?\n" if $@;

	{
		open my $makeconf, '>', '/etc/make.conf' or die "Couldn't open /etc/make.conf\n";
		print $makeconf $_;
		close $makeconf;
	}
}
