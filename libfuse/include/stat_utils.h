#pragma once

#include "config.h"

#ifdef HAVE_STRUCT_STAT_ST_ATIM
#define ST_ATIM_NSEC(ST) ((ST)->st_atim.tv_nsec)
#define ST_CTIM_NSEC(ST) ((ST)->st_ctim.tv_nsec)
#define ST_MTIM_NSEC(ST) ((ST)->st_mtim.tv_nsec)
#elif defined HAVE_STRUCT_STAT_ST_ATIMESPEC
#define ST_ATIM_NSEC(ST) ((ST)->st_atimespec.tv_nsec)
#define ST_CTIM_NSEC(ST) ((ST)->st_ctimespec.tv_nsec)
#define ST_MTIM_NSEC(ST) ((ST)->st_mtimespec.tv_nsec)
#else
#define ST_ATIM_NSEC(ST) 0
#define ST_CTIM_NSEC(ST) 0
#define ST_MTIM_NSEC(ST) 0
#endif
