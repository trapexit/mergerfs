#include "acutest.h"

#include "config.hpp"

void
test_nop()
{
  TEST_CHECK(true);
}

void
test_config_bool()
{
  ConfigBOOL v;

  TEST_CHECK(v.from_string("true") == 0);
  TEST_CHECK(v == true);
  TEST_CHECK(v.to_string() == "true");

  TEST_CHECK(v.from_string("false") == 0);
  TEST_CHECK(v == false);
  TEST_CHECK(v.to_string() == "false");

  TEST_CHECK(v.from_string("asdf") == -EINVAL);
}

void
test_config_uint64()
{
  ConfigUINT64 v;

  TEST_CHECK(v.from_string("0") == 0);
  TEST_CHECK(v == (uint64_t)0);
  TEST_CHECK(v.to_string() == "0");

  TEST_CHECK(v.from_string("123") == 0);
  TEST_CHECK(v == (uint64_t)123);
  TEST_CHECK(v.to_string() == "123");

  TEST_CHECK(v.from_string("1234567890") == 0);
  TEST_CHECK(v == (uint64_t)1234567890);
  TEST_CHECK(v.to_string() == "1234567890");

  TEST_CHECK(v.from_string("asdf") == -EINVAL);
}

void
test_config_int()
{
  ConfigINT v;

  TEST_CHECK(v.from_string("0") == 0);
  TEST_CHECK(v == (int)0);
  TEST_CHECK(v.to_string() == "0");

  TEST_CHECK(v.from_string("123") == 0);
  TEST_CHECK(v == (int)123);
  TEST_CHECK(v.to_string() == "123");

  TEST_CHECK(v.from_string("1234567890") == 0);
  TEST_CHECK(v == (int)1234567890);
  TEST_CHECK(v.to_string() == "1234567890");

  TEST_CHECK(v.from_string("asdf") == -EINVAL);
}

void
test_config_str()
{
  ConfigSTR v;

  TEST_CHECK(v.from_string("foo") == 0);
  TEST_CHECK(v == "foo");
  TEST_CHECK(v.to_string() == "foo");
}

void
test_config_branches()
{
  uint64_t minfreespace;
  Branches b(minfreespace);
  Branches::CPtr bcp0;
  Branches::CPtr bcp1;

  minfreespace = 1234;
  TEST_CHECK(b->minfreespace() == 1234);
  TEST_CHECK(b.to_string() == "");

  bcp0 = b;
  TEST_CHECK(b.from_string("/foo/bar") == 0);
  TEST_CHECK(b.to_string() == "/foo/bar=RW");
  bcp1 = b;
  TEST_CHECK(bcp0.get() != bcp1.get());

  TEST_CHECK(b.from_string("/foo/bar=RW,1234") == 0);
  TEST_CHECK(b.to_string() == "/foo/bar=RW,1234");

  TEST_CHECK(b.from_string("/foo/bar=RO,1234:/foo/baz=NC,4321") == 0);
  TEST_CHECK(b.to_string() == "/foo/bar=RO,1234:/foo/baz=NC,4321");
  bcp0 = b;
  TEST_CHECK((*bcp0).size() == 2);
  TEST_CHECK((*bcp0)[0].path == "/foo/bar");
  TEST_CHECK((*bcp0)[0].minfreespace() == 1234);
  TEST_MSG("minfreespace: expected = %lu; produced = %lu",
           1234,
           (*bcp0)[0].minfreespace());
  TEST_CHECK((*bcp0)[1].path == "/foo/baz");
  TEST_CHECK((*bcp0)[1].minfreespace() == 4321);
  TEST_MSG("minfreespace: expected = %lu; produced = %lu",
           4321,
           (*bcp0)[1].minfreespace());

  TEST_CHECK(b.from_string("foo/bar") == 0);
  TEST_CHECK(b.from_string("./foo/bar") == 0);
  TEST_CHECK(b.from_string("./foo/bar:/bar/baz:blah/asdf") == 0);
}

void
test_config_cachefiles()
{
  CacheFiles cf;

  TEST_CHECK(cf.from_string("libfuse") == 0);
  TEST_CHECK(cf.to_string() == "libfuse");
  TEST_CHECK(cf.from_string("off") == 0);
  TEST_CHECK(cf.to_string() == "off");
  TEST_CHECK(cf.from_string("partial") == 0);
  TEST_CHECK(cf.to_string() == "partial");
  TEST_CHECK(cf.from_string("full") == 0);
  TEST_CHECK(cf.to_string() == "full");
  TEST_CHECK(cf.from_string("auto-full") == 0);
  TEST_CHECK(cf.to_string() == "auto-full");
  TEST_CHECK(cf.from_string("foobar") == -EINVAL);
}

void
test_config_inodecalc()
{
  InodeCalc ic("passthrough");

  TEST_CHECK(ic.from_string("passthrough") == 0);
  TEST_CHECK(ic.to_string() == "passthrough");
  TEST_CHECK(ic.from_string("path-hash") == 0);
  TEST_CHECK(ic.to_string() == "path-hash");
  TEST_CHECK(ic.from_string("devino-hash") == 0);
  TEST_CHECK(ic.to_string() == "devino-hash");
  TEST_CHECK(ic.from_string("hybrid-hash") == 0);
  TEST_CHECK(ic.to_string() == "hybrid-hash");
  TEST_CHECK(ic.from_string("path-hash32") == 0);
  TEST_CHECK(ic.to_string() == "path-hash32");
  TEST_CHECK(ic.from_string("devino-hash32") == 0);
  TEST_CHECK(ic.to_string() == "devino-hash32");
  TEST_CHECK(ic.from_string("hybrid-hash32") == 0);
  TEST_CHECK(ic.to_string() == "hybrid-hash32");
  TEST_CHECK(ic.from_string("asdf") == -EINVAL);
}

void
test_config_moveonenospc()
{
  MoveOnENOSPC m(false);

  TEST_CHECK(m.to_string() == "false");
  TEST_CHECK(m.from_string("mfs") == 0);
  TEST_CHECK(m.to_string() == "mfs");
  TEST_CHECK(m.from_string("mspmfs") == 0);
  TEST_CHECK(m.to_string() == "mspmfs");
  TEST_CHECK(m.from_string("true") == 0);
  TEST_CHECK(m.to_string() == "mfs");
  TEST_CHECK(m.from_string("blah") == -EINVAL);
}

void
test_config_nfsopenhack()
{
  NFSOpenHack n;

  TEST_CHECK(n.from_string("off") == 0);
  TEST_CHECK(n.to_string() == "off");
  TEST_CHECK(n == NFSOpenHack::ENUM::OFF);

  TEST_CHECK(n.from_string("git") == 0);
  TEST_CHECK(n.to_string() == "git");
  TEST_CHECK(n == NFSOpenHack::ENUM::GIT);

  TEST_CHECK(n.from_string("all") == 0);
  TEST_CHECK(n.to_string() == "all");
  TEST_CHECK(n == NFSOpenHack::ENUM::ALL);
}

void
test_config_readdir()
{
  ReadDir r;

  TEST_CHECK(r.from_string("linux") == 0);
  TEST_CHECK(r.to_string() == "linux");
  TEST_CHECK(r == ReadDir::ENUM::LINUX);

  TEST_CHECK(r.from_string("posix") == 0);
  TEST_CHECK(r.to_string() == "posix");
  TEST_CHECK(r == ReadDir::ENUM::POSIX);
}

void
test_config_statfs()
{
  StatFS s;

  TEST_CHECK(s.from_string("base") == 0);
  TEST_CHECK(s.to_string() == "base");
  TEST_CHECK(s == StatFS::ENUM::BASE);

  TEST_CHECK(s.from_string("full") == 0);
  TEST_CHECK(s.to_string() == "full");
  TEST_CHECK(s == StatFS::ENUM::FULL);

  TEST_CHECK(s.from_string("asdf") == -EINVAL);
}

void
test_config_statfs_ignore()
{
  StatFSIgnore s;

  TEST_CHECK(s.from_string("none") == 0);
  TEST_CHECK(s.to_string() == "none");
  TEST_CHECK(s == StatFSIgnore::ENUM::NONE);

  TEST_CHECK(s.from_string("ro") == 0);
  TEST_CHECK(s.to_string() == "ro");
  TEST_CHECK(s == StatFSIgnore::ENUM::RO);

  TEST_CHECK(s.from_string("nc") == 0);
  TEST_CHECK(s.to_string() == "nc");
  TEST_CHECK(s == StatFSIgnore::ENUM::NC);

  TEST_CHECK(s.from_string("asdf") == -EINVAL);
}

void
test_config_xattr()
{
  XAttr x;

  TEST_CHECK(x.from_string("passthrough") == 0);
  TEST_CHECK(x.to_string() == "passthrough");
  TEST_CHECK(x == XAttr::ENUM::PASSTHROUGH);

  TEST_CHECK(x.from_string("noattr") == 0);
  TEST_CHECK(x.to_string() == "noattr");
  TEST_CHECK(x == XAttr::ENUM::NOATTR);

  TEST_CHECK(x.from_string("nosys") == 0);
  TEST_CHECK(x.to_string() == "nosys");
  TEST_CHECK(x == XAttr::ENUM::NOSYS);

  TEST_CHECK(x.from_string("asdf") == -EINVAL);
}

void
test_config()
{
  Config cfg;

  TEST_CHECK(cfg.set_raw("async_read","true") == 0);
}

TEST_LIST =
  {
   {"nop",test_nop},
   {"config_bool",test_config_bool},
   {"config_uint64",test_config_uint64},
   {"config_int",test_config_int},
   {"config_str",test_config_str},
   {"config_branches",test_config_branches},
   {"config_cachefiles",test_config_cachefiles},
   {"config_inodecalc",test_config_inodecalc},
   {"config_moveonenospc",test_config_moveonenospc},
   {"config_nfsopenhack",test_config_nfsopenhack},
   {"config_readdir",test_config_readdir},
   {"config_statfs",test_config_statfs},
   {"config_statfsignore",test_config_statfs_ignore},
   {"config_xattr",test_config_xattr},
   {"config",test_config},
   {NULL,NULL}
  };
