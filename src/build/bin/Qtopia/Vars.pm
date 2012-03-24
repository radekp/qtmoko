package Qtopia::Vars;

use strict;
use warnings;

use File::Basename;
use Qtopia::Paths;
use Qtopia::File;
use Carp;
#perl2exe_include Carp::Heavy
$Carp::CarpLevel = 1;
use Digest::MD5 qw(md5_base64);

require Exporter;
our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Qtopia::opt ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
    $isWindows
    $isMac
    $qtopiaVersionStr
    $qtVersionStr
    $shadow
    check_script
    configopt
    script_name

    $depotpath
    $SDKROOT
);

our $VERSION = '0.01';

# imported variables
our $depotpath;
our $SDKROOT;

use constant TRACE => 0;

# Platform detection
our $isWindows = ( $^O eq "MSWin32" );
our $isMac = ( $^O eq "darwin" );
our $qtopiaVersionStr;
our $qtVersionStr;
our @configureoptions;
our $shadow;
our $perl2exe_hash;

# Check for a bug in perl on RedHat 9.1
if ( !$isWindows ) {
    TRACE and print "Qtopia::Vars::perl_bug_test()\n";
    my $testfile = "/etc/passwd";
    open IN, "$testfile" or die "Can't read $testfile";
    my $common_perl_bug_test = <IN>;
    close IN;
    if ( !defined($common_perl_bug_test) ) {
        die "ERROR: Your /etc/passwd file has no contents!";
    }
    unless ( $common_perl_bug_test =~ /[^\s]+/ ) {
        warn "WARNING: Your perl has a bug with regular expressions and UTF-8\n";
        # remove the UTF8 bit from the LANG environment variable and relaunch
        my $lang = $ENV{LANG};
        if ( !$lang ) {
            die "ERROR: ".basename($0)." cannot work around the bug.\n";
        }
        unless ( $lang =~ s/\.UTF-?8//i ) {
            die "ERROR: ".basename($0)." cannot work around the bug.\n";
        }
        warn "WARNING: ".basename($0)." will attempt to work around the bug by changing your\n".
             "         LANG variable from ".$ENV{LANG}." to $lang.\n";
        $ENV{LANG} = $lang;
        exec($0, @ARGV);
        die "ERROR Could not relaunch ".basename($0);
    }

    # RedHat upgraded their perl so that it does not trigger the test above but now
    # it overruns output data, potentially leading to an unexplained segfault.
    # Try to detect RedHat 9 and just force the fix.
    my $redhat_verfile = "/etc/redhat-release";
    if ( -f $redhat_verfile ) {
        open IN, $redhat_verfile or die "Can't read $redhat_verfile";
        $_ = <IN>;
        close IN;
        if ( !defined($_) ) {
            die "ERROR: Your /etc/redhat-release file has no contents!";
        }
        if ( /release 9/ ) {
            my $lang = $ENV{LANG};
            if ( $lang =~ /\.UTF-?8/i ) {
                warn "WARNING: Your perl has a bug with output and UTF-8\n";
                # remove the UTF8 bit from the LANG environment variable and relaunch
                unless ( $lang =~ s/\.UTF-?8//i ) {
                    die "ERROR: ".basename($0)." cannot work around the bug.\n";
                }
                warn "WARNING: ".basename($0)." will attempt to work around the bug by changing your\n".
                     "         LANG variable from ".$ENV{LANG}." to $lang.\n";
                $ENV{LANG} = $lang;
                exec($0, @ARGV);
                die "ERROR Could not relaunch ".basename($0)."\n";
            }
        }
    }
}

sub check_script
{
    TRACE and print "Qtopia::Vars::check_script()\n";
    my ( $script, $path, $arg ) = @_;
    #print "check_script $script $path ".(defined($arg)?$arg:"")."\n";
    my $compiled_code = 0;
    if ( $script =~ /\.exe$/i ) {
        $compiled_code = 1;
    }
    my $orig = "$path/".script_name($script);
    # perl2exe test run bail out hook
    if ( $compiled_code && defined($arg) && $arg eq "-nop" ) {
        if ( !check_perl2exe_hash($orig) ) {
            die "ERROR: Stored hash does not match script hash:\n".
                "       $0\n".
                "       $orig\n";
        }
	exit 0;
    }
    # If we're in the depot, run the perl scripts directly
    if ( $compiled_code ) {
        $script = script_name($script);
        #print "running the perl script ".fixpath("$path/$script")."\n";
        my $ret = system("perl $path/$script ".(scalar(@ARGV)?"\"":"").join("\" \"", @ARGV).(scalar(@ARGV)?"\"":""));
        $ret = $ret >> 8;
        exit $ret;
    }
    # Windows doesn't set HOME but the build system expects it to be set!
    $ENV{HOME} = $ENV{HOMEDRIVE}.$ENV{HOMEPATH};
}

# Check the md5sum embedded in the .exe file with the script it was built from
sub check_perl2exe_hash
{
    my ( $orig ) = @_;
    if ( !defined($perl2exe_hash) ) {
        die "ERROR: Stored hash is missing!";
    }
    open IN, "$orig" or die "Can't read $orig";
    my $data = join("", <IN>);
    close IN;
    my $md5 = md5_base64($data);
    #print "stored hash $perl2exe_hash\n";
    #print "script hash $md5\n";
    if ( $perl2exe_hash eq $md5 ) {
        return 1;
    }
    return 0;
}

# Is a particular value in the .configureoptions file
sub configopt
{
    TRACE and print "Qtopia::Vars::configopt()\n";
    my $dir = $depotpath;
    if ( ! $dir ) {
        $dir = $SDKROOT;
    }
    if ( ! $dir ) {
        #Qtopia::Paths::get_vars();
        croak "You must use Qtopia::Paths and call get_paths() before using Qtopia::Vars";
    }
    if ( ! @configureoptions ) {
        if ( -f "$dir/LICENSE.GPL" ) {
            push(@configureoptions, "free");
        }
        if ( -d "$dir/src/qtopiadesktop" ) {
            push(@configureoptions, "desktop");
        }
    }
    my ( $opt ) = @_;
    if ( grep { $_ eq $opt } @configureoptions ) {
	return 1;
    }
    return 0;
}

sub script_name
{
    TRACE and print "Qtopia::Vars::script_name()\n";
    my ( $script ) = @_;
    $script = basename($script);
    my $len = length $script;
    my $compiled_code = 0;
    if ( basename($script) =~ /\.exe$/i ) {
        $compiled_code = 1;
    }
    if ( $compiled_code ) {
	$len -= 4;
    }
    return substr($script, 0, $len);
}

# Make this file require()able.
1;
