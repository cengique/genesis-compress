#! /bin/bash
if [ -z "$1" ]; then
	echo ""
	echo "gencompress.sh:"
	echo "  Compresses given Genesis binary files generated with the disk_out"
	echo "Genesis object. An a.bin file would become a.genflac. Existing destination"
	echo "files are skipped."
	echo ""
	echo "   Requires at least 1 argument "
	echo "   1. (Required) Name pattern of files to compress, in DOUBLE QUOTES." 
	#echo "      --> You should be in the directory where the files are located."
	echo "   2. (Optional): Destination directory for compressed files."
	echo "      --> Directory will be created if it doesn't exist."
	echo ""
	echo "   Usage example: "
	echo "	   $0 \"data/*.bin\" compressed_data/"
	echo ""
	exit -1
fi

trap exit INT

commandname=gen2flac

# DO in same directory if no destination specified
if [ -z "$2" ]; then
	destdir=`pwd`
else
	destdir=$2
	if [ -d $destdir ]; then
		destdir=${destdir%%/}
	else
		# Create if doesn't exist
		mkdir $destdir
		#echo "Destination directory $2 does not exist."
		#echo "Program exiting."
		#exit -1
	fi
fi

echo "destination for compressed files: $destdir"
for i in $1; do
	newfname="$destdir/${i%%.bin}.genflac"
	[ ! -r $newfname ] && $commandname $i $newfname
done
