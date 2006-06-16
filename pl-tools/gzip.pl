#!/usr/bin/perl -w
#
# Copyright (c) 2005, 2006 Miek Gieben; Mark J Hewitt
# See LICENSE for the license
#
# zip rdup -c's output
#
use strict;

use Getopt::Std;
use File::Basename;
use File::Temp qw{tempdir tempfile};
use File::Spec;

my $S_ISDIR = 040000;
my $S_ISLNK = 0120000;

my $progName = basename $0;

my %opt;
my $zipOptions = "";

getopts('dhV', \%opt);
usage() if $opt{'h'};
version() if $opt{'V'};
$zipOptions .= "-d " if $opt{'d'};

my $tmpDir = tempdir("rdup.backup.XXXXXX", TMPDIR => 1, CLEANUP => 1);
die "$tmpDir could not be created: $!" unless -d $tmpDir;

while (($_ = <STDIN>))
{
    chomp;
    my($mode, $uid, $gid, $psize, $fsize) = split;
    my $dump = substr($mode, 0, 1);
    my $modebits = substr($mode, 1);
    my $typ = 0;

    sanity_check($dump, $modebits, $psize, $fsize, $uid, $gid);

    my $path = "";
    read STDIN, $path, $psize;
    $typ = 1 if ($mode & $S_ISDIR) == $S_ISDIR;
    $typ = 2 if ($mode & $S_ISLNK) == $S_ISLNK;
    if ($dump eq '+')
    {				# add
	if ($typ == 0)
	{			# REG
	    if ($fsize != 0)
	    {
		my($fh, $filename) = tempfile("file.XXXXX", DIR => $tmpDir, SUFFIX => ".gz" );
		$fh->close();
		open ZIP, "|gzip $zipOptions -c > $filename" or die "$filename: $!";
		copy($fsize, *ZIP);
		close ZIP or die "$filename: $!";
		my $size = (stat($filename))[7];
		syswrite STDOUT, "$dump$modebits $uid $gid $psize $size\n$path";
		catfile($filename);
		unlink $filename;
	    }
	    else
	    {			# No content
		syswrite STDOUT, "$dump$modebits $uid $gid $psize $fsize\n$path";
	    }
	}
	elsif ($typ == 1)
	{			# DIR
	    syswrite STDOUT, "$dump$modebits $uid $gid $psize $fsize\n$path";
	}
	elsif ($typ == 2)
	{			# LNK
	    my $target;
	    read STDIN, $target, $fsize;
	    syswrite STDOUT, "$dump$modebits $uid $gid $psize $fsize\n$path$target";
	}
    }
    else
    {
	syswrite STDOUT, "$dump$modebits $uid $gid $psize $fsize\n$path";
    }

}


sub usage
{
    print "$progName [OPTIONS]\n\n";
    print "Compress or decompress the file's contents\n\n";
    print "OPTIONS:\n";
    print " -c    ignored as gzip.pl always works on content\n";
    print " -d    decompress the files\n";
    print " -h    this help\n";
    print " -V    print version\n";
    exit;
}

sub version
{
    print "$progName: 0.2.14 (rdup-utils)\n";
    exit;
}


sub sanity_check
{
    my $dump = $_[0];
    my $mode = $_[1];
    my $psize = $_[2];
    my $fsize = $_[3];
    my $uid = $_[4];
    my $gid = $_[5];

    die "$progName: dump must be + or -"   if $dump ne "+" && $dump ne "-";
    die "$progName: mode must be numeric"  unless $mode =~ "[0-9]+";
    die "$progName: psize must be numeric" unless $psize =~ "[0-9]+";
    die "$progName: fsize must be numeric" unless $fsize =~ "[0-9]+";
    die "$progName: uid must be numeric"   unless $uid =~ "[0-9]+";
    die "$progName: gid must be numeric"   unless $gid =~ "[0-9]+";
}


sub copy
{
    my $count = $_[0];
    my $pipe = $_[1];

    my $buf;
    my $n;

    while ($count > 4096)
    {
	$n = read STDIN, $buf, 4096;
	syswrite $pipe, $buf, $n;
	$count -= 4096;
    }
    if ($count > 0)
    {
	$n = read STDIN, $buf, $count;
	syswrite $pipe, $buf, $n;
    }
}


sub cat
{
    my $file = $_[0];

    my $buf;
    my $n;

    while (($n = read $file, $buf, 4096) > 0)
    {
	syswrite STDOUT, $buf, $n;
    }
}

sub catfile
{
    my $name = $_[0];
    open FILE, "<$name" or die "$name: $!";
    cat(*FILE);
    close FILE;
}
