struct ffi
{
  uint64_t  nodeid;
  uint64_t  gen;
  uid_t     uid;
  gid_t     gid;
  pid_t     pid;
  uint64_t  fh;
  char     *name;
  char      path[0];
};
