# Process this file with autoconf 2.0 or later to make a configure script.

# gencompress autoconf file.
# Cengiz Gunay 2007-07-17

# Inherited some stuff from Alfonso Delgado-Reyes's scripts for Xicanal
# <adelgado@biology.emory.edu>

# $Id$

# Basic stuff
AC_REVISION([$Revision$])
AC_PREREQ(2.60)
AC_INIT(gencompress, 1.0, [cengique@users.sf.net], gencompress)

# Check for programs
AC_PROG_CC
AC_PROG_INSTALL
AC_INCLUDES_DEFAULT
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h float.h malloc.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strncasecmp strrchr])

# Check for endianness
AC_C_BIGENDIAN

# ?? AM_INIT_AUTOMAKE

# Check for FlAC
AC_ARG_VAR(FLAC_EXEC, [Location of the FLAC executable.])
AC_PATH_PROG(FLAC_EXEC, flac, [N/A])
AC_DEFINE_UNQUOTED(FLAC_EXEC, $FLAC_EXEC, [Location of the FLAC executable])

# check for system
AC_CANONICAL_BUILD
AC_MSG_RESULT([System build type is: $build])

# Platform-specific CC flags
case $build in
  i?86-*-*)	PLAT_CFLAGS="-malign-double"; MEX_EXT=mexglx;; #-m32 
  x86_64-*-*)	PLAT_CFLAGS=; MEX_EXT=mexa64;;
  *)		PLAT_CFLAGS=; AC_MSG_FAILURE([System type not recognized, assuming x86_64 compatible]);;
esac
AC_SUBST(PLAT_CFLAGS)
AC_SUBST(MEX_EXT)

# AC_SUBST(PLAT_CFLAGS, "-DWIN32")
# what about windows?

AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
