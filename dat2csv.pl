#!/usr/bin/perl

use autodie;
use strict;
use warnings;
use Getopt::Long;

my $basedir   = ".";
my $csvdir   = ".";
my $schemefile   = "database_0.scheme";
my $verbose;
my $decode;
my $encode;
my $help;
GetOptions ("scheme=s"  => \$schemefile,
            "basedir=s" => \$basedir,
            "csvdir=s"  => \$csvdir,
            "verbose"   => \$verbose,
            "decode"    => \$decode,
            "encode"    => \$encode,
            "help"      => \$help
            )
or die("Error in command line arguments\n");

$help and usage();

unless ($encode or $decode) {
    print "Error: You must specify either -encode or -decode.\n";
    usage();
}

if ($encode and $decode) {
    print "Error: You cannot specify both -decode and -encode.\n";
    usage();
}

open SCHEME, '<', $schemefile;

#[db=Curry.dat csv=Curry.csv]
my $schemeheaderpattern = '^\[db=(?<inname>[\w_\.]+)\s+csv=(?<outname>[\w_\.]+)\]$';
#name_JP,			string,		0x28
my $schemepattern = '^(?<vname>.*?),\s*(?<vtype>.*?)(,\s*(?<vsize>\d*?|0x[\da-f]*?))?\s*$';

my $prevlien = "";
while (my $lien = <SCHEME>) {
    last unless defined $lien; #The last scheme parse will have exhausted the lines, so catch that.
    chomp $lien;
    
    #This bit for pretending we didn't already read past the next scheme header during the previous parse.
    if (not $prevlien eq "") {
        $lien = $prevlien;
        $prevlien = "";
    }
    elsif ($lien eq "") { next; } #in case there are any empty lines.
    
    if ($lien =~ /$schemeheaderpattern/in) {
        $verbose and print "Found scheme for ". $+{inname}. "\n";
        #read the scheme definition
        my $definition = 0;
        my @scheme;
        while ($lien = <SCHEME>) {
            chomp $lien;
            if ($lien eq "") { next; } #in case there are any empty lines.
            elsif ($lien =~ /$schemepattern/i) {
                my $size;
                if ($+{vtype} eq "uint") {
                    $scheme[1][$definition] = 'I'; #type
                    $size = 4;
                }
                elsif ($+{vtype} eq "int") {
                    $scheme[1][$definition] = 'i'; #type
                    $size = 4;
                }
                elsif ($+{vtype} eq "quad") {
                    $scheme[1][$definition] = 'q'; #type
                    $size = 8;
                }
                elsif ($+{vtype} eq "uquad") {
                    $scheme[1][$definition] = 'Q'; #type
                    $size = 8;
                }
                elsif ($+{vtype} eq "short") {
                    $scheme[1][$definition] = 's'; #type
                    $size = 2;
                }
                elsif ($+{vtype} eq "char") {
                    $scheme[1][$definition] = 'c'; #type
                    $size = 1;
                }
                elsif ($+{vtype} eq "string")  {
                    $scheme[1][$definition] = 'Z'; #type
                    $+{vsize} or pdie ("Error: string defined without valid size at scheme line $.. Stop.\n");
                }
                else { 
                    pdie ("Error: Unknown variable type found at scheme line $.: $2. Stop.\n");
                }
                $scheme[0][$definition] = $+{vname}; #name
                $+{vsize} and $size = $+{vsize};
                $scheme[2][$definition] = $size; #size
                $verbose and print "Found variable: \"".$+{vname}."\", type: ".$+{vtype}.", size: ".$size."\n";
            }
            #Scheme concludes if new scheme definition encountered.
            #However, this will read a line we'll need later, so save that.
            elsif ($lien =~ /$schemeheaderpattern/i) {
                $verbose and print "End of scheme found at line $..\n";
                $prevlien = $lien;
                last;
            }
            else {
                pdie ("Error: Unexpected line in scheme line $.: \"$lien\". Stop.\n");
            }
            $definition++;
        }
        if ($decode) {
            print "Decode ".$basedir."/".$+{inname}." => ".$csvdir."/".$+{outname}.".... ";
            decode($1, $2, \@scheme);
        }
        elsif ($encode) {
            print "Encode ".$csvdir."/".$+{outname}." => ".$basedir."/".$+{inname}.".... ";
            encode($2, $1, \@scheme);
        }
        else { pdie ("Error: I dunno what wwent wrong, but nothing happened!\n"); }
        redo;
    }
    else {
        pdie ("Error: Unexpected line $lien in scheme at line $.\n");
    }
}

close SCHEME;
print "Done.\n";

#----------------------Subroutines--------------------#

sub decode {
    my $inname = $_[0];
    my $outname = $_[1];
    my @scheme = @ { $_[2] };

    my $schemesize = @{ $scheme[0] };
    $verbose and print "\nDecode starting, scheme size is now $schemesize\n";

    open my $fh, '<:raw', "$basedir/$inname";
    my $bytes_read = read $fh, my $bytes, 4;
    pdie ("Got $bytes_read but expected 4\n") unless $bytes_read == 4;
    my $elements = unpack 'I', $bytes; #The amount of entries in the .dat

    open OUT, '>', "$csvdir/$outname";
    for (my $j = 0; $j < $schemesize; $j++) {
        print OUT "\"" . $scheme[0][$j] . "\",";
    }
    print OUT "\n";
    
    #iterate over dat elements
    for (my $i = 0; $i < $elements; $i++) {
        #iterate over scheme elements
        for (my $j = 0; $j < $schemesize; $j++) {
            $j and print OUT ',';
            my $something = binread($fh, $scheme[2][$j], $scheme[1][$j]);
            $scheme[1][$j] eq 'Z' and $something =~ s/"/”/g;
            print OUT "\"$something\"";
        }
        print OUT "\n";
    }
    
    #Simple sanity check.
    if (-s "$basedir/$inname" > tell $fh) {
        print "Warning: not all of $inname was processed. Scheme might be incorrect.\n";
    }
    else {
        print "OK.\n";
    }
    
    close OUT;
    close $fh;
}

sub encode {
    my $inname = $_[0];
    my $outname = $_[1];
    my @scheme = @ { $_[2] };
    my $lien;
    my $entries = 0;

    my $schemesize = @{ $scheme[0] };
    $verbose and print "\nEncode starting, scheme size is now $schemesize\n";

    open IN, '<:crlf', "$csvdir/$inname"; #must read in crlf mode, because csv might be saved with CRLF line endings.
    open my $fh, '>:raw', "$basedir/$outname";
    
    #iterate over csv lines to find how many there are.
    $lien = <IN>; #First line is headings. Ignore.
    while ($lien = <IN>) {
        $lien eq "" or $entries++; #empty lines do not count toward the index.
    }
    binwrite($fh, 4, 'I', $entries);
    
    $. = 0;
    seek IN, 0, 0;
    #iterate over csv lines again to actually process them.
    $lien = <IN>; #First line is headings. Ignore.
    while ($lien = <IN>) {
        chomp $lien;
        $lien eq "" and next; #empty lines are ignored.
        my $element = 0;
        $verbose and print "\n";
        #iterate over csv elements
        while ($lien =~ /(^|,)(?<capture>[^\",]*?|\".*?\")(?=,|$)/g) {
            if ( defined $+{capture} ) { 
                $verbose and print "$outname: column $element matched '$+{capture}'\n"; 
            }
            else { 
                die "Error: could not read .csv column $element at line $.\n"; 
            }
            
            $element >= $schemesize and pdie ("Error: $inname has more columns than are defined in the scheme. Stop.\n");
            
            #csv quotes don't go into the output database.
            my $data = $+{capture};
            $data =~ s/^\"(.*)\"$/$1/;
            $scheme[1][$element] eq 'Z' and $data =~ s/”/"/g; #Bug: this breaks Japanese quotes, but it doesn't break the game I think.
            
            binwrite($fh, $scheme[2][$element], $scheme[1][$element], $data);
            $element++;
        }
        $element < $schemesize and pdie ("Error: $inname has fewer columns than are defined in the scheme. Stop.\n");
    }
    
    print "OK.\n";
    close IN;
    close $fh;
}

sub binread {
    my $fh = $_[0];
    my $rlen = $_[1];
    my $rtype = $_[2];
    
    $rlen =~ /0x[\da-f]+/i and $rlen = hex($rlen);
    
    my $bytes_read = read $fh, (my $bytes), $rlen;
    $bytes_read == $rlen or pdie ("Error: Could not read more than $bytes_read bytes when expected $rlen. Scheme might be incorrect.\n");
    
    return unpack $rtype . $rlen, $bytes;
}

sub binwrite {
    my $fh = $_[0];
    my $rlen = $_[1];
    my $rtype = $_[2];
    my $data = $_[3];
    my $bin;
    
    $rlen =~ /0x[\da-f]+/ and $rlen = hex($rlen);
    $rtype eq "Z" and $bin = pack $rtype . $rlen, $data or $bin = pack $rtype, $data; #only strings take a size for pack.
    
    print $fh $bin;
}

sub pdie {
    print $_[0];
    exit -1;
}

sub usage {
    print 
"Disgaea 5 database <-> csv converter by BTAxis. Usage: $0 <-decode/-encode> [options]
      Options:
        -decode: decode D5 database files into human-readable csv files.
        -encode: encode csv files into D5 database files).
        -scheme=<path>: Scheme file to use. If unspecified, './database_0.scheme' will be used.
        -basedir=<path>: path to use for D5 database files. If unspecified, current directory will be used.
        -csvdir=<path>: path to use for csv files. If unspecified, current directory will be used.
        -verbose: print verbose debug output.
        -help: print this message.
";
    exit 0;
}

