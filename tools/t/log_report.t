# Tests for log_report.pl (run with: prove tools/t)
use strict;
use warnings;
use Test::More;
use File::Temp qw(tempfile);
use FindBin;

my $script = "$FindBin::Bin/../log_report.pl";

sub run_on {
    my ($csv, @args) = @_;
    my ($fh, $path) = tempfile(SUFFIX => '.csv', UNLINK => 1);
    print $fh $csv;
    close $fh;
    my $out = `$^X $script $path @args 2>&1`;
    return ($out, $? >> 8);
}

my $hdr = "t_s,train,position_m,speed_mps,authority_m,mode\n";

my ($out, $code) = run_on($hdr . "0.1,1,0.0,0.1,500,ACCEL\n10.0,1,40.0,8.0,500,EB\n20.0,1,90.0,0.0,500,DONE\n");
is($code, 0, 'safe run passes');
like($out, qr/^1\s+19\.9\s+28\.8\s+1\s+no/m, 'run time, top speed and EB count');

($out, $code) = run_on($hdr . "0.1,1,0.0,1.0,500,ACCEL\n5.0,1,510.0,2.0,500,BRAKE\n9.0,1,515.0,0.0,500,DONE\n");
is($code, 1, 'passing the authority limit fails');
like($out, qr/YES/, 'violation reported');

($out, $code) = run_on($hdr . "0.1,1,0.0,1.0,500,ACCEL\n");
is($code, 1, 'train that never reaches the terminus fails');

($out, $code) = run_on("hello\n");
isnt($code, 0, 'rejects a file that is not a simulator log');

my ($jfh, $jpath) = tempfile(SUFFIX => '.xml', UNLINK => 1);
close $jfh;
run_on($hdr . "0.1,1,0.0,1.0,500,DONE\n", '--junit', $jpath);
open(my $x, '<', $jpath) or die;
my $xml = do { local $/; <$x> };
like($xml, qr/<testsuite name="cbtc_log" tests="2" failures="0">/, 'writes JUnit XML');

done_testing();
