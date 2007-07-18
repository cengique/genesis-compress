/*

Program to uncompress FLAC-compressed Genesis files into 
Genesis disk_in compatible format.

Copyright (c) 2006 Cengiz Gunay 
<cengique@users.sf.net> 2006/02/26 

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

****

    Uses some code from Alfonso Delgado-Reyes 07.03.2002
    
    Any cc compiler should work ([ ] = optional)
	
	To compile:
	  In Winblows (MS Visual C++ 6.x):
		cc [-v -O] -DWIN32 -o flac2gen flac2gen.c
	  To create 64-bit binary (on 64-bit architecture):
		gcc -O -o flac2gen-x86_64 flac2gen.c
	  To create 32-bit binary:
		gcc -O -m32 -malign-double -o flac2gen flac2gen.c

  $Id$
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h> /* isalnum */
#if defined(WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif
#include <string.h>
#include <fcntl.h>
#if !defined(__APPLE__)
#include <malloc.h>
#endif
#include <float.h>
#include <math.h>
#include <assert.h>

#define COMMANDNAME "flac2gen"

#include "genesis16bit.h"

#define ADFBLOCK 12000
#define BUF_SIZE 1024
#define OLD_BUF_SIZE 240
#define TRUE 1

#define GEN_EXT ".bin\0"

/** Similar to the Matlab reader, convert from 16bit to float and
 *  arrange data in columns. */
float *scramble_data_for_genesis(struct file_info_16bit_data *unflac_data) {
  int chan8blocksize, num_chans, chan, offset, i;
  struct fixed_point_16bit_format_v0_3 *file_info = unflac_data->file_info;
  unsigned short *data = unflac_data->data;
  struct fixed_point_16bit_format_chan *chan_ranges = unflac_data->chan_ranges;
  float *fdata;

  if ((fdata = (float *)malloc(file_info->num_samples * file_info->num_chans *
			       sizeof(float))) == NULL) {
    fprintf(stderr, "\n" COMMANDNAME ": could not malloc data array of %d bytes.\n",
	    file_info->num_samples * file_info->num_chans * sizeof(float));
    free(chan_ranges);
    free(file_info);
    free(data);
    return(NULL);
  }

  chan8blocksize = 8 * file_info->num_samples;
  num_chans = file_info->num_chans;

  /* now scale, and reconstruct >8 chan data */
  for (chan = 0; chan < num_chans; chan++) {
    /* Until the last less than 8 channels, use 8 channel encoding.
     * In the last part, use less-than-8-channels encoding. */
    offset = chan / 8 * chan8blocksize + chan % 8;

    /* Scale the data to reconstruct floats from 16-bit integers */
    for (i = 0; i < file_info->num_samples; i ++) {
      fdata[i*num_chans + chan] = chan_ranges[chan].range_low +
	data[i * ( (chan/8 + 1) * 8 > num_chans ? num_chans % 8: 8) + offset] *
	(chan_ranges[chan].range_high - chan_ranges[chan].range_low) / 0xFFFFU;
    }
  }

  return fdata;
}

void print_usage_exit() {
  fprintf(stderr, "\nUncompresses a previously FLAC-compressed Genesis (.genflac) \n"
	  "data file to generate a Genesis disk_in object-compatible file.\n");
  fprintf(stderr, "\nUsage: " COMMANDNAME " infilename [outfilename]\n");
  exit(-1);
}

int main(int argc, const char **argv) {
  char		*filename, *outfilename, *ext_start;
  int 		num_items, num_bytes;
  float 	*fdata;
  struct file_info_16bit_data *read_data;
  struct genesis_disk_out_format out_header = { "FMT1\0", 0.0f, 0.0f, 1, 4};
  FILE *fp;
  
  if (argc < 2) print_usage_exit();
  filename = argv[1];

  if (argc < 3) {
    /* Arg 2 is optional filename, o/w replace w/ .bin extension */
    /* Replace .genflac with .bin for output file */
    if ((ext_start = strrchr(filename, '.')) == NULL) {
      ext_start = filename + strlen(filename);
    }
    outfilename = (char*) malloc(ext_start - filename + strlen(GEN_EXT) + 1);
    strncpy(outfilename, filename, ext_start - filename);
    strcpy(&outfilename[ext_start - filename], GEN_EXT);
  } else {
    outfilename = argv[2];
  }
  
  /*if (!isalnum(filename[0])) {
    fprintf(stderr, "\n" COMMANDNAME ": first argument must be a string");
    print_usage_exit();
    }*/
  
  if ((read_data = unflac_file(filename)) != NULL) {
    out_header.dt = 1.0e-3f / read_data->file_info->sample_rate;
    out_header.num_chans = read_data->file_info->num_chans;

    /* Open the file */
    if ((fp = fopen(outfilename, "wb")) == NULL) {
      fprintf(stderr, "\n" COMMANDNAME ": could not open file '%s' for writing.\n", 
	      outfilename);
      return(-1);
    }

    printf("Size of header: %d\n", sizeof(out_header));

    /* Write header */
    if ((num_items = fwrite(&out_header, sizeof(out_header), 1, fp)) != 1) {
      fprintf(stderr, "\n" COMMANDNAME ": could not write header to file '%s'\n",
	      outfilename);
      return(-1);
    }

    /* Write the junk */
    if (fseek(fp, read_data->file_info->num_chans * sizeof(float) * 3, SEEK_CUR)) {
      fprintf(stderr, "\n" COMMANDNAME ": could not write Genesis junk header\n");
      return(-1);
    }
    /* Convert data into Genesis format and append*/
    fdata = scramble_data_for_genesis(read_data);
    
    if (fdata == NULL) {
      free(read_data->data);
      free(read_data->file_info);
      free(read_data->chan_ranges);
      free(read_data);
      fprintf(stderr, "\n" COMMANDNAME ": could not allocate memory for data.");
      return(-1);
    }
    num_bytes = read_data->file_info->num_samples * 
      read_data->file_info->num_chans *
      sizeof(float);
    if ((num_items = fwrite(fdata, num_bytes, 1, fp)) != 1) {
      fprintf(stderr, "\n" COMMANDNAME ": could not write data to file '%s', "
	      "while writing %lu bytes fwrite returned %d\n", 
	      outfilename, num_bytes, num_items);
      return(-1);
    }

    free(read_data->data);
    free(read_data->file_info);
    free(read_data->chan_ranges);
    free(read_data);
  } else {
    fprintf(stderr, "\n" COMMANDNAME ": error... see output above.\n");
  }
  
  return;
}
