/**
 * General utilities and common structures for the Genesis lossless compression 
 * scheme based on the free-lossless audio codec (FLAC).
 *
 * Copyright (c) 2006 Cengiz Gunay <cengique@users.sf.net>, 2006/03/06
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Version info:
 * Major=0: Initial file format that uses flac binary externally.
 *   Minor=1: Initial format for <=8 chan data.
 *   Minor=2: Makeshift support for >8 chan data. Works, but kludgy.
 *   Minor=3: Fixed 64/32-bit incompat due to 'long' value in header. added magic number.
 * Proposed:
 * Major=1: Use FLAC API and create a new format. Maybe embed in FLAC envelope? 
 *   	    Register 'NeSA' as signature at FlaC site.
 *
 * $Id: genesis16bit.h,v 1.6 2007/01/09 00:11:50 cengiz Exp $
 */
#include <errno.h>

#define BUFSIZE 16384

#define NESA_MAGIC "NeSA"

#ifdef __GLIBC__
/* For systems with GLIBC */
typedef long long int __int64;
typedef unsigned long long int __uint64;
#endif

/** Headers for 16-bit fixed-point integer representation raw data format. */

/** Backwards compatibility headers */
struct fixed_point_16bit_format_v0_2_64bit {
  /* This is the magic number at the beginning to detect versions */
  unsigned int		magic_number, version_major, version_minor;	
  unsigned short 	header_len;
  unsigned int		sample_rate; 
  __uint64		num_samples;
  unsigned short	num_chans;	
  unsigned char 	is_compressed; 
};

/** Corrected for 32/64 bit portability. num_samples=32 bit. 
 * Gives >1000 hours of recording at 100kHz sample-rate. Hopefully enough for near-future. */
struct fixed_point_16bit_format_v0_3 {
  /* This is the magic number at the beginning to detect versions */
  unsigned int		magic_number, version_major, version_minor;	
  unsigned short 	header_len/* = sizeof(struct fixed_point_16bit_format)*/;
  unsigned int		sample_rate;	/* In kHz */
  unsigned int		num_samples;	/* Number of samples per channel */
  unsigned short	num_chans;	/* Number of interleaved channels */
  unsigned char 	is_compressed;  /* Is data in compressed form? */
};

struct fixed_point_16bit_format_chan {
  double		range_low, range_high;
};

/** The disk-out object format as explained in the Genesis manual */
struct genesis_disk_out_format {
  char label[80];
  float start_time, dt;
  /* datatype: SHORT = 2, INT = 3, FLOAT = 4 (*default), DOUBLE = 5 */
  int num_chans, datatype;
  /* Followed by  (3 * num_chans) floats and then data */
};

struct file_info_16bit_data {
  struct fixed_point_16bit_format_v0_3 *file_info;
  struct fixed_point_16bit_format_chan *chan_ranges;
  short *data;
};


/** Get system-dependent parameters for Flac binary based on its version. */
char *get_flac_version_opts() {
  char versionstring[BUFSIZE];
  FILE *flacpipe;
  int subminor;
  char *retval;
  
  if ((retval = malloc(50)) == NULL) {
    fprintf(stderr, "\n" COMMANDNAME ": could not alloc memory.");
    exit(-1);
  }
  strcpy(retval, "\0");

  /* Call flac to get its version */
  if ((flacpipe = popen("flac -vs", "r")) == NULL) {
    fprintf(stderr, "\n" COMMANDNAME ": could not call Flac. Is it even installed?");
    exit(-1);
  }  

  /* read output from pipe */
  if (fgets(versionstring, BUFSIZE, flacpipe) == NULL) {
    fprintf(stderr, "\n" COMMANDNAME ": could not read output of Flac command.");
    exit(-1);
  }
  pclose(flacpipe);

  /*fprintf(stderr, "\n" COMMANDNAME ": Version '%s'\n", versionstring);*/

  /* sanity check */
  if (strncasecmp(versionstring, "flac", 4) == 0) {
    /*fprintf(stderr, "\n" COMMANDNAME ": found flac\n");*/
    /* version number */
    if (strncmp(&versionstring[5], "1.1.", 4) == 0) {
      subminor = atoi(&versionstring[9]);
      /*fprintf(stderr, "\n" COMMANDNAME ": Version subminor %d\n", subminor);*/
      if (subminor > 0)
	strcpy(retval, "-f");
    }
  }
  
  return retval;
}

struct file_info_16bit_data *unflac_file(const char *infilename) {
  int num_items, i, chan, testlen, filelen, chan8blocksize, offset, num_chans;
  FILE *fp, *tfp;
  unsigned short *data;
  struct fixed_point_16bit_format_v0_3 *file_info;
  struct fixed_point_16bit_format_v0_2_64bit *file_info_old;
  struct fixed_point_16bit_format_chan *chan_ranges;
  struct file_info_16bit_data *read_data;
  char template[BUFSIZE] = "/tmp/genesis_uncomp_XXXXXX\0";
  int fd;
  char commandbuffer[BUFSIZE];
  char *extraopts;

  if ((file_info = (struct fixed_point_16bit_format_v0_3 *)
       malloc(sizeof(struct fixed_point_16bit_format_v0_3))) == NULL) {
    fprintf(stderr, "\n" COMMANDNAME ": could not malloc data structure\n");
    return(NULL);
  }

  /* Open the file */
  if ((fp = fopen(infilename, "rb")) == NULL) {
    fprintf(stderr, "\n" COMMANDNAME ": could not read file '%s'\n", infilename);
    free(file_info);
    return(NULL);
  }

  /* Read file header */
  if ((num_items = fread(file_info, sizeof(*file_info), 1, fp)) != 1) {
    fprintf(stderr, "\n" COMMANDNAME ": could not read header from file '%s'\n",
	    infilename);
    free(file_info);
    return(NULL);
  }

  /* Handle broken header */
  if (file_info->version_major == 0 && file_info->version_major <= 2) {
    /* guess the architecture of generating computer 
       [reading arch doesn't matter, originating arch matters]
       Only problem is translating 64-bit long value in the 64-bit created header */
    if (! file_info->num_samples) {
      printf("Converting broken 64-bit header...\n");
      fseek(fp, 0L, SEEK_SET);
      /* Read file header */
      if (((file_info_old = (struct fixed_point_16bit_format_v0_2_64bit *)
	    malloc(sizeof(struct fixed_point_16bit_format_v0_2_64bit))) == NULL) || 
	  ((num_items = fread(file_info_old, sizeof(*file_info_old), 1, fp)) != 1)) {
	fprintf(stderr, "\n" COMMANDNAME ": could not malloc or read old data structure\n");
	return(NULL);
      }
      /* Transfer values to the correct data structure */
      file_info->sample_rate = file_info_old->sample_rate;
      file_info->num_samples = file_info_old->num_samples;
      file_info->num_chans = file_info_old->num_chans;
      file_info->is_compressed = file_info_old->is_compressed;
      free(file_info_old);
    }

    /*printf("size old: %d (%d), size new: %d, ", sizeof(*file_info_old),
	   sizeof(file_info_old->num_samples), 
	   sizeof(struct fixed_point_16bit_format_v0_3));
    printf("sample_rate=%d, ", ((struct fixed_point_16bit_format_v0_3*)file_info)->sample_rate);
    printf("num_chans=0x%hX, ", ((struct fixed_point_16bit_format_v0_3*)file_info)->num_chans);
    printf("num_samples=%d\n", ((struct fixed_point_16bit_format_v0_3*)file_info)->num_samples);*/
  }

  if ((chan_ranges = (struct fixed_point_16bit_format_chan *)
       malloc(file_info->num_chans * sizeof(struct fixed_point_16bit_format_chan))) == NULL) {
    fprintf(stderr, "\n" COMMANDNAME ": could not malloc data structure\n");
    return(NULL);
  }

  /* Read channel header */
  if ((num_items = fread(chan_ranges, file_info->num_chans * sizeof(*chan_ranges), 1, fp)) != 1) {
    fprintf(stderr, "\n" COMMANDNAME ": could not read channels from file '%s'\n",
	    infilename);
    free(chan_ranges);
    free(file_info);
    return(NULL);
    }

  fseek(fp, 0L, SEEK_END);
  filelen = ftell(fp);
  fclose(fp);

  /* Create temp file for uncompressing */
  if ((fd = mkstemp(template)) == -1 || (tfp = fdopen(fd, "rb")) == NULL) {
    fprintf(stderr, "\n" COMMANDNAME ": could not create temporary file '%s'\n", template);
    free(chan_ranges);
    free(file_info);
    return(NULL);
  }

  extraopts = get_flac_version_opts();

  /* Call decompressor to read from infile and write to tempfile */
  testlen = sprintf(commandbuffer,
		    "tail -c%d %s | flac -s -d --force-raw-format --endian=little "
		    "--sign=unsigned %s - -o %s",
		    filelen - file_info->header_len, infilename, extraopts, template); 
  assert(testlen <= BUFSIZE);
  if (system(commandbuffer) != 0) {
    fprintf(stderr, "\n" COMMANDNAME ": could not execute decompressor command:\n%s\n",
	    commandbuffer);
    free(chan_ranges);
    free(file_info);
    return(NULL);
  }

  if ((data = (short int *)malloc(file_info->num_samples * file_info->num_chans *
				  sizeof(short int))) == NULL) {
    fprintf(stderr, "\n" COMMANDNAME ": could not malloc data array of %d bytes.\n",
	    file_info->num_samples * file_info->num_chans * sizeof(short int));
    free(chan_ranges);
    free(file_info);
    return(NULL);
  }

  /* Read the data from temp file */
  if ((num_items = fread(data, file_info->num_samples * file_info->num_chans *
			 sizeof(unsigned short), 1, tfp)) != 1) {
    fprintf(stderr, "\n" COMMANDNAME ": could not read data from file '%s'.\n",
	    template);
    if (feof(tfp)) 
      fprintf(stderr, "End of file reached.\n");
    free(chan_ranges);
    free(file_info);
    free(data);
    return(NULL);
    }

  /* Close temp file */
  fclose(tfp);

  /* Remove temp files */
  if (remove(template) != 0) {
    fprintf(stderr, "\n" COMMANDNAME ": could not delete the temporary file: %s\n",
	    template);
  }

  if ((read_data = (struct file_info_16bit_data *) malloc(sizeof(struct file_info_16bit_data))) == NULL) {
    fprintf(stderr, "\n" COMMANDNAME ": could not malloc data structure\n");
    free(chan_ranges);
    free(file_info);
    free(data);
    return(NULL);
  }
  
  read_data->file_info = file_info;
  read_data->chan_ranges = chan_ranges;
  read_data->data = data;

  return read_data;
}
