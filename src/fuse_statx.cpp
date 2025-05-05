#include "fs_statx.hpp"

#ifdef MERGERFS_STATX_SUPPORTED
#pragma message "statx supported"
# include "fuse_statx_supported.icpp"
#else
#pragma message "statx NOT supported"
# include "fuse_statx_unsupported.icpp"
#endif
