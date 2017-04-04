#ifndef __UTIMESAT_HPP__
#define __UTIMESAT_HPP__

#if __APPLE__
int
_futimesat(int dirfd, const char* path, struct timeval *tvp);
#endif

#endif
