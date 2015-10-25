use strict;
use warnings;

use File::Slurp;
use File::Basename;
use File::Spec::Functions;

my $root = $ENV{'SASS_LIBSASS_PATH'} || catfile(dirname($0), '..');

sub process($)
{

	my $count = 0;
	my ($file) = @_;

	my $cpp = read_file($file, { binmode => ':raw' });

	my $org = $cpp;

	my $re_decl = qr/(?:const\s*)?\w+(?:\:\:\w+)*(?:\s*[\*\&])?/;
	my $re_val = qr/\w+(?:\(\))?(?:(?:->|\.)\w+(?:\(\))?)*/;

	$cpp =~ s/for\s*\(\s*($re_decl)\s*(\w+)\s*:\s*(\(\*?$re_val\)|$re_val)\s*\)\s*{/
		$count ++;
		"for (auto __$2 = $3.begin(); __$2 != $3.end(); ++__$2) { $1 $2 = *(__$2); ";
	/gex;

	return if $org eq $cpp || $count == 0;

	warn sprintf "made %02d replacements in %s\n", $count, $file;

	write_file($file, { binmode => ':raw' }, $cpp);

}

my $rv = opendir(my $dh, catfile($root, "src"));
die "not found ", catfile($root, "src") unless $rv;
while (my $entry = readdir($dh)) {
	next if $entry eq "." || $entry eq "..";
	next unless $entry =~ m/\.[hc]pp$/;
	process(catfile($root, "src", $entry));
}