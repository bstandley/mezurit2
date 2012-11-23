#!/usr/bin/perl

#########################  notes  #########################

# Version from 0.56 can print out a whole tree for debugging. This verion cannot.

#######################  defaults  ########################

$inc = 'src';

################  command line arguments  #################

$opt = shift @ARGV;
foreach(@ARGV) { add(\@args, $_); }  # skip duplicates

#####################  main program  ######################

if($opt eq '--objects')
{
	if($opt eq '--tree') { $tree = 1; }

	@done_list = ();
	@obj_list = ();
	foreach(@args)
	{
		print STDERR "\nSearching $_ for deps: \n";

		mention(add(\@obj_list, ext($_, 'o')));
		search($_);
	}
	print "@obj_list\n";
}
elsif($opt eq '--list') { print "@args\n"; }
elsif($opt eq '--targets')
{
	foreach(@args)
	{
		my $cf = ext($_, 'c');
		my @x = split ': ', `gcc -I$inc -MM $cf`;
		print "$_ : $x[1]";
	}
}

#######################  functions  #######################

sub ext
{
	my ($file, $extension) = @_;
	my @x = split(/\./, $file);
	pop @x;
	return join('.', @x) . '.' . $extension;
}

sub add
{
	my ($list, $file) = @_;  # pass array reference for $list

	foreach(@$list) { if($file eq $_) { return ''; } }
	push @$list, $file;

	return $file;
}

sub mention
{
	my ($file) = @_;
	if($file ne '') { print STDERR "  $file\n"; }
}

sub check
{
	my ($file, $parent) = @_;

	if(-e $file)
	{
		my $c_file = ext($file, 'c');
		if(($file eq ext($file, 'h')) && (-e $c_file))  # header file with an associated .c file
		{
			mention(add(\@obj_list, ext($c_file, 'o')));
			if(!($c_file eq $parent)) { search($c_file); }
		}

		search($file);
	}
}

sub search
{
	my ($file) = @_;

	if(add(\@done_list, $file) eq '') { return; }  # speed things up by skipping previously searched files

	my @x = split '/', $file;
	pop @x;
	my $cur = join '/', @x;

	open my $input, "< $file" || die "Cannot open \"$file\".\n";
	while($line = <$input>)
	{
		if   ($line =~ /#include <(.*)>/) { check("$inc/$1", $file); }
		elsif($line =~ /#include "(.*)"/) { check("$cur/$1", $file); }
	}
	close $input;
}
