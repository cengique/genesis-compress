#! /bin/bash

if [ -z "$1" ]; then
	echo ""
	echo "genuncompress.sh file_pattern [destination_dir]"
	echo "  Uncompresses given Genesis binary files compressed with convertgenesis16bit"
	echo "command. An a.genflac file would become a.bin. Existing destination"
	echo "files are skipped."
	echo ""
	echo "Parameters: "
	echo "   file_pattern: Wildcard pattern of files to uncompress, in DOUBLE QUOTES." 
	echo "   destination_dir: Destination directory for uncompressed files."
	echo "      		  Directory will be created if it doesn't exist."
	echo ""
	echo "   Usage example: "
	echo "	   $0 \"data/*.genflac\" uncompressed_data/"
	echo ""
	exit -1
fi

trap exit INT

commandname=flac2gen

# DO in same directory if no destination specified
if [ -z "$2" ]; then
	destdir=`pwd`
else
	destdir=$2
	if [ -d $destdir ]; then
		destdir=${destdir%%/}
	else
		# Create if missing
		mkdir $destdir
	fi
fi

echo "destination for uncompressed files: $destdir"
for i in $1; do
	newfname="$destdir/${i%%.genflac}.bin"
	[ ! -r $newfname ] && $commandname $i $newfname
done
