#!/usr/bin/perl

use strict;

my ($i, $j, $fileName,@inArray,$arrayName, $station, $chamber); 
my ($station, $chamber, $stub, $endcap, $sector, $mainCount); 

# print("starting script .. \n"); 
 
    $stub    = "th_corr_lut"; 
 
#    $station = 2; 
#    $chamber = 1; 
 
$j = 0; 
 
for ($station = 0; $station <= 1; $station++){ 
     
    for ($chamber = 1; $chamber <= 3; $chamber++){ 
	 
	for ($mainCount = 0; $mainCount < 12; $mainCount++){ 
 
	    $j++; 
 
	    @inArray = (); 
  
	    {  
		use integer;  
		$endcap  = $mainCount / 6 + 1; 
		$sector  = $mainCount % 6 + 1 ; 
	    } 
	     
	    $arrayName = join "_", $stub, "st_1","sub", "$station", "ch" , "$chamber"; 
	    $arrayName = "_$arrayName"; 
	     
	    $fileName = join "_", "vl", $stub, "endcap", $endcap, "sec", $sector,  
	    "sub", $station+1, "st_1", "ch" , $chamber; 
	     
	    $fileName = "$fileName.lut"; 
	     
	    open(FILE,"<", $fileName) or die "Can't find file $fileName $!\n"; 
	     
	    $i = 0; 
	     
	    while (<FILE>){ 
		chomp; 
		$inArray[$i] = "0x$_"; 
		$i++; 
	    } 
 
	    close (FILE); 
	     
	    if ($mainCount == 0){ 
		print("unsigned int $arrayName\[12\][$i] = {\n"); 
	    } 
	     
	    print ("\n// Endcap: $endcap Sector: $sector"); 
	     
	    for ($i = 0; $i < $#inArray; $i++){ 
		if ($i%10 == 0){ 
		    print("\n");  
		    if ($i == 0) { 
			print("{ "); 
		    } else { 
			print("  "); 
		    } 
		} 
		 
		printf("%5s, ", $inArray[$i]); 
	    } 
	     
	    printf("%5s  %5s",$inArray[$#inArray],"}"); 
	     
	    print(",\n\n"); 
	     
	} 
	 
	print("};\n"); 
		
    } 
 
} 
 
print("\n// thcorrst1.pl merged $j files\n"); 
