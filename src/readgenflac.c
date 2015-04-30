/*
 * Matlab reader (C-MEX file) for the Genesis lossless compression 
 * scheme based on the free-lossless audio codec (FLAC).
 *
 * Copyright (c) 2006 Cengiz Gunay <cengique@users.sf.net>, 2006/03/06
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
    Adapted for general use (a routine...)
    from Dieter Jaeger's xicanal lx_genread.c
    Alfonso Delgado-Reyes 07.03.2002

    Cengiz Gunay <cgunay@emory.edu> 03.13.2004
    Fixed memory leak of not deallocating memory for the raw data buffer.
    
    o Adapted for MATLAB 5.x and 6.x under:
        - Linux x86/PPC, September 2002
        - MS Windows, September 2002
        - Mac OS 7-9, September 2002
        - Mac OS X 10.x, September 2002
    
    Any cc compiler should work ([ ] = optional)
	
	To compile:
	  In Winblows (MS Visual C++ 6.x):
		mex [-v -O] -DWIN32 -output readgenflac readgenflac.c
	  To create 64-bit binary (only on x86_64 64-bit architecture):
		mex -O -output readgenflac readgenflac.c
	  To create 32-bit binary on an i386 32-bit architecture:
		mex -O -output readgenflac readgenflac.c
	  To create 32-bit binary on a 64-bit architecture:
		mex -O CFLAGS='$CFLAGS -m32 -malign-double' -output readgenflac readgenflac.c

 * $Id$
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

#include <errno.h>
extern int errno;

#include <mex.h>
#include <mat.h>

#define ADFBLOCK 12000
#define TRUE 1

#define COMMANDNAME "readgenflac"

#include "genesis16bit.h"

int get_gendata();

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

mxArray	*scramble_data_for_matlab(struct file_info_16bit_data *unflac_data) {
  int chan8blocksize, num_chans, chan, in_offset, out_offset, i;
  struct fixed_point_16bit_format_v0_3 *file_info = unflac_data->file_info;
  unsigned short *data = unflac_data->data;
  struct fixed_point_16bit_format_chan *chan_ranges = unflac_data->chan_ranges;
  double *ddata;
  mxArray *matrix;

  matrix = mxCreateDoubleMatrix(unflac_data->file_info->num_samples,
				unflac_data->file_info->num_chans, mxREAL);
    
  if (matrix == NULL) {
    free(file_info);
    free(data);
    free(chan_ranges);
    mexErrMsgTxt("\n" COMMANDNAME ": could not create mxArray (data)");
  }
	
  ddata = (double *) mxGetPr(matrix);
    
  chan8blocksize = 8 * file_info->num_samples;
  num_chans = file_info->num_chans;

  /* now scale, and reconstruct >8 chan data */
  for (chan = 0; chan < num_chans; chan++) {
    /* Until the last less than 8 channels, use 8 channel encoding.
     * In the last part, use less-than-8-channels encoding. */
    in_offset = chan / 8 * chan8blocksize + chan % 8;
    out_offset = chan * file_info->num_samples;

    /* Scale the data and truncate to 16-bit integers */
    for (i = 0; i < file_info->num_samples; i ++) {
      ddata[i + out_offset] = chan_ranges[chan].range_low +
	data[i * ( (chan/8 + 1) * 8 > num_chans ? num_chans % 8: 8) + in_offset] *
	(chan_ranges[chan].range_high - chan_ranges[chan].range_low) / 0xFFFFU;
    }
  }

  free(file_info);
  free(data);
  free(chan_ranges);

  return matrix;
}

void mexFunction(int nlhs, mxArray *plhs[], 
		         int nrhs, const mxArray *prhs[])
{
  char		*filename;
  int		 buflen;
  struct file_info_16bit_data *read_data;
  
  if (nrhs != 1) {
    mexErrMsgTxt("\nUsage: data = " COMMANDNAME "('filename')");
  }
  else if (nlhs != 1) {
    mexErrMsgTxt("\n" COMMANDNAME " has one output argument");
  }
  
  if (mxIsChar(prhs[0]) != 1) {
    mexErrMsgTxt("\n" COMMANDNAME ": first argument must be a string");
  }
  else {
    buflen = mxGetN(prhs[0])+1;
    
    filename = (char *) mxCalloc(buflen, sizeof(mxChar));
    mxGetString(prhs[0], filename, buflen); 
  }
  
  if ((read_data = unflac_file(filename)) != NULL) {
    plhs[0] = scramble_data_for_matlab(read_data);
    free(read_data);
  } else {
    mexErrMsgTxt(msgbuf);
  }
  
  mxFree(filename);
  return;
}
