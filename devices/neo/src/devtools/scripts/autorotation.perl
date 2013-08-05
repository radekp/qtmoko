#!/usr/bin/perl

$x = 0;
$y = 0;
$z = 0;


$secondsensorfile = "/dev/input/event3";
#open file in binary mode
open FILE, $secondsensorfile;
binmode FILE;

$count = 0;

$current = 20;




while (read FILE, $buf,16) {

    ($t1,$t2,$type,$code,$value)=unpack "iissi",$buf;
    if ($type==3) {
        if ($code==0) {$x=$value};
        if ($code==1) {$y=$value};
        if ($code==2) {$z=$value};
    }
    if (($type==0) && ($code==0)) {
	$count = $count + 1;
        # printf "%f %05d %05d %05d\n",$t1+$t2/1000000,$x,$y,$z;
	if ($count==5) {
		close(FILE);

		if (($x>0) && ($y>0) && ($current != 180)) {
			printf "neo vertical\n";
			$status = system("LD_LIBRARY_PATH=/opt/qtmoko/lib/ /opt/qtmoko/bin/autorotation 180");
			$current = 180;
		}
		if (($x>0) && ($y<0) && ($current != 90)) {
			printf "neo horizontal\n";
			$status = system("LD_LIBRARY_PATH=/opt/qtmoko/lib/ /opt/qtmoko/bin/autorotation 90");
			$current = 90;
		}
		if (($x<0) && ($y<0) && ($current != 0)) {
			printf "vertical neo\n";
			$status = system("LD_LIBRARY_PATH=/opt/qtmoko/lib/ /opt/qtmoko/bin/autorotation 0");
			$current = 0;
		}
		if (($x<0) && ($y>0) && ($current != 270)) {
			printf "horizontal neo\n";
			$status = system("LD_LIBRARY_PATH=/opt/qtmoko/lib/ /opt/qtmoko/bin/autorotation 270");
			$current = 270;
		}

		sleep(3);
		$count = 0;

		# Now we check if we need to quit
		open(DAT, "/tmp/autorotation_running.dontdelete");
		@raw_data=<DAT>;
		close(DAT);
	
		foreach $line (@raw_data) {
		 	chomp($line);
 			($running) = split(/\|/,$line);
 			if ($running != 1) {
				die("Exiting...");
			}
		}

		open FILE, $secondsensorfile;
	}
    }
}
