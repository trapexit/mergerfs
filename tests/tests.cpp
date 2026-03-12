#include "acutest/acutest.h"

#include "config.hpp"
#include "str.hpp"

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
  ConfigU64 v;

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
test_str_stuff()
{
  std::vector<std::string> v;
  std::set<std::string> s;
  std::pair<std::string, std::string> kv;

  // split() - vector
  v = str::split("",':');
  TEST_CHECK(v.size() == 0);

  v = str::split("a:b:c",':');
  TEST_CHECK(v.size() == 3);
  TEST_CHECK(v[0] == "a");
  TEST_CHECK(v[1] == "b");
  TEST_CHECK(v[2] == "c");

  v = str::split("a::b:c",':');
  TEST_CHECK(v.size() == 4);
  TEST_CHECK(v[0] == "a");
  TEST_CHECK(v[1] == "");
  TEST_CHECK(v[2] == "b");
  TEST_CHECK(v[3] == "c");

  v = str::split("single",':');
  TEST_CHECK(v.size() == 1);
  TEST_CHECK(v[0] == "single");

  // split_to_set()
  s = str::split_to_set("",':');
  TEST_CHECK(s.size() == 0);

  s = str::split_to_set("a:b:c",':');
  TEST_CHECK(s.size() == 3);
  TEST_CHECK(s.count("a") == 1);
  TEST_CHECK(s.count("b") == 1);
  TEST_CHECK(s.count("c") == 1);

  s = str::split_to_set("a:a:b",':');
  TEST_CHECK(s.size() == 2);
  TEST_CHECK(s.count("a") == 1);
  TEST_CHECK(s.count("b") == 1);

  // split_on_null()
  std::string nullstr = "hello";
  nullstr += '\0';
  nullstr += "world";
  nullstr += '\0';
  nullstr += "test";
  v = str::split_on_null(std::string_view(nullstr.data(), nullstr.size()));
  TEST_CHECK(v.size() == 3);
  TEST_CHECK(v[0] == "hello");
  TEST_CHECK(v[1] == "world");
  TEST_CHECK(v[2] == "test");

  v = str::split_on_null("");
  TEST_CHECK(v.size() == 0);

  // lsplit1()
  v = str::lsplit1("foo=bar=baz",'=');
  TEST_CHECK(v.size() == 2);
  TEST_CHECK(v[0] == "foo");
  TEST_CHECK(v[1] == "bar=baz");

  v = str::lsplit1("",'=');
  TEST_CHECK(v.size() == 0);

  v = str::lsplit1("no_delimiter",'=');
  TEST_CHECK(v.size() == 1);
  TEST_CHECK(v[0] == "no_delimiter");

  // rsplit1()
  v = str::rsplit1("foo=bar=baz",'=');
  TEST_CHECK(v.size() == 2);
  TEST_CHECK(v[0] == "foo=bar");
  TEST_CHECK(v[1] == "baz");

  v = str::rsplit1("",'=');
  TEST_CHECK(v.size() == 0);

  v = str::rsplit1("no_delimiter",'=');
  TEST_CHECK(v.size() == 1);
  TEST_CHECK(v[0] == "no_delimiter");

  // splitkv()
  kv = str::splitkv("key=value", '=');
  TEST_CHECK(kv.first == "key");
  TEST_CHECK(kv.second == "value");

  kv = str::splitkv("key=", '=');
  TEST_CHECK(kv.first == "key");
  TEST_CHECK(kv.second == "");

  kv = str::splitkv("key", '=');
  TEST_CHECK(kv.first == "key");
  TEST_CHECK(kv.second == "");

  kv = str::splitkv("", '=');
  TEST_CHECK(kv.first == "");
  TEST_CHECK(kv.second == "");

  // join() - vector with substr
  v = {"abc", "def", "ghi"};
  std::string joined = str::join(v, 1, ':');
  TEST_CHECK(joined == "bc:ef:hi");

  // join() - vector
  joined = str::join(v, ':');
  TEST_CHECK(joined == "abc:def:ghi");

  v = {"single"};
  joined = str::join(v, ':');
  TEST_CHECK(joined == "single");

  v = {};
  joined = str::join(v, ':');
  TEST_CHECK(joined == "");

  // join() - set
  s = {"a", "b", "c"};
  joined = str::join(s, ':');
  TEST_CHECK(joined == "a:b:c");

  s = {};
  joined = str::join(s, ':');
  TEST_CHECK(joined == "");

  // longest_common_prefix_index()
  v = {"/foo/bar", "/foo/baz", "/foo/qux"};
  TEST_CHECK(str::longest_common_prefix_index(v) == 5);

  v = {"/foo/bar"};
  TEST_CHECK(str::longest_common_prefix_index(v) == std::string::npos);

  v = {};
  TEST_CHECK(str::longest_common_prefix_index(v) == std::string::npos);

  v = {"abc", "xyz"};
  TEST_CHECK(str::longest_common_prefix_index(v) == 0);

  // longest_common_prefix()
  v = {"/foo/bar", "/foo/baz"};
  TEST_CHECK(str::longest_common_prefix(v) == "/foo/ba");

  v = {"/foo/bar"};  // Single element - returns empty string (implementation detail)
  TEST_CHECK(str::longest_common_prefix(v) == "");

  v = {};
  TEST_CHECK(str::longest_common_prefix(v) == "");

  // remove_common_prefix_and_join()
  v = {"/foo/bar", "/foo/baz", "/foo/qux"};
  joined = str::remove_common_prefix_and_join(v, ':');
  TEST_CHECK(joined == "bar:baz:qux");

  v = {};
  joined = str::remove_common_prefix_and_join(v, ':');
  TEST_CHECK(joined == "");

  // startswith() - string_view
  TEST_CHECK(str::startswith("hello world", "hello") == true);
  TEST_CHECK(str::startswith("hello world", "world") == false);
  TEST_CHECK(str::startswith("hello", "hello world") == false);
  TEST_CHECK(str::startswith("", "") == true);
  TEST_CHECK(str::startswith("hello", "") == true);

  // startswith() - char*
  TEST_CHECK(str::startswith("hello world", "hello") == true);
  TEST_CHECK(str::startswith("hello world", "world") == false);

  // endswith()
  TEST_CHECK(str::endswith("hello world", "world") == true);
  TEST_CHECK(str::endswith("hello world", "hello") == false);
  TEST_CHECK(str::endswith("hello", "hello world") == false);
  TEST_CHECK(str::endswith("", "") == true);
  TEST_CHECK(str::endswith("hello", "") == true);

  // trim()
  TEST_CHECK(str::trim("  hello  ") == "hello");
  TEST_CHECK(str::trim("hello") == "hello");
  TEST_CHECK(str::trim("   ") == "");
  TEST_CHECK(str::trim("") == "");
  TEST_CHECK(str::trim("\t\nhello\r\n") == "hello");
  TEST_CHECK(str::trim("  hello world  ") == "hello world");

  // eq()
  TEST_CHECK(str::eq("hello", "hello") == true);
  TEST_CHECK(str::eq("hello", "world") == false);
  TEST_CHECK(str::eq("", "") == true);

  // replace()
  TEST_CHECK(str::replace("hello world", ' ', '_') == "hello_world");
  TEST_CHECK(str::replace("abc", 'x', 'y') == "abc");
  TEST_CHECK(str::replace("", 'x', 'y') == "");
  TEST_CHECK(str::replace("aaa", 'a', 'b') == "bbb");

  // erase_fnmatches()
  v = {"foo.txt", "bar.cpp", "baz.h", "test.txt"};
  std::vector<std::string> patterns = {"*.txt"};
  str::erase_fnmatches(patterns, v);
  TEST_CHECK(v.size() == 2);
  // Order may vary, just check elements exist
  bool has_bar_cpp = false;
  bool has_baz_h = false;
  for(const auto& s : v) {
    if(s == "bar.cpp") has_bar_cpp = true;
    if(s == "baz.h") has_baz_h = true;
  }
  TEST_CHECK(has_bar_cpp);
  TEST_CHECK(has_baz_h);

  v = {"foo.txt", "bar.cpp"};
  patterns = {"*.txt", "*.cpp"};
  str::erase_fnmatches(patterns, v);
  TEST_CHECK(v.size() == 0);

  v = {"foo.txt", "bar.cpp"};
  patterns = {};
  str::erase_fnmatches(patterns, v);
  TEST_CHECK(v.size() == 2);
}

void
test_config_branches()
{
  uint64_t minfreespace = 1234;
  Branches b;
  b.minfreespace = minfreespace;
  Branches::Ptr bcp0;
  Branches::Ptr bcp1;

  TEST_CHECK(b->minfreespace() == 1234);
  TEST_CHECK(b.to_string() == "");

  // Parse initial value for branch
  TEST_CHECK(b.from_string(b.to_string()) == 0);

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
           1234UL,
           (*bcp0)[0].minfreespace());
  TEST_CHECK((*bcp0)[1].path == "/foo/baz");
  TEST_CHECK((*bcp0)[1].minfreespace() == 4321);
  TEST_MSG("minfreespace: expected = %lu; produced = %lu",
           4321UL,
           (*bcp0)[1].minfreespace());

  TEST_CHECK(b.from_string("foo/bar") == 0);
  TEST_CHECK(b.from_string("./foo/bar") == 0);
  TEST_CHECK(b.from_string("./foo/bar:/bar/baz:blah/asdf") == 0);
}

void
test_config_cachefiles()
{
  CacheFiles cf;

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
  TEST_CHECK(m.to_string() == "pfrd");
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

  TEST_CHECK(cfg.set("async-read","true") == 0);
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
   {"str",test_str_stuff},
   {NULL,NULL}
  };
