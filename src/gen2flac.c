/*
Quantizes floating-point data to 16-bit fixed-point from file created
by Genesis diskio object. Then uses the FLAC utility
(http://flac.sf.net) to losslessly compress the data.

Copyright (c) 2005 Cengiz Gunay <cengique@users.sf.net> 2005-03-14

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


Uses code to read diskio files by Dieter Jaeger and
Alfonso Delgado-Reyes.

To compile:
  In Winblows (MS Visual C++ 6.x):
	cc [-v -O] -DWIN32 -o gen2flac gen2flac.c
  To create 64-bit binary (on 64-bit architecture):
	gcc -O -o gen2flac-x86_64 gen2flac.c
  To create 32-bit binary:
	gcc -O -m32 -malign-double -o gen2flac gen2flac.c
    
TODO:
- check flac version to use the '-f' option or not
- Use getopt to:
  - define tolerance parameter on maximal range
  - specify number of channels to take

$Id$
*/

#include <stdio.h>
#include <stdlib.h>
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

#define ADFBLOCK 12000
#define TRUE 1

#define GENFLAC_EXT ".genflac"
#define COMMANDNAME "gen2flac"

#define min(a,b) ((a>b)?b:a)
#define max(a,b) ((a<b)?b:a)

struct filter_type {
  double lowcut;
  double highcut;
  double notchlow;
  double notchhigh;
};

#include "genesis16bit.h"

struct analog_format {
  int		type;
  int		file_type;
  int		file_idx;
  int		cross_idx;
  int		trace_no;
  char		title[240];
  int		channel;
  int		color;
  int		plot_group;
  int		select;
  int		comp_sign;
  struct	filter_type filter;
  float		gain;
  int		invert;
  float		offset;
  float		factor;
  float		overlay_pos;
  float		overlay_val;
  float		xmax;
  float		xmin;
  long		num_samples;
  float		sample_rate;
  int		filled;
  double	*fdata;
};

struct analog_format *raw = NULL;

static void wswap(ar1, ar2)
char *ar1, *ar2;
{
    /* swap the bits around for big <--> little endian */
	
  /* little --> big */
  *ar1     = *(ar2+3);
  *(ar1+1) = *(ar2+2);
  *(ar1+2) = *(ar2+1);
  *(ar1+3) = *ar2;
}

int scaledata(const char *outfilename, const double *min_val, const double *max_val,
	      int num_chans) {
  int num_items, i, chan, testlen, padded_size, offset, offset_i, chan8blocksize;
  FILE *fp, *tfp, *tfp2;
  unsigned short*	data;
  struct fixed_point_16bit_format_v0_3 *fixed_raw;
  struct fixed_point_16bit_format_chan *chan_ranges;
  char template[BUFSIZE] = "/tmp/genesis_raw_XXXXXX\0", 
    template2[BUFSIZE] = "/tmp/genesis_comp_XXXXXX\0";
  int fd, fd2;
  char	commandbuffer[BUFSIZE];
  char *extraopts;

  if ((fixed_raw = (struct fixed_point_16bit_format_v0_3 *)
       malloc(sizeof(struct fixed_point_16bit_format_v0_3))) == NULL) {
    fprintf(stderr, "\ngen2flac: could not malloc data structure\n");
    return(-1);
  }

  /* put magic number as of version 0.3 */
  strncpy((char*)&fixed_raw->magic_number, NESA_MAGIC, 4);

  fixed_raw->version_major = 0;
  fixed_raw->version_minor = 3;
  fixed_raw->sample_rate = raw->sample_rate;
  fixed_raw->num_samples = raw->num_samples;
  fixed_raw->num_chans = num_chans;
  fixed_raw->header_len = sizeof(*fixed_raw) + num_chans * sizeof(*chan_ranges);
  fixed_raw->is_compressed = TRUE;

  if ((chan_ranges = (struct fixed_point_16bit_format_chan *)
       malloc(num_chans * sizeof(struct fixed_point_16bit_format_chan))) == NULL) {
    fprintf(stderr, "\ngen2flac: could not malloc data structure\n");
    return(-1);
  }

  if ((data = (short int *)malloc(raw->num_samples * num_chans * sizeof(short int))) 
      == NULL) {
    fprintf(stderr, "\ngen2flac: could not malloc data array of %d bytes.\n",
	    raw->num_samples * num_chans * sizeof(short int));
    free(fixed_raw);
    return(-1);
  }

  chan8blocksize = 8 * raw->num_samples;

  /* now scale the data */
  for (chan = 0; chan < num_chans; chan++) {
    offset = chan / 8 * chan8blocksize + chan % 8;

    /* printf("Chan %d: Min: %e, Max: %e\n", chan, min_val[chan], max_val[chan]);*/
    chan_ranges[chan].range_low = min_val[chan];
    chan_ranges[chan].range_high = max_val[chan];

    /* Scale the data and truncate to 16-bit integers */
    for (i = 0; i < raw->num_samples; i++) {
      offset_i = i * ( (chan/8 + 1) * 8 > num_chans ? num_chans % 8: 8) + offset;
      data[offset_i] =
	(raw->fdata[offset_i] - min_val[chan]) / (max_val[chan] - min_val[chan]) * 0xFFFFU;
    }
  }

  /* Create file */
  if ((fp = fopen(outfilename, "wb")) == NULL) {
    fprintf(stderr, "\ngen2flac: could not create file '%s'\n", outfilename);
    free(data);
    free(fixed_raw);
    return(-1);
  }

  /* Write file header */
  if ((num_items = fwrite(fixed_raw, sizeof(*fixed_raw), 1, fp)) != 1) {
    fprintf(stderr, "\ngen2flac: could not write to file '%s'\n", outfilename);
    free(data);
    free(fixed_raw);
    return(-1);
    }

  /* Write channel header */
  if ((num_items = fwrite(chan_ranges, num_chans * sizeof(*chan_ranges), 1, fp)) != 1) {
    fprintf(stderr, "\ngen2flac: could not write to file '%s'\n", outfilename);
    free(data);
    free(fixed_raw);
    return(-1);
    }

  /* printf("Size of header is %d\n", fixed_raw->header_len); */

  /* Create temporary file */
  if ((fd = mkstemp(template)) == -1 || (tfp = fdopen(fd, "wb")) == NULL) {
    fprintf(stderr, "\ngen2flac: could not create temporary file '%s'\n", template);
    free(data);
    free(fixed_raw);
    return(-1);
  }

  /* Write file data to temp file */
  padded_size = sizeof(*data) * fixed_raw->num_samples * num_chans;
  if (num_chans > 8) 
    padded_size += (8 - padded_size /sizeof(*data) % 8) * sizeof(*data);
  if ((num_items = fwrite(data, padded_size, 1, tfp)) != 1) {
    fprintf(stderr, "\ngen2flac: could not write to file '%s'\n", template);
    free(data);
    free(fixed_raw);
    return(-1);
  }
  fclose(tfp);
  fclose(fp);

  /* Create output temporary file */
  if ((fd2 = mkstemp(template2)) == -1 || (tfp2 = fdopen(fd2, "rb")) == NULL) {
    fprintf(stderr, "\ngen2flac: could not create temporary file '%s' "
	    "for compressed data.\n", template2);
    free(data);
    free(fixed_raw);
    return(-1);
  }
  fclose(tfp2);

  extraopts = get_flac_version_opts();

  /* Call compressor to read from tmp file and write to tmp file2 */
  testlen = 
    sprintf(commandbuffer,
	    "flac -s --force-raw-format --channels=%d --bps=16 --sample-rate=44100 "
	    "--endian=little --sign=unsigned -8 %s %s -o %s", min(num_chans, 8),
	    extraopts, template, template2);
  assert(testlen <= BUFSIZE);
  if (system(commandbuffer) != 0) {
    fprintf(stderr, "\ngen2flac: could not execute compressor command:\n%s\n",
	    commandbuffer);
    free(data);
    free(fixed_raw);
    return(-1);
  }

  /* Append compressed data in tmpfile2 to outfile */
  testlen = sprintf(commandbuffer, "cat %s >> %s", template2, outfilename);
  assert(testlen <= BUFSIZE);
  if (system(commandbuffer) != 0) {
    fprintf(stderr, "\ngen2flac: could not execute concat command:\n%s\n",
	    commandbuffer);
    free(data);
    free(fixed_raw);
    return(-1);
  }

  free(data);
  free(fixed_raw);

  /* Remove temp files */
  if (remove(template) != 0 || remove(template2) != 0) {
    fprintf(stderr, "\ngen2flac: could not delete the temporary files: %s or %s\n",
	    template, template2);
  }
  return 0;  
}

int get_gendata(const char *infilename, const char *outfilename) {
    int     	datatype, i, chan, offset, chan8blocksize;
    int		num_chans, headersize, pci;
    long	num_items, blockpos, readblock;
    long 	fst_pt, lst_pt, dat_points, flength;
    float   	pcf, fval, ftemp[ADFBLOCK], startti, tistep, xmax_read, gain;
    double 	*min_val, *max_val;
    char	headstr[256], titlebuffer[256];
    int 	plotno = 1;
    FILE	*fp;
    
    if ((fp = fopen(infilename, "rb")) == NULL) {
        fprintf(stderr, "\ngen2flac: could not open file '%s'\n", infilename);
        return(-1);
    }
    
    if ((raw = (struct analog_format *)
               malloc(1*sizeof(struct analog_format))) == NULL) {
        fprintf(stderr, "\ngen2flac: could not malloc data structure\n");
        fclose(fp);
        return(-1);
    }
    
    raw->gain     = 1; /* 0.001; */ 
    raw->type     = 3;
    raw->factor   = 1;
    raw->offset   = 0.0;
    raw->xmin     = 0.0;
    raw->xmax     = 100000.0;
    raw->fdata    = NULL;
    raw->trace_no = plotno;
    raw->channel  = 1;
    raw->invert   = 0;
    raw->sample_rate = 1;
    
    gain = raw->gain;

#if !defined(__APPLE__)      || \
     !defined(__BIG_ENDIAN__) || \
     !defined(WORDS_BIGENDIAN)
	fval = 0.0;
#endif

    fseek(fp, 0L, SEEK_SET);
    fread(headstr, sizeof(char), 80, fp);
    fread(&pcf, sizeof(float), 1, fp);
    
#if defined(__APPLE__)      || \
    defined(__BIG_ENDIAN__) || \
    defined(WORDS_BIGENDIAN)
    wswap((char *)&startti, (char *)&pcf);
#else
	startti = pcf;
#endif
    
    fread(&pcf, sizeof(float), 1, fp);
    
#if defined(__APPLE__)      || \
    defined(__BIG_ENDIAN__) || \
    defined(WORDS_BIGENDIAN)
    wswap((char *)&tistep, (char *)&pcf);
#else
	tistep = pcf;
#endif
    
    raw->sample_rate = 0.001 / tistep;
    
    fread(&pci, sizeof(int), 1, fp);
	
#if defined(__APPLE__)      || \
    defined(__BIG_ENDIAN__) || \
    defined(WORDS_BIGENDIAN)
	wswap((char *)&num_chans, (char *)&pci);
#else
	num_chans = pci;
#endif

	/*fprintf(stderr, "\ngen2flac: num. chans = %d\n", num_chans);*/
	
    fread(&pci, sizeof(int), 1, fp);

#if defined(__APPLE__)      || \
    defined(__BIG_ENDIAN__) || \
    defined(WORDS_BIGENDIAN)
	wswap((char *)&datatype, (char *)&pci);
#else
	datatype = pci;
#endif
	
    headersize = 80 + 2 * sizeof(float) +
                 2 * sizeof(int) + 3 * num_chans * sizeof(float);
    
    if (plotno < 0 || plotno > num_chans) {
        fprintf(stderr,
             "\ngen2flac: requested plot %i not available (plots = %i)\n",
                plotno, num_chans);
        free(raw);
        fclose(fp);
        return(-1);
    }
    
    fseek(fp, 0L, SEEK_END);
    flength = ftell(fp);
    dat_points = (flength - headersize) / (num_chans * sizeof(float));
    xmax_read = (float)(dat_points) * tistep * 1000.0;
    
    if (raw->xmin >= xmax_read) {
        fprintf(stderr,
                "\ngen2flac: no data available above given xmin\n");
        free(raw);
        fclose(fp);
        return(-1);
    }
    if (raw->xmax > xmax_read) {
        raw->xmax = xmax_read;
    }
    
    fst_pt = (long) (raw->xmin * raw->sample_rate);
    lst_pt = (long) (raw->xmax * raw->sample_rate);
    
    raw->num_samples = lst_pt - fst_pt;
    
    raw->fdata = (double *)malloc(raw->num_samples*num_chans*sizeof(double));

    min_val = (double *)malloc(num_chans*sizeof(double));
    max_val = (double *)malloc(num_chans*sizeof(double));
    
    if (raw->fdata == NULL) {
        fprintf(stderr, "\ngen2flac: could not malloc data array of %d bytes.\n",
		raw->num_samples*num_chans*sizeof(double));
        free(raw);
        fclose(fp);
        return(-1);
    }
    
    raw->filled    = 1;
    raw->file_type = 3;
    sprintf(titlebuffer, "Plot %i from %s; read from %f to %f msec",
            plotno, infilename, raw->xmin, raw->xmax);
    strcpy(raw->title, titlebuffer);
    
    fseek(fp, headersize, SEEK_SET);
    fseek(fp, (plotno-1) * sizeof(float), SEEK_CUR);
    fseek(fp, fst_pt * (num_chans * sizeof(float)), SEEK_CUR);
    
    num_items = -1;
    blockpos = 0;
    
    readblock = ADFBLOCK - (ADFBLOCK % num_chans);
    
    chan8blocksize = 8 * raw->num_samples;
    /* fill data pointer, convert to double */
    
    for (i = 0; i < raw->num_samples; i++) {
        if (blockpos >= num_items) {
            if ((num_items = fread(ftemp, sizeof(float), readblock, fp)) == 0)
            {
                break;
            } else {
                blockpos = 0;
            }
        }
	for (chan = 0; chan < num_chans; chan++) {
	  /* Until the last less than 8 channels, use 8 channel encoding.
	   * In the last part, use less-than-8-channels encoding. */
	  offset = i * ( (chan/8 + 1) * 8 > num_chans ? num_chans % 8: 8) + chan / 8 * chan8blocksize + chan % 8;
#if defined(__APPLE__)      || \
    defined(__BIG_ENDIAN__) || \
    defined(WORDS_BIGENDIAN)
	  wswap((char *)&fval, (char *)&ftemp[blockpos + chan]);
	  raw->fdata[offset] = (double)(fval / gain);
#else
	  raw->fdata[offset] = (double)(ftemp[blockpos + chan] / gain);
#endif

	  /* Search for min and max */
	  min_val[chan] = min(min_val[chan], raw->fdata[offset]);
	  max_val[chan] = max(max_val[chan], raw->fdata[offset]);
	}

        blockpos += num_chans;
    }
    
    fclose(fp);

    fprintf(stderr, "gen2flac: Read %s, all of %i channels (%d samples @ %g kHz)\n",
            infilename, num_chans, i, raw->sample_rate);

    return scaledata(outfilename, min_val, max_val, num_chans);
}

int main(int argc, const char **argv) {
  const char *infilename;
  char *outfilename;
  int trace;

  if (argc < 2) {
    fprintf(stderr, "Usage: gen2flac infilename [outfilename]\n");
    return -1;
  }

  infilename = argv[1];
  if (argc > 2) 
    outfilename = (char*) argv[2];
  else {
    /* Strip extension and add new extension */
    outfilename = (char*) malloc(strlen(infilename) + strlen(GENFLAC_EXT));
    strcpy(outfilename, infilename);
    strcpy((char*) memrchr(outfilename, '.', strlen(outfilename)), GENFLAC_EXT);
    /* fprintf(stderr, "Output filename: '%s'\n", outfilename); */
  }
  /*trace = strtol(argv[3], NULL, 10);*/

  if (get_gendata(infilename, outfilename) != 0) 
    return -1;
}
