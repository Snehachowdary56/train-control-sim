#!/usr/bin/env perl
# Reads a cbtc_sim CSV log and prints a test report per train: run time, top speed, time spent in each
# mode, emergency brake events, and any authority violation. Exit code 1 if a safety check fails.
# Usage: perl tools/log_report.pl run.csv [--junit report.xml]
use strict;
use warnings;

my ($file, $junit);
while (my $a = shift @ARGV) {
    if ($a eq '--junit') { $junit = shift @ARGV } else { $file = $a }
}
die "usage: $0 run.csv [--junit report.xml]\n" unless defined $file;
open(my $fh, '<', $file) or die "cannot open $file: $!\n";

my $header = <$fh>;
die "not a cbtc_sim log\n" unless defined $header && $header =~ /^t_s,train,position_m,speed_mps,authority_m,mode/;

my %t;    # per-train stats
my $rows = 0;
while (my $line = <$fh>) {
    chomp $line;
    next unless length $line;
    my ($ts, $train, $pos, $speed, $ma, $mode) = split /,/, $line;
    die "bad row $.: $line\n" unless defined $mode && $train =~ /^\d+$/;
    $rows++;
    my $s = $t{$train} //= { first => $ts, last => $ts, vmax => 0, eb => 0, viol => 0, prev => '', modes => {} };
    $s->{last} = $ts;
    $s->{vmax} = $speed if $speed > $s->{vmax};
    $s->{eb}++   if $mode eq 'EB' && $s->{prev} ne 'EB';
    $s->{viol}++ if $pos > $ma + 0.01;
    $s->{modes}{$mode}++;
    $s->{prev} = $mode;
}
close $fh;
die "log has no rows\n" unless $rows;

printf "%-6s %9s %8s %4s %10s  %s\n", 'train', 'run_s', 'max_kmh', 'EB', 'violation', 'mode samples';
my @cases;
for my $id (sort { $a <=> $b } keys %t) {
    my $s = $t{$id};
    my $modes = join ' ', map { "$_=$s->{modes}{$_}" } sort keys %{ $s->{modes} };
    printf "%-6d %9.1f %8.1f %4d %10s  %s\n", $id, $s->{last} - $s->{first}, $s->{vmax} * 3.6, $s->{eb},
        ($s->{viol} ? 'YES' : 'no'), $modes;
    push @cases, { name => "train_${id}_stays_within_authority", fail => $s->{viol} };
    push @cases, { name => "train_${id}_reaches_terminus", fail => ($s->{prev} ne 'DONE') };
}
my $failed = grep { $_->{fail} } @cases;
printf "%d rows, %d trains, %d checks, %d failed\n", $rows, scalar(keys %t), scalar(@cases), $failed;

if ($junit) {    # JUnit XML so CI can show the results as test cases
    open(my $out, '>', $junit) or die "cannot write $junit: $!\n";
    printf $out qq{<?xml version="1.0" encoding="UTF-8"?>\n<testsuite name="cbtc_log" tests="%d" failures="%d">\n},
        scalar(@cases), $failed;
    for my $c (@cases) {
        print $out qq{  <testcase name="$c->{name}">}, ($c->{fail} ? '<failure message="check failed"/>' : ''), "</testcase>\n";
    }
    print $out "</testsuite>\n";
    close $out;
}
exit($failed ? 1 : 0);
