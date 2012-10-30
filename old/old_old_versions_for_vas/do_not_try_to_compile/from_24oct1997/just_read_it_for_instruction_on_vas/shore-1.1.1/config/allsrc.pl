#!/s/std/bin/perl -w
# Use this little script to generate files allconf, allimake, allmake, and
# allsrc.  See also the shell file setup.
use strict 'subs';

%suf = (
	"C" => 1,
	"c" => 1,
	"cc" => 1,
	"h" => 1,
	"hh" => 1,
	"i" => 1,
	"l" => 1,
	"x" => 1,
	"y" => 1,
);

chomp($cwd = `pwd`);
$src = $cwd;
die unless $src =~ s/config$/src/;

die "allconf: $!\n" unless open(OUT,">allconf");
foreach (<Imake.*>) {
	print OUT "$cwd/$_\n";
}
print OUT "$cwd/config.h\n";

die "allimake: $!\n" unless open(IMAKE,">allimake");
die "allmake: $!\n" unless open(MAKE,">allmake");
die "allsrc: $!\n" unless open(SRC,">allsrc");
do_dir("src");
do_dir("examples");
do_dir("installed");
sub do_dir {
	local ($suf) = @_;
	local $dir = "../$suf";
	local $pref = $src;
	$pref =~ s/src$/$suf/;
	return unless -d $dir;
	print STDERR "do_dir '$dir'\n";
	for $_ (`find $dir -print`) {
		chomp();
		print STDERR "$_\n" unless s|\Q$dir\E||;
		print SRC "$pref$_\n"
			if m|\.([^.]+)$| && $suf{$1};
		print IMAKE "$pref$_\n"
			if m|/Imakefile$|;
		print MAKE "$pref$_\n"
			if m|/Makefile$|;
	}
}
