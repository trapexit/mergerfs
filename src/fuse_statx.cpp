#include "fs_statx.hpp"

#ifdef MERGERFS_SUPPORTED_STATX
#pragma message "statx supported"
# include "fuse_statx_supported.icpp"
#else
#pragma message "statx NOT supported"
# include "fuse_statx_unsupported.icpp"
#endif
