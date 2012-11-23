#!/usr/bin/perl

#use Cwd;  # run from the win32 directory
#$target = cwd();

$target = shift @ARGV;

foreach(@ARGV)  # remaining args are files
{
	$f  = $_;
	$tf = $f . ".tmp";

	open my $in,  "< $f";
	open my $out, "> $tf";

	while($line = <$in>)
	{
		if($line =~ /^prefix=(.*)/ && $1 ne $target)
		{
			print "prefix fixed in $f\n";
			$line = "prefix=$target\n";
		}

		print $out $line;
	}

	close $in;
	close $out;

	system "mv $tf $f";
}
