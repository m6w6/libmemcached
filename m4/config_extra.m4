# SYNOPSIS
#
#   CONFIG_EXTRA
#
# DESCRIPTION
#
#   Adds required extras to config.h
#
# LICENSE
#
#  Copyright (C) 2014 Andrew Hutchings
#  Based on bottom.m4 from libdrizzle
#
#  All rights reserved.
#  
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#
#      * Redistributions of source code must retain the above copyright
#  notice, this list of conditions and the following disclaimer.
#
#      * Redistributions in binary form must reproduce the above
#  copyright notice, this list of conditions and the following disclaimer
#  in the documentation and/or other materials provided with the
#  distribution.
#
#      * The names of its contributors may not be used to endorse or
#  promote products derived from this software without specific prior
#  written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#serial 1

AC_DEFUN([CONFIG_EXTRA], [

AH_TOP([
#pragma once

/* _SYS_FEATURE_TESTS_H is Solaris, _FEATURES_H is GCC */
#if defined( _SYS_FEATURE_TESTS_H) || defined(_FEATURES_H)
# error "You should include config.h as your first include file"
#endif

])

AH_BOTTOM([

#ifndef __STDC_FORMAT_MACROS
#  define __STDC_FORMAT_MACROS
#endif

#if defined(__cplusplus)
#  include CINTTYPES_H
#else
#  include <inttypes.h>
#endif

#if !defined(HAVE_ULONG) && !defined(__USE_MISC)
# define HAVE_ULONG 1
typedef unsigned long int ulong;
#endif

])

])dnl CONFIG_EXTRA