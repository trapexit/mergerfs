#include "acutest/acutest.h"

#include "config.hpp"
#include "fs_copyfile.hpp"
#include "fs_inode.hpp"
#include "from_string.hpp"
#include "hashset.hpp"
#include "num.hpp"
#include "rapidhash/rapidhash.h"
#include "rnd.hpp"
#include "str.hpp"
#include "thread_pool.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <future>
#include <mutex>
#include <numeric>
#include <optional>
#include <sstream>
#include <thread>

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

template<typename Predicate>
bool
wait_until(Predicate pred_,
           const int timeout_ms_ = 2000)
{
  for(int i = 0; i < timeout_ms_; ++i)
    {
      if(pred_())
        return true;

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

  return pred_();
}

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

// ---------------------------------------------------------------------------
// Branch unit tests
// ---------------------------------------------------------------------------

void
test_branch_default_ctor_mode()
{
  Branch b;

  TEST_CHECK(b.mode == Branch::Mode::RW);
}

void
test_branch_to_string_modes()
{
  uint64_t global = 0;
  Branch b(global);

  b.path = "/mnt/disk1";

  b.mode = Branch::Mode::RW;
  TEST_CHECK(b.to_string() == "/mnt/disk1=RW");

  b.mode = Branch::Mode::RO;
  TEST_CHECK(b.to_string() == "/mnt/disk1=RO");

  b.mode = Branch::Mode::NC;
  TEST_CHECK(b.to_string() == "/mnt/disk1=NC");
}

void
test_branch_to_string_with_minfreespace()
{
  uint64_t global = 0;
  Branch b(global);

  b.path = "/mnt/disk1";
  b.mode = Branch::Mode::RW;

  // While minfreespace is held by pointer (global default), it is NOT emitted
  TEST_CHECK(b.to_string() == "/mnt/disk1=RW");

  // Once set_minfreespace is called the value is stored locally and IS emitted
  b.set_minfreespace(1024);
  TEST_CHECK(b.to_string() == "/mnt/disk1=RW,1K");

  b.set_minfreespace(1024 * 1024);
  TEST_CHECK(b.to_string() == "/mnt/disk1=RW,1M");

  b.set_minfreespace(1024 * 1024 * 1024);
  TEST_CHECK(b.to_string() == "/mnt/disk1=RW,1G");

  b.set_minfreespace(500);
  TEST_CHECK(b.to_string() == "/mnt/disk1=RW,500");
}

void
test_branch_mode_predicates()
{
  Branch b;

  b.mode = Branch::Mode::RW;
  TEST_CHECK(b.ro()       == false);
  TEST_CHECK(b.nc()       == false);
  TEST_CHECK(b.ro_or_nc() == false);

  b.mode = Branch::Mode::RO;
  TEST_CHECK(b.ro()       == true);
  TEST_CHECK(b.nc()       == false);
  TEST_CHECK(b.ro_or_nc() == true);

  b.mode = Branch::Mode::NC;
  TEST_CHECK(b.ro()       == false);
  TEST_CHECK(b.nc()       == true);
  TEST_CHECK(b.ro_or_nc() == true);
}

void
test_branch_minfreespace_pointer_vs_value()
{
  uint64_t global = 9999;
  Branch b(global);

  // Reads through the pointer
  TEST_CHECK(b.minfreespace() == 9999);

  // Pointer tracks changes to the referenced global
  global = 1234;
  TEST_CHECK(b.minfreespace() == 1234);

  // set_minfreespace switches variant to owned u64
  b.set_minfreespace(42);
  TEST_CHECK(b.minfreespace() == 42);

  // Global changes no longer affect it
  global = 9999;
  TEST_CHECK(b.minfreespace() == 42);
}

void
test_branch_copy_constructor()
{
  uint64_t global = 100;
  Branch orig(global);
  orig.path = "/orig";
  orig.mode = Branch::Mode::RO;

  Branch copy(orig);
  TEST_CHECK(copy.path == "/orig");
  TEST_CHECK(copy.mode == Branch::Mode::RO);
  TEST_CHECK(copy.minfreespace() == 100);
}

void
test_branch_copy_assignment()
{
  uint64_t global = 77;
  Branch a(global);
  a.path = "/a";
  a.mode = Branch::Mode::NC;
  a.set_minfreespace(555);

  Branch b;
  b = a;
  TEST_CHECK(b.path == "/a");
  TEST_CHECK(b.mode == Branch::Mode::NC);
  TEST_CHECK(b.minfreespace() == 555);
}

// ---------------------------------------------------------------------------
// Branches unit tests
// ---------------------------------------------------------------------------

void
test_branches_default_minfreespace()
{
  Branches b;

  // Default minfreespace propagates into each parsed branch
  b.minfreespace = 8192;
  TEST_CHECK(b.from_string("/tmp/a") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p)[0].minfreespace() == 8192);
}

void
test_branches_per_branch_minfreespace_overrides_default()
{
  Branches b;
  b.minfreespace = 1000;

  TEST_CHECK(b.from_string("/tmp/a=RW,2000") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p)[0].minfreespace() == 2000);
  TEST_MSG("expected 2000, got %lu", (unsigned long)(*p)[0].minfreespace());
}

void
test_branches_add_end_operator()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a") == 0);
  TEST_CHECK(b->size() == 1);

  // + appends to end
  TEST_CHECK(b.from_string("+/tmp/b") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 2);
  TEST_CHECK((*p)[0].path == "/tmp/a");
  TEST_CHECK((*p)[1].path == "/tmp/b");

  // +> also appends to end
  TEST_CHECK(b.from_string("+>/tmp/c") == 0);
  p = b;
  TEST_CHECK((*p).size() == 3);
  TEST_CHECK((*p)[2].path == "/tmp/c");
}

void
test_branches_add_begin_operator()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a") == 0);
  TEST_CHECK(b.from_string("+</tmp/b") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 2);
  TEST_CHECK((*p)[0].path == "/tmp/b");
  TEST_CHECK((*p)[1].path == "/tmp/a");
}

void
test_branches_set_operator()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a:/tmp/b") == 0);
  TEST_CHECK(b->size() == 2);

  // = replaces the whole list
  TEST_CHECK(b.from_string("=/tmp/c") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 1);
  TEST_CHECK((*p)[0].path == "/tmp/c");
}

void
test_branches_erase_end_operator()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a:/tmp/b:/tmp/c") == 0);
  TEST_CHECK(b.from_string("->") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 2);
  TEST_CHECK((*p).back().path == "/tmp/b");
}

void
test_branches_erase_begin_operator()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a:/tmp/b:/tmp/c") == 0);
  TEST_CHECK(b.from_string("-<") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 2);
  TEST_CHECK((*p).front().path == "/tmp/b");
}

void
test_branches_erase_fnmatch_operator()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/disk1:/tmp/disk2:/mnt/ssd") == 0);

  // Remove branches matching /tmp/*
  TEST_CHECK(b.from_string("-/tmp/*") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 1);
  TEST_CHECK((*p)[0].path == "/mnt/ssd");
}

void
test_branches_erase_fnmatch_multiple_patterns()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a:/tmp/b:/mnt/c:/mnt/d") == 0);
  TEST_CHECK(b.from_string("-/tmp/a:/mnt/d") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 2);
  TEST_CHECK((*p)[0].path == "/tmp/b");
  TEST_CHECK((*p)[1].path == "/mnt/c");
}

void
test_branches_invalid_mode()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a=XX") == -EINVAL);
}

void
test_branches_invalid_minfreespace()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a=RW,notanumber") == -EINVAL);
}

void
test_branches_to_paths()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a:/tmp/b:/tmp/c") == 0);
  Branches::Ptr p = b;

  std::vector<fs::path> paths = (*p).to_paths();
  TEST_CHECK(paths.size() == 3);
  TEST_CHECK(paths[0] == "/tmp/a");
  TEST_CHECK(paths[1] == "/tmp/b");
  TEST_CHECK(paths[2] == "/tmp/c");

  StrVec sv;
  (*p).to_paths(sv);
  TEST_CHECK(sv.size() == 3);
  TEST_CHECK(sv[0] == "/tmp/a");
  TEST_CHECK(sv[1] == "/tmp/b");
  TEST_CHECK(sv[2] == "/tmp/c");
}

void
test_branches_cow_semantics()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a") == 0);
  Branches::Ptr snap0 = b;

  TEST_CHECK(b.from_string("+/tmp/b") == 0);
  Branches::Ptr snap1 = b;

  // Each mutation produces a new Impl pointer
  TEST_CHECK(snap0.get() != snap1.get());

  // snap0 is unchanged
  TEST_CHECK((*snap0).size() == 1);
  TEST_CHECK((*snap1).size() == 2);
}

void
test_branches_mode_round_trip()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a=RO:/tmp/b=NC:/tmp/c=RW") == 0);
  TEST_CHECK(b.to_string() == "/tmp/a=RO:/tmp/b=NC:/tmp/c=RW");

  Branches::Ptr p = b;
  TEST_CHECK((*p)[0].mode == Branch::Mode::RO);
  TEST_CHECK((*p)[1].mode == Branch::Mode::NC);
  TEST_CHECK((*p)[2].mode == Branch::Mode::RW);
}

void
test_branches_srcmounts_to_string()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a=RO,1234:/tmp/b=NC") == 0);

  SrcMounts sm(b);
  // SrcMounts::to_string emits only paths, no mode or minfreespace
  TEST_CHECK(sm.to_string() == "/tmp/a:/tmp/b");
}

void
test_branches_srcmounts_empty()
{
  Branches b;
  SrcMounts sm(b);

  TEST_CHECK(sm.to_string() == "");
}

// ---------------------------------------------------------------------------
// Branch::to_string humanization edge cases
// ---------------------------------------------------------------------------

void
test_branch_to_string_1t()
{
  uint64_t global = 0;
  Branch b(global);
  b.path = "/mnt/disk1";
  b.mode = Branch::Mode::RW;

  b.set_minfreespace(1024ULL * 1024 * 1024 * 1024);
  TEST_CHECK(b.to_string() == "/mnt/disk1=RW,1T");

  b.set_minfreespace(2ULL * 1024 * 1024 * 1024 * 1024);
  TEST_CHECK(b.to_string() == "/mnt/disk1=RW,2T");
}

void
test_branch_to_string_non_exact_multiple()
{
  // 1536 = 1.5K: not divisible by any tier, emitted as raw decimal
  uint64_t global = 0;
  Branch b(global);
  b.path = "/mnt/disk1";
  b.mode = Branch::Mode::RW;

  b.set_minfreespace(1536);
  TEST_CHECK(b.to_string() == "/mnt/disk1=RW,1536");

  // 1537 also not exact
  b.set_minfreespace(1537);
  TEST_CHECK(b.to_string() == "/mnt/disk1=RW,1537");
}

void
test_branch_to_string_zero_owned_minfreespace()
{
  // Default Branch() holds u64(0) in variant — to_string WILL emit ",0"
  Branch b;
  b.path = "/mnt/disk1";
  b.mode = Branch::Mode::RW;

  TEST_CHECK(b.to_string() == "/mnt/disk1=RW,0");
}

void
test_branch_copy_pointer_aliasing()
{
  // A copy of a pointer-backed Branch shares the same pointer —
  // mutating the original global value is seen by both.
  uint64_t global = 1024;
  Branch orig(global);
  orig.path = "/mnt/a";
  orig.mode = Branch::Mode::RW;

  Branch copy = orig;

  TEST_CHECK(orig.minfreespace() == 1024);
  TEST_CHECK(copy.minfreespace() == 1024);

  // Both see the updated value through the shared pointer
  global = 2048;
  TEST_CHECK(orig.minfreespace() == 2048);
  TEST_CHECK(copy.minfreespace() == 2048);
}

// ---------------------------------------------------------------------------
// parse_mode: case-sensitivity
// ---------------------------------------------------------------------------

void
test_branches_mode_case_sensitivity()
{
  Branches b;

  // Only exact uppercase is accepted
  TEST_CHECK(b.from_string("/tmp/a=rw") == -EINVAL);
  TEST_CHECK(b.from_string("/tmp/a=ro") == -EINVAL);
  TEST_CHECK(b.from_string("/tmp/a=nc") == -EINVAL);
  TEST_CHECK(b.from_string("/tmp/a=Rw") == -EINVAL);
  TEST_CHECK(b.from_string("/tmp/a=RW") == 0);
}

// ---------------------------------------------------------------------------
// parse_minfreespace: suffix variants
// ---------------------------------------------------------------------------

void
test_branches_minfreespace_lowercase_suffixes()
{
  Branches b;
  Branches::Ptr p;

  // Lowercase k/m/g/t must be accepted and multiply correctly
  TEST_CHECK(b.from_string("/tmp/a=RW,1k") == 0);
  p = b;
  TEST_CHECK((*p)[0].minfreespace() == 1024ULL);

  TEST_CHECK(b.from_string("/tmp/a=RW,1m") == 0);
  p = b;
  TEST_CHECK((*p)[0].minfreespace() == 1024ULL * 1024);

  TEST_CHECK(b.from_string("/tmp/a=RW,1g") == 0);
  p = b;
  TEST_CHECK((*p)[0].minfreespace() == 1024ULL * 1024 * 1024);

  TEST_CHECK(b.from_string("/tmp/a=RW,1t") == 0);
  p = b;
  TEST_CHECK((*p)[0].minfreespace() == 1024ULL * 1024 * 1024 * 1024);
}

void
test_branches_minfreespace_bytes_suffix()
{
  Branches b;
  Branches::Ptr p;

  // 'B' / 'b' suffix means bytes (no multiplication)
  TEST_CHECK(b.from_string("/tmp/a=RW,1024B") == 0);
  p = b;
  TEST_CHECK((*p)[0].minfreespace() == 1024ULL);

  TEST_CHECK(b.from_string("/tmp/a=RW,1024b") == 0);
  p = b;
  TEST_CHECK((*p)[0].minfreespace() == 1024ULL);
}

void
test_branches_minfreespace_zero()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a=RW,0") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p)[0].minfreespace() == 0ULL);
  // to_string should reproduce the zero
  TEST_CHECK(p->to_string() == "/tmp/a=RW,0");
}

void
test_branches_minfreespace_negative_invalid()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a=RW,-1") == -EINVAL);
}

void
test_branches_minfreespace_overflow()
{
  Branches b;

  // 16777216T overflows u64 (16777216 * 2^40 > 2^64-1)
  TEST_CHECK(b.from_string("/tmp/a=RW,16777216T") == -EOVERFLOW);
}

// ---------------------------------------------------------------------------
// Minfreespace round-trip through Branches
// ---------------------------------------------------------------------------

void
test_branches_minfreespace_round_trip()
{
  Branches b;

  // Parse 1K — should store 1024, to_string should emit "1K"
  TEST_CHECK(b.from_string("/tmp/a=RW,1K") == 0);
  {
    Branches::Ptr p = b;
    TEST_CHECK((*p)[0].minfreespace() == 1024ULL);
    TEST_CHECK(p->to_string() == "/tmp/a=RW,1K");

    // Re-parsing the output should produce the same state
    TEST_CHECK(b.from_string(p->to_string()) == 0);
  }
  {
    Branches::Ptr p = b;
    TEST_CHECK((*p)[0].minfreespace() == 1024ULL);
  }
}

void
test_branches_minfreespace_round_trip_1t()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a=RW,1T") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p)[0].minfreespace() == 1024ULL * 1024 * 1024 * 1024);
  TEST_CHECK(p->to_string() == "/tmp/a=RW,1T");
}

// ---------------------------------------------------------------------------
// Instruction dispatch edge cases
// ---------------------------------------------------------------------------

void
test_branches_clear_with_equals_alone()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a:/tmp/b") == 0);
  TEST_CHECK(b->size() == 2);

  // "=" alone clears the list
  TEST_CHECK(b.from_string("=") == 0);
  TEST_CHECK(b->empty());
}

void
test_branches_empty_string_clears_same_as_equals()
{
  // "" and "=" both dispatch to l::set — should produce identical result
  Branches b1, b2;

  TEST_CHECK(b1.from_string("/tmp/a") == 0);
  TEST_CHECK(b2.from_string("/tmp/a") == 0);

  TEST_CHECK(b1.from_string("=") == 0);
  TEST_CHECK(b2.from_string("") == 0);

  TEST_CHECK(b1->empty());
  TEST_CHECK(b2->empty());
}

void
test_branches_plus_and_plus_greater_equivalent()
{
  // "+" and "+>" both dispatch to l::add_end
  Branches b1, b2;

  TEST_CHECK(b1.from_string("/tmp/a") == 0);
  TEST_CHECK(b2.from_string("/tmp/a") == 0);

  TEST_CHECK(b1.from_string("+/tmp/b") == 0);
  TEST_CHECK(b2.from_string("+>/tmp/b") == 0);

  TEST_CHECK(b1->to_string() == b2->to_string());
}

void
test_branches_unknown_instruction_einval()
{
  Branches b;

  // "++" is not a recognized instruction
  TEST_CHECK(b.from_string("++-/tmp/a") == -EINVAL);

  // "+-" is not recognized either
  TEST_CHECK(b.from_string("+-/tmp/a") == -EINVAL);
}

// ---------------------------------------------------------------------------
// parse_branch edge cases
// ---------------------------------------------------------------------------

void
test_branches_empty_options_after_equals_einval()
{
  Branches b;

  // "/tmp/a=" — empty options field (no mode) is invalid
  TEST_CHECK(b.from_string("/tmp/a=") == -EINVAL);
}

void
test_branches_path_with_equals_in_name()
{
  Branches b;

  // rsplit1 splits on the LAST '=' so "/path/with=sign=RW" works
  TEST_CHECK(b.from_string("/tmp/with=equals=RW") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p)[0].path == "/tmp/with=equals");
  TEST_CHECK((*p)[0].mode == Branch::Mode::RW);
}

// ---------------------------------------------------------------------------
// add_begin / add_end with multiple colon-separated paths
// ---------------------------------------------------------------------------

void
test_branches_add_end_multiple_paths()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a") == 0);

  // "+>" with two paths should append both in order
  TEST_CHECK(b.from_string("+>/tmp/b:/tmp/c") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 3);
  TEST_CHECK((*p)[0].path == "/tmp/a");
  TEST_CHECK((*p)[1].path == "/tmp/b");
  TEST_CHECK((*p)[2].path == "/tmp/c");
}

void
test_branches_add_begin_multiple_paths()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a") == 0);

  // "+<" with two paths should prepend both in order
  TEST_CHECK(b.from_string("+</tmp/b:/tmp/c") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 3);
  TEST_CHECK((*p)[0].path == "/tmp/b");
  TEST_CHECK((*p)[1].path == "/tmp/c");
  TEST_CHECK((*p)[2].path == "/tmp/a");
}

// ---------------------------------------------------------------------------
// erase_fnmatch edge cases
// ---------------------------------------------------------------------------

void
test_branches_erase_empty_pattern_noop()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a:/tmp/b") == 0);

  // "-" alone (empty pattern) — no-op, list unchanged
  TEST_CHECK(b.from_string("-") == 0);
  TEST_CHECK(b->size() == 2);
}

void
test_branches_erase_wildcard_removes_all()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a:/tmp/b:/tmp/c") == 0);

  // "-*" — '*' matches everything (no FNM_PATHNAME, so '*' matches '/')
  TEST_CHECK(b.from_string("-*") == 0);
  TEST_CHECK(b->empty());
}

void
test_branches_erase_nonmatching_pattern_noop()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a:/tmp/b") == 0);

  // Pattern that matches nothing — list unchanged
  TEST_CHECK(b.from_string("-/mnt/*") == 0);
  TEST_CHECK(b->size() == 2);
}

void
test_branches_erase_multi_pattern_first_miss_second_hit()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a:/mnt/b") == 0);

  // First pattern matches nothing, second matches /mnt/b
  TEST_CHECK(b.from_string("-/nonexist/*:/mnt/*") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 1);
  TEST_CHECK((*p)[0].path == "/tmp/a");
}

// ---------------------------------------------------------------------------
// erase_begin / erase_end on single-element list
// ---------------------------------------------------------------------------

void
test_branches_erase_end_single_element()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a") == 0);
  TEST_CHECK(b.from_string("->") == 0);
  TEST_CHECK(b->empty());
}

void
test_branches_erase_begin_single_element()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a") == 0);
  TEST_CHECK(b.from_string("-<") == 0);
  TEST_CHECK(b->empty());
}

void
test_branches_erase_end_empty_returns_enoent()
{
  Branches b;

  TEST_CHECK(b.from_string("->") == -ENOENT);
  TEST_CHECK(b->empty());
}

void
test_branches_erase_begin_empty_returns_enoent()
{
  Branches b;

  TEST_CHECK(b.from_string("-<") == -ENOENT);
  TEST_CHECK(b->empty());
}

// ---------------------------------------------------------------------------
// Operation sequences
// ---------------------------------------------------------------------------

void
test_branches_clear_then_add()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a:/tmp/b") == 0);
  TEST_CHECK(b.from_string("=") == 0);
  TEST_CHECK(b->empty());

  TEST_CHECK(b.from_string("+/tmp/c") == 0);
  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 1);
  TEST_CHECK((*p)[0].path == "/tmp/c");
}

void
test_branches_add_begin_then_end_then_erase_both()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/b") == 0);
  TEST_CHECK(b.from_string("+</tmp/a") == 0);  // prepend
  TEST_CHECK(b.from_string("+>/tmp/c") == 0);  // append
  {
    Branches::Ptr p = b;
    TEST_CHECK((*p).size() == 3);
    TEST_CHECK((*p)[0].path == "/tmp/a");
    TEST_CHECK((*p)[1].path == "/tmp/b");
    TEST_CHECK((*p)[2].path == "/tmp/c");
  }

  TEST_CHECK(b.from_string("-<") == 0);  // remove first
  TEST_CHECK(b.from_string("->") == 0);  // remove last
  {
    Branches::Ptr p = b;
    TEST_CHECK((*p).size() == 1);
    TEST_CHECK((*p)[0].path == "/tmp/b");
  }
}

void
test_branches_error_in_multipath_add_leaves_list_unchanged()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a") == 0);

  // Second path has invalid mode — the whole add should fail atomically
  TEST_CHECK(b.from_string("+/tmp/b:/tmp/c=XX") == -EINVAL);

  // Original list unchanged
  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 1);
  TEST_CHECK((*p)[0].path == "/tmp/a");
}

void
test_branches_error_in_set_leaves_list_unchanged()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a") == 0);

  // Invalid set — list should be unchanged
  TEST_CHECK(b.from_string("=/tmp/b=BADMODE") == -EINVAL);

  Branches::Ptr p = b;
  TEST_CHECK((*p).size() == 1);
  TEST_CHECK((*p)[0].path == "/tmp/a");
}

// ---------------------------------------------------------------------------
// Branches::minfreespace live tracking
// ---------------------------------------------------------------------------

void
test_branches_global_minfreespace_live_tracking()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a") == 0);

  // Branch uses pointer to Branches::minfreespace — tracks it live
  uint64_t initial = b.minfreespace;
  {
    Branches::Ptr p = b;
    TEST_CHECK((*p)[0].minfreespace() == initial);
  }

  b.minfreespace = 999999;
  {
    Branches::Ptr p = b;
    TEST_CHECK((*p)[0].minfreespace() == 999999);
  }
}

// ---------------------------------------------------------------------------
// SrcMounts: from_string stub and live reflection of Branches mutations
// ---------------------------------------------------------------------------

void
test_branches_srcmounts_from_string_noop()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a:/tmp/b") == 0);

  SrcMounts sm(b);

  // from_string on SrcMounts is a no-op stub — always returns 0, no change
  TEST_CHECK(sm.from_string("/tmp/c:/tmp/d") == 0);

  // Branches should be unchanged (still /tmp/a:/tmp/b)
  TEST_CHECK(sm.to_string() == "/tmp/a:/tmp/b");
}

void
test_branches_srcmounts_reflects_mutations()
{
  Branches b;

  TEST_CHECK(b.from_string("/tmp/a") == 0);

  SrcMounts sm(b);
  TEST_CHECK(sm.to_string() == "/tmp/a");

  // Mutate Branches; SrcMounts::to_string should reflect the new state
  TEST_CHECK(b.from_string("+/tmp/b") == 0);
  TEST_CHECK(sm.to_string() == "/tmp/a:/tmp/b");

  TEST_CHECK(b.from_string("->") == 0);
  TEST_CHECK(sm.to_string() == "/tmp/a");
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
test_fs_inode_readdir_calc_matches_calc()
{
  const fs::path branch_path("/mnt/disk1");
  const std::vector<fs::path> dirpaths = {
    fs::path(),
    fs::path("tv/season_01"),
  };
  const std::vector<std::pair<std::string,mode_t>> entries = {
    {"episode_01.mkv", S_IFREG},
    {"extras", S_IFDIR},
    {"nfo-file.nfo", S_IFREG},
    {"a_very_long_entry_name_used_to_force_buffer_growth.txt", S_IFREG},
  };
  const std::vector<std::string> algos = {
    "passthrough",
    "path-hash",
    "path-hash32",
    "devino-hash",
    "devino-hash32",
    "hybrid-hash",
    "hybrid-hash32",
  };

  for(const auto &algo : algos)
    {
      TEST_CHECK(fs::inode::set_algo(algo) == 0);

      for(const auto &dirpath : dirpaths)
        {
          fs::inode::ReaddirCalc calc(branch_path,dirpath);
          ino_t ino = 1000;

          for(const auto &[name,mode] : entries)
            {
              const fs::path fullpath = dirpath / name;
              const u64 expected = fs::inode::calc(branch_path.string(),
                                                   fullpath.string(),
                                                   mode,
                                                   ino);
              const u64 actual = calc.calc(name.c_str(),
                                           name.size(),
                                           mode,
                                           ino);

              TEST_CHECK(actual == expected);

              ++ino;
            }
        }
    }
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
  Config cfg;

  TEST_CHECK(cfg.has_key("func.readdir") == true);

  const auto &map = cfg.get_map();
  TEST_CHECK(map.count("func.readdir") > 0);
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

// =====================================================================
// ThreadPool tests
// =====================================================================

void
test_tp_construct_default()
{
  ThreadPool tp(2, 4, "test.default");

  auto threads = tp.threads();

  TEST_CHECK(threads.size() == 2);
}

void
test_tp_construct_named()
{
  ThreadPool tp(1, 2, "test.named");

  auto threads = tp.threads();

  TEST_CHECK(threads.size() == 1);
}

void
test_tp_construct_zero_threads_throws()
{
  bool threw = false;

  try
    {
      ThreadPool tp(0, 4, "test.zero");
    }
  catch(const std::runtime_error &)
    {
      threw = true;
    }

  TEST_CHECK(threw);
}

void
test_tp_enqueue_work()
{
  ThreadPool tp(2, 4, "test.ew");

  std::atomic<int> counter{0};

  for(int i = 0; i < 10; ++i)
    tp.enqueue_work([&counter](){ counter.fetch_add(1); });

  // wait for completion
  for(int i = 0; i < 1000 && counter.load() < 10; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  TEST_CHECK(counter.load() == 10);
}

void
test_tp_enqueue_work_with_ptoken()
{
  ThreadPool tp(2, 4, "test.ewp");

  std::atomic<int> counter{0};
  auto ptok = tp.ptoken();

  for(int i = 0; i < 10; ++i)
    tp.enqueue_work(ptok, [&counter](){ counter.fetch_add(1); });

  for(int i = 0; i < 1000 && counter.load() < 10; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  TEST_CHECK(counter.load() == 10);
}

void
test_tp_try_enqueue_work()
{
  // Use 1 thread and a small queue depth of 2
  ThreadPool tp(1, 2, "test.tew");

  std::atomic<int> counter{0};

  // These should succeed (queue has room)
  bool ok = tp.try_enqueue_work([&counter](){ counter.fetch_add(1); });
  TEST_CHECK(ok);

  for(int i = 0; i < 1000 && counter.load() < 1; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  TEST_CHECK(counter.load() == 1);
}

void
test_tp_try_enqueue_work_with_ptoken()
{
  ThreadPool tp(1, 2, "test.tewp");

  std::atomic<int> counter{0};
  auto ptok = tp.ptoken();

  bool ok = tp.try_enqueue_work(ptok, [&counter](){ counter.fetch_add(1); });
  TEST_CHECK(ok);

  for(int i = 0; i < 1000 && counter.load() < 1; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  TEST_CHECK(counter.load() == 1);
}

void
test_tp_try_enqueue_work_for()
{
  ThreadPool tp(1, 2, "test.tewf");

  std::atomic<int> counter{0};

  bool ok = tp.try_enqueue_work_for(100000, // 100ms
                                    [&counter](){ counter.fetch_add(1); });
  TEST_CHECK(ok);

  for(int i = 0; i < 1000 && counter.load() < 1; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  TEST_CHECK(counter.load() == 1);
}

void
test_tp_try_enqueue_work_for_with_ptoken()
{
  ThreadPool tp(1, 2, "test.tewfp");

  std::atomic<int> counter{0};
  auto ptok = tp.ptoken();

  bool ok = tp.try_enqueue_work_for(ptok,
                                    100000, // 100ms
                                    [&counter](){ counter.fetch_add(1); });
  TEST_CHECK(ok);

  for(int i = 0; i < 1000 && counter.load() < 1; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  TEST_CHECK(counter.load() == 1);
}

void
test_tp_enqueue_task_int()
{
  ThreadPool tp(2, 4, "test.eti");

  auto future = tp.enqueue_task([]() -> int { return 42; });

  TEST_CHECK(future.get() == 42);
}

void
test_tp_enqueue_task_void()
{
  ThreadPool tp(2, 4, "test.etv");

  std::atomic<bool> ran{false};

  auto future = tp.enqueue_task([&ran]() { ran.store(true); });

  future.get(); // should not throw

  TEST_CHECK(ran.load());
}

void
test_tp_enqueue_task_string()
{
  ThreadPool tp(2, 4, "test.ets");

  auto future = tp.enqueue_task([]() -> std::string { return "hello"; });

  TEST_CHECK(future.get() == "hello");
}

void
test_tp_enqueue_task_exception()
{
  ThreadPool tp(2, 4, "test.ete");

  auto future = tp.enqueue_task([]() -> int
    {
      throw std::runtime_error("task error");
      return 0;
    });

  bool caught = false;

  try
    {
      future.get();
    }
  catch(const std::runtime_error &e)
    {
      caught = true;
      TEST_CHECK(std::string(e.what()) == "task error");
    }

  TEST_CHECK(caught);
}

void
test_tp_threads_returns_correct_count()
{
  ThreadPool tp(4, 8, "test.thr");

  auto threads = tp.threads();

  TEST_CHECK(threads.size() == 4);

  // All thread IDs should be non-zero
  for(auto t : threads)
    TEST_CHECK(t != 0);
}

void
test_tp_threads_unique_ids()
{
  ThreadPool tp(4, 8, "test.uid");

  auto threads = tp.threads();

  // All thread IDs should be unique
  for(std::size_t i = 0; i < threads.size(); ++i)
    for(std::size_t j = i + 1; j < threads.size(); ++j)
      TEST_CHECK(threads[i] != threads[j]);
}

void
test_tp_add_thread()
{
  ThreadPool tp(2, 4, "test.add");

  TEST_CHECK(tp.threads().size() == 2);

  int rv = tp.add_thread();

  TEST_CHECK(rv == 0);
  TEST_CHECK(tp.threads().size() == 3);
}

void
test_tp_remove_thread()
{
  ThreadPool tp(3, 4, "test.rm");

  TEST_CHECK(tp.threads().size() == 3);

  int rv = tp.remove_thread();

  TEST_CHECK(rv == 0);
  TEST_CHECK(tp.threads().size() == 2);
}

void
test_tp_remove_thread_refuses_last()
{
  ThreadPool tp(1, 4, "test.rmlast");

  TEST_CHECK(tp.threads().size() == 1);

  int rv = tp.remove_thread();

  TEST_CHECK(rv == -EINVAL);
  TEST_CHECK(tp.threads().size() == 1);
}

void
test_tp_set_threads_grow()
{
  ThreadPool tp(2, 8, "test.grow");

  TEST_CHECK(tp.threads().size() == 2);

  int rv = tp.set_threads(4);

  TEST_CHECK(rv == 0);
  TEST_CHECK(tp.threads().size() == 4);
}

void
test_tp_set_threads_shrink()
{
  ThreadPool tp(4, 8, "test.shrink");

  TEST_CHECK(tp.threads().size() == 4);

  int rv = tp.set_threads(2);

  TEST_CHECK(rv == 0);
  TEST_CHECK(tp.threads().size() == 2);
}

void
test_tp_set_threads_zero()
{
  ThreadPool tp(2, 4, "test.sz");

  int rv = tp.set_threads(0);

  TEST_CHECK(rv == -EINVAL);
  TEST_CHECK(tp.threads().size() == 2);
}

void
test_tp_set_threads_same()
{
  ThreadPool tp(3, 4, "test.same");

  int rv = tp.set_threads(3);

  TEST_CHECK(rv == 0);
  TEST_CHECK(tp.threads().size() == 3);
}

void
test_tp_work_after_add_thread()
{
  ThreadPool tp(1, 4, "test.waat");

  tp.add_thread();
  tp.add_thread();

  std::atomic<int> counter{0};
  const int N = 20;

  for(int i = 0; i < N; ++i)
    tp.enqueue_work([&counter](){ counter.fetch_add(1); });

  for(int i = 0; i < 2000 && counter.load() < N; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  TEST_CHECK(counter.load() == N);
}

void
test_tp_work_after_remove_thread()
{
  ThreadPool tp(3, 4, "test.wart");

  tp.remove_thread();

  std::atomic<int> counter{0};
  const int N = 10;

  for(int i = 0; i < N; ++i)
    tp.enqueue_work([&counter](){ counter.fetch_add(1); });

  for(int i = 0; i < 2000 && counter.load() < N; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  TEST_CHECK(counter.load() == N);
}

void
test_tp_worker_exception_no_crash()
{
  ThreadPool tp(2, 4, "test.exc");

  // Enqueue work that throws -- pool should not crash
  tp.enqueue_work([](){
    throw std::runtime_error("deliberate error");
  });

  // Enqueue normal work after the exception
  std::atomic<bool> ran{false};
  tp.enqueue_work([&ran](){ ran.store(true); });

  for(int i = 0; i < 1000 && !ran.load(); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  TEST_CHECK(ran.load());
}

void
test_tp_concurrent_enqueue()
{
  ThreadPool tp(4, 64, "test.conc");

  std::atomic<int> counter{0};
  const int PRODUCERS = 4;
  const int ITEMS_PER = 50;
  const int TOTAL = PRODUCERS * ITEMS_PER;

  std::vector<std::thread> producers;
  for(int p = 0; p < PRODUCERS; ++p)
    {
      producers.emplace_back([&tp, &counter]()
        {
          auto ptok = tp.ptoken();
          for(int i = 0; i < ITEMS_PER; ++i)
            tp.enqueue_work(ptok, [&counter](){ counter.fetch_add(1); });
        });
    }

  for(auto &t : producers)
    t.join();

  for(int i = 0; i < 5000 && counter.load() < TOTAL; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  TEST_CHECK(counter.load() == TOTAL);
}

void
test_tp_enqueue_task_multiple()
{
  ThreadPool tp(4, 16, "test.etm");

  std::vector<std::future<int>> futures;
  const int N = 20;

  for(int i = 0; i < N; ++i)
    futures.push_back(tp.enqueue_task([i]() -> int { return i * i; }));

  for(int i = 0; i < N; ++i)
    TEST_CHECK(futures[i].get() == i * i);
}

void
test_tp_ptoken_creation()
{
  ThreadPool tp(2, 4, "test.ptok");

  // Multiple ptokens should be independently usable
  auto ptok1 = tp.ptoken();
  auto ptok2 = tp.ptoken();

  std::atomic<int> counter{0};

  tp.enqueue_work(ptok1, [&counter](){ counter.fetch_add(1); });
  tp.enqueue_work(ptok2, [&counter](){ counter.fetch_add(1); });

  for(int i = 0; i < 1000 && counter.load() < 2; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  TEST_CHECK(counter.load() == 2);
}

void
test_tp_destruction_drains_queue()
{
  std::atomic<int> counter{0};

  {
    ThreadPool tp(2, 64, "test.drain");

    for(int i = 0; i < 50; ++i)
      tp.enqueue_work([&counter](){
        counter.fetch_add(1);
      });

    // destructor runs here -- should drain all queued work
  }

  TEST_CHECK(counter.load() == 50);
}

void
test_tp_move_only_callable()
{
  ThreadPool tp(2, 4, "test.moc");

  auto ptr = std::make_unique<int>(99);

  auto future = tp.enqueue_task([p = std::move(ptr)]() -> int
    {
      return *p;
    });

  TEST_CHECK(future.get() == 99);
}

void
test_tp_work_ordering_single_thread()
{
  // With a single thread, work should execute in FIFO order
  ThreadPool tp(1, 16, "test.order");

  std::vector<int> results;
  std::mutex mtx;
  const int N = 10;

  for(int i = 0; i < N; ++i)
    tp.enqueue_work([i, &results, &mtx](){
      std::lock_guard<std::mutex> lk(mtx);
      results.push_back(i);
    });

  // Wait for all work to complete
  for(int i = 0; i < 2000; ++i)
    {
      {
        std::lock_guard<std::mutex> lk(mtx);
        if((int)results.size() == N)
          break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

  std::lock_guard<std::mutex> lk(mtx);
  TEST_CHECK((int)results.size() == N);
  for(int i = 0; i < N; ++i)
    TEST_CHECK(results[i] == i);
}

void
test_tp_try_enqueue_work_fails_when_full()
{
  ThreadPool tp(1, 1, "test.tewfull");

  std::atomic<bool> release{false};
  std::atomic<bool> worker_started{false};
  std::atomic<int> ran{0};

  tp.enqueue_work([&release, &worker_started, &ran]()
    {
      worker_started.store(true);
      while(!release.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      ran.fetch_add(1);
    });

  TEST_CHECK(wait_until([&worker_started](){ return worker_started.load(); }));

  tp.enqueue_work([&ran](){ ran.fetch_add(1); });

  bool ok = tp.try_enqueue_work([&ran](){ ran.fetch_add(1); });
  TEST_CHECK(!ok);

  release.store(true);

  TEST_CHECK(wait_until([&ran](){ return ran.load() == 2; }));
  TEST_CHECK(ran.load() == 2);
}

void
test_tp_try_enqueue_work_for_times_out_when_full()
{
  ThreadPool tp(1, 1, "test.tewftimeout");

  std::atomic<bool> release{false};
  std::atomic<bool> worker_started{false};
  std::atomic<int> ran{0};

  tp.enqueue_work([&release, &worker_started, &ran]()
    {
      worker_started.store(true);
      while(!release.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      ran.fetch_add(1);
    });

  TEST_CHECK(wait_until([&worker_started](){ return worker_started.load(); }));

  tp.enqueue_work([&ran](){ ran.fetch_add(1); });

  auto start = std::chrono::steady_clock::now();
  bool ok = tp.try_enqueue_work_for(20000, [&ran](){ ran.fetch_add(1); });
  auto end = std::chrono::steady_clock::now();

  TEST_CHECK(!ok);

  auto elapsed_us =
    std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  TEST_CHECK(elapsed_us >= 10000);

  release.store(true);

  TEST_CHECK(wait_until([&ran](){ return ran.load() == 2; }));
  TEST_CHECK(ran.load() == 2);
}

void
test_tp_enqueue_work_blocks_until_slot_available()
{
  ThreadPool tp(1, 1, "test.ewblocks");

  std::atomic<bool> release{false};
  std::atomic<bool> worker_started{false};
  std::atomic<bool> producer_returned{false};
  std::atomic<int> ran{0};

  tp.enqueue_work([&release, &worker_started, &ran]()
    {
      worker_started.store(true);
      while(!release.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      ran.fetch_add(1);
    });

  TEST_CHECK(wait_until([&worker_started](){ return worker_started.load(); }));

  tp.enqueue_work([&ran](){ ran.fetch_add(1); });

  std::thread producer([&tp, &ran, &producer_returned]()
    {
      tp.enqueue_work([&ran](){ ran.fetch_add(1); });
      producer_returned.store(true);
    });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  TEST_CHECK(!producer_returned.load());

  release.store(true);

  TEST_CHECK(wait_until([&producer_returned](){ return producer_returned.load(); }));
  producer.join();

  TEST_CHECK(wait_until([&ran](){ return ran.load() == 3; }));
  TEST_CHECK(ran.load() == 3);
}

void
test_tp_worker_nonstd_exception_no_crash()
{
  ThreadPool tp(2, 4, "test.nsexc");

  tp.enqueue_work([](){ throw 7; });

  std::atomic<bool> ran{false};
  tp.enqueue_work([&ran](){ ran.store(true); });

  TEST_CHECK(wait_until([&ran](){ return ran.load(); }));
  TEST_CHECK(ran.load());
}

void
test_tp_remove_thread_under_load()
{
  ThreadPool tp(4, 64, "test.rmuload");

  std::atomic<int> counter{0};
  const int N = 300;

  for(int i = 0; i < N; ++i)
    tp.enqueue_work([&counter]()
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        counter.fetch_add(1);
      });

  int rv1 = -1;
  int rv2 = -1;

  std::thread remover([&tp, &rv1, &rv2]()
    {
      rv1 = tp.remove_thread();
      rv2 = tp.remove_thread();
    });

  remover.join();

  TEST_CHECK(rv1 == 0);
  TEST_CHECK(rv2 == 0);
  TEST_CHECK(tp.threads().size() == 2);

  TEST_CHECK(wait_until([&counter, N](){ return counter.load() == N; }, 10000));
  TEST_CHECK(counter.load() == N);

  std::atomic<bool> post_ok{false};
  tp.enqueue_work([&post_ok](){ post_ok.store(true); });
  TEST_CHECK(wait_until([&post_ok](){ return post_ok.load(); }));
}

void
test_tp_set_threads_concurrent_with_enqueue()
{
  ThreadPool tp(3, 128, "test.stcwe");

  const int PRODUCERS = 4;
  const int PER_PRODUCER = 150;
  const int TOTAL = PRODUCERS * PER_PRODUCER;
  std::atomic<int> counter{0};

  std::atomic<bool> producers_done{false};
  std::atomic<int> resize_calls{0};

  std::thread resizer([&tp, &producers_done, &resize_calls]()
    {
      int i = 0;
      while(!producers_done.load())
        {
          std::size_t count = ((i % 2) == 0 ? 2 : 6);
          int rv = tp.set_threads(count);
          if(rv == 0)
            resize_calls.fetch_add(1);
          ++i;
        }
    });

  std::vector<std::thread> producers;
  for(int p = 0; p < PRODUCERS; ++p)
    {
      producers.emplace_back([&tp, &counter]()
        {
          auto ptok = tp.ptoken();
          for(int i = 0; i < PER_PRODUCER; ++i)
            tp.enqueue_work(ptok, [&counter](){ counter.fetch_add(1); });
        });
    }

  for(auto &t : producers)
    t.join();

  producers_done.store(true);
  resizer.join();

  TEST_CHECK(resize_calls.load() > 0);
  TEST_CHECK(wait_until([&counter, TOTAL](){ return counter.load() == TOTAL; }, 10000));
  TEST_CHECK(counter.load() == TOTAL);
}

void
test_tp_repeated_resize_stress()
{
  ThreadPool tp(2, 64, "test.resize");

  const int ROUNDS = 30;
  const int PER_ROUND = 20;
  const int TOTAL = ROUNDS * PER_ROUND;
  std::atomic<int> counter{0};

  for(int r = 0; r < ROUNDS; ++r)
    {
      std::size_t target = 1 + (r % 5);
      int rv = tp.set_threads(target);
      TEST_CHECK(rv == 0);

      for(int i = 0; i < PER_ROUND; ++i)
        tp.enqueue_work([&counter](){ counter.fetch_add(1); });
    }

  TEST_CHECK(wait_until([&counter, TOTAL](){ return counter.load() == TOTAL; }, 10000));
  TEST_CHECK(counter.load() == TOTAL);
}

void
test_tp_many_producers_many_tasks_stress()
{
  ThreadPool tp(8, 256, "test.mpmt");

  const int PRODUCERS = 8;
  const int ITEMS_PER = 200;
  const int TOTAL = PRODUCERS * ITEMS_PER;
  std::atomic<int> counter{0};

  std::vector<std::thread> producers;
  for(int p = 0; p < PRODUCERS; ++p)
    {
      producers.emplace_back([&tp, &counter]()
        {
          auto ptok = tp.ptoken();
          for(int i = 0; i < ITEMS_PER; ++i)
            tp.enqueue_work(ptok, [&counter](){ counter.fetch_add(1); });
        });
    }

  for(auto &t : producers)
    t.join();

  TEST_CHECK(wait_until([&counter, TOTAL](){ return counter.load() == TOTAL; }, 10000));
  TEST_CHECK(counter.load() == TOTAL);
}

void
test_tp_heavy_mixed_resize_and_enqueue_stress()
{
  ThreadPool tp(6, 512, "test.hmix");

  const int PRODUCERS = 10;
  const int PER_PRODUCER = 2500;
  const int TOTAL = PRODUCERS * PER_PRODUCER;

  std::atomic<int> executed{0};
  std::atomic<bool> producers_done{false};
  std::atomic<int> resize_ops{0};

  std::thread resizer([&tp, &producers_done, &resize_ops]()
    {
      int i = 0;
      while(!producers_done.load())
        {
          const std::size_t target = 2 + (i % 10);
          int rv = tp.set_threads(target);
          if(rv == 0)
            resize_ops.fetch_add(1);
          ++i;
        }
    });

  std::vector<std::thread> producers;
  producers.reserve(PRODUCERS);
  for(int p = 0; p < PRODUCERS; ++p)
    {
      producers.emplace_back([&tp, &executed]()
        {
          auto ptok = tp.ptoken();

          for(int i = 0; i < PER_PRODUCER; ++i)
            tp.enqueue_work(ptok, [&executed](){ executed.fetch_add(1); });
        });
    }

  for(auto &t : producers)
    t.join();

  producers_done.store(true);
  resizer.join();

  TEST_CHECK(resize_ops.load() > 0);
  TEST_CHECK(wait_until([&executed, TOTAL](){ return executed.load() == TOTAL; }, 30000));
  TEST_CHECK(executed.load() == TOTAL);
}

void
test_tp_heavy_try_enqueue_pressure()
{
  ThreadPool tp(1, 1, "test.hpress");

  const int PRODUCERS = 6;
  const int ATTEMPTS_PER = 2500;

  std::atomic<int> accepted{0};
  std::atomic<int> rejected{0};
  std::atomic<int> executed{0};

  std::vector<std::thread> producers;
  producers.reserve(PRODUCERS);
  for(int p = 0; p < PRODUCERS; ++p)
    {
      producers.emplace_back([&tp, &accepted, &rejected, &executed]()
        {
          auto ptok = tp.ptoken();

          for(int i = 0; i < ATTEMPTS_PER; ++i)
            {
              bool ok = tp.try_enqueue_work_for(ptok,
                                                200,
                                                [&executed]()
                                                {
                                                  std::this_thread::sleep_for(std::chrono::microseconds(50));
                                                  executed.fetch_add(1);
                                                });
              if(ok)
                accepted.fetch_add(1);
              else
                rejected.fetch_add(1);
            }
        });
    }

  for(auto &t : producers)
    t.join();

  TEST_CHECK(accepted.load() > 0);
  TEST_CHECK(rejected.load() > 0);

  TEST_CHECK(wait_until([&executed, &accepted]()
                        {
                          return executed.load() == accepted.load();
                        },
                        30000));
  TEST_CHECK(executed.load() == accepted.load());
}

void
test_tp_heavy_repeated_construct_destroy()
{
  const int ROUNDS = 150;

  for(int r = 0; r < ROUNDS; ++r)
    {
      std::atomic<int> counter{0};

      {
        ThreadPool tp(4, 128, "test.hctor");

        for(int i = 0; i < 400; ++i)
          tp.enqueue_work([&counter]()
            {
              std::this_thread::sleep_for(std::chrono::microseconds(25));
              counter.fetch_add(1);
            });
      }

      TEST_CHECK(counter.load() == 400);
    }
}

void
test_tp_heavy_enqueue_task_mixed_outcomes()
{
  ThreadPool tp(8, 512, "test.htask");

  const int N = 12000;

  std::vector<std::future<int>> futures;
  futures.reserve(N);

  for(int i = 0; i < N; ++i)
    {
      futures.emplace_back(tp.enqueue_task([i]() -> int
        {
          if((i % 11) == 0)
            throw std::runtime_error("heavy task error");

          return i;
        }));
    }

  long long expected_sum = 0;
  int expected_success = 0;
  int expected_errors = 0;

  for(int i = 0; i < N; ++i)
    {
      if((i % 11) == 0)
        {
          expected_errors += 1;
        }
      else
        {
          expected_success += 1;
          expected_sum += i;
        }
    }

  int actual_success = 0;
  int actual_errors = 0;
  long long actual_sum = 0;

  for(auto &f : futures)
    {
      try
        {
          actual_sum += f.get();
          actual_success += 1;
        }
      catch(const std::runtime_error &e)
        {
          actual_errors += 1;
          TEST_CHECK(std::string(e.what()) == "heavy task error");
        }
    }

  TEST_CHECK(actual_success == expected_success);
  TEST_CHECK(actual_errors == expected_errors);
  TEST_CHECK(actual_sum == expected_sum);
}

void
test_tp_heavy_add_remove_churn_under_enqueue()
{
  ThreadPool tp(6, 512, "test.hchurn");

  const int PRODUCERS = 6;
  const int PER_PRODUCER = 2200;
  const int TOTAL = PRODUCERS * PER_PRODUCER;
  const int CHURN_OPS = 700;

  std::atomic<int> executed{0};
  std::atomic<bool> producers_done{false};
  std::atomic<int> add_ok{0};
  std::atomic<int> remove_ok{0};

  std::thread churner([&tp, &producers_done, &add_ok, &remove_ok]()
    {
      int i = 0;
      while(!producers_done.load() && i < CHURN_OPS)
        {
          int rv_add = tp.add_thread();
          if(rv_add == 0)
            add_ok.fetch_add(1);

          int rv_remove = tp.remove_thread();
          if(rv_remove == 0)
            remove_ok.fetch_add(1);

          ++i;
        }
    });

  std::vector<std::thread> producers;
  producers.reserve(PRODUCERS);
  for(int p = 0; p < PRODUCERS; ++p)
    {
      producers.emplace_back([&tp, &executed]()
        {
          auto ptok = tp.ptoken();
          for(int i = 0; i < PER_PRODUCER; ++i)
            tp.enqueue_work(ptok,
                            [&executed]()
                            {
                              executed.fetch_add(1);
                            });
        });
    }

  for(auto &t : producers)
    t.join();

  producers_done.store(true);
  churner.join();

  TEST_CHECK(add_ok.load() > 0);
  TEST_CHECK(remove_ok.load() > 0);
  TEST_CHECK(wait_until([&executed, TOTAL](){ return executed.load() == TOTAL; }, 30000));
  TEST_CHECK(executed.load() == TOTAL);
}

void
test_config_has_key()
{
  Config cfg;

  TEST_CHECK(cfg.has_key("async-read") == true);
  TEST_CHECK(cfg.has_key("branches") == true);
  TEST_CHECK(cfg.has_key("version") == true);
  TEST_CHECK(cfg.has_key("nonexistent-key-xyz") == false);
  TEST_CHECK(cfg.has_key("") == false);

  // Underscore form should NOT match (has_key does no normalization)
  TEST_CHECK(cfg.has_key("async_read") == false);
}

void
test_config_keys()
{
  Config cfg;

  const auto &map = cfg.get_map();

  // Result must be non-empty
  TEST_CHECK(!map.empty());

  // Verify known keys are present
  TEST_CHECK(map.count("async-read") > 0);
  TEST_CHECK(map.count("branches")   > 0);
  TEST_CHECK(map.count("version")    > 0);

  // No key should be empty
  for(const auto &[k, v] : map)
    TEST_CHECK(!k.empty());
}

void
test_config_keys_xattr()
{
  Config cfg;

  ssize_t needed = cfg.keys_listxattr(nullptr, 0);
  TEST_CHECK(needed > 0);

  std::string s(needed, '\0');
  ssize_t written = cfg.keys_listxattr(s.data(), needed);
  TEST_CHECK(written > 0);
  s.resize(written);

  // Every token must start with "user.mergerfs." and end with NUL
  const char *p   = s.data();
  const char *end = p + s.size();
  int count = 0;

  while(p < end)
    {
      const char *nul = static_cast<const char*>(memchr(p, '\0', end - p));
      TEST_CHECK(nul != nullptr);
      if(!nul) break;

      std::string entry(p, nul);
      TEST_CHECK(str::startswith(entry, "user.mergerfs."));
      ++count;
      p = nul + 1;
    }

  TEST_CHECK(count > 0);
}

void
test_config_keys_listxattr_size()
{
  Config cfg;

  ssize_t sz = cfg.keys_listxattr_size();
  TEST_CHECK(sz > 0);

  // Size must accommodate a buffer filled by keys_listxattr
  std::string buf(sz, '\0');
  ssize_t written = cfg.keys_listxattr(buf.data(), sz);
  TEST_CHECK(written > 0);
  TEST_CHECK(written <= sz);
}

void
test_config_keys_listxattr_fill()
{
  Config cfg;

  // size=0 must return required size (not write anything)
  ssize_t needed = cfg.keys_listxattr(nullptr, 0);
  TEST_CHECK(needed > 0);

  // Allocate exact size and fill
  std::vector<char> buf(needed);
  ssize_t written = cfg.keys_listxattr(buf.data(), needed);
  TEST_CHECK(written > 0);
  TEST_CHECK(written <= needed);

  // Every entry must start with "user.mergerfs." and end with NUL
  const char *p   = buf.data();
  const char *end = p + written;
  int count = 0;
  while(p < end)
    {
      const char *nul = static_cast<const char*>(memchr(p, '\0', end - p));
      TEST_CHECK(nul != nullptr);
      if(!nul) break;

      std::string entry(p, nul);
      TEST_CHECK(str::startswith(entry, "user.mergerfs."));
      TEST_CHECK(entry.size() > strlen("user.mergerfs."));
      ++count;
      p = nul + 1;
    }

  TEST_CHECK(count > 0);

  // Keys reported by keys_listxattr must be consistent across two calls
  std::vector<char> xattr_buf(needed);
  ssize_t xattr_written = cfg.keys_listxattr(xattr_buf.data(), needed);
  TEST_CHECK(xattr_written == written);
  TEST_CHECK(memcmp(buf.data(), xattr_buf.data(), written) == 0);
}

void
test_config_keys_listxattr_erange()
{
  Config cfg;

  // Buffer too small (1 byte) must return -ERANGE
  char tiny[1] = {'\xff'};
  ssize_t rv = cfg.keys_listxattr(tiny, 1);
  TEST_CHECK(rv == -ERANGE);
}

void
test_config_get_set_roundtrip()
{
  Config cfg;
  std::string val;

  // bool: async-read
  TEST_CHECK(cfg.set("async-read", "false") == 0);
  TEST_CHECK(cfg.get("async-read", &val) == 0);
  TEST_CHECK(val == "false");

  TEST_CHECK(cfg.set("async-read", "true") == 0);
  TEST_CHECK(cfg.get("async-read", &val) == 0);
  TEST_CHECK(val == "true");

  // u64: cache-attr
  TEST_CHECK(cfg.set("cache.attr", "42") == 0);
  TEST_CHECK(cfg.get("cache.attr", &val) == 0);
  TEST_CHECK(val == "42");

  // string: fsname
  TEST_CHECK(cfg.set("fsname", "myfs") == 0);
  TEST_CHECK(cfg.get("fsname", &val) == 0);
  TEST_CHECK(val == "myfs");
}

void
test_config_get_unknown_key()
{
  Config cfg;
  std::string val;

  int rv = cfg.get("no-such-key", &val);
  TEST_CHECK(rv == -ENOATTR);
}

void
test_config_set_unknown_key()
{
  Config cfg;

  int rv = cfg.set("no-such-key", "value");
  TEST_CHECK(rv == -ENOATTR);
}

void
test_config_set_invalid_value()
{
  Config cfg;
  std::string val;

  // "async-read" is a bool — "notabool" must be rejected
  int rv = cfg.set("async-read", "notabool");
  TEST_CHECK(rv == -EINVAL);

  // value must be unchanged
  TEST_CHECK(cfg.get("async-read", &val) == 0);
  TEST_CHECK(val == "true");
}

void
test_config_set_underscore_normalization()
{
  Config cfg;
  std::string val;

  // Underscores must be converted to hyphens
  TEST_CHECK(cfg.set("async_read", "false") == 0);
  TEST_CHECK(cfg.get("async-read", &val) == 0);
  TEST_CHECK(val == "false");
}

void
test_config_get_underscore_normalization()
{
  Config cfg;
  std::string val;

  TEST_CHECK(cfg.set("async-read", "false") == 0);
  TEST_CHECK(cfg.get("async_read", &val) == 0);
  TEST_CHECK(val == "false");
}

void
test_config_set_kv_form()
{
  Config cfg;
  std::string val;

  TEST_CHECK(cfg.set("async-read=false") == 0);
  TEST_CHECK(cfg.get("async-read", &val) == 0);
  TEST_CHECK(val == "false");

  // With spaces around delimiter
  TEST_CHECK(cfg.set("async-read = true") == 0);
  TEST_CHECK(cfg.get("async-read", &val) == 0);
  TEST_CHECK(val == "true");
}

void
test_config_remember_nodes_bool_numeric()
{
  bool old_remember_nodes = fuse_cfg.remember_nodes;
  Config cfg;
  std::string val;

  fuse_cfg.remember_nodes = false;

  // remember-nodes accepts ints: 0 disables, non-zero enables
  TEST_CHECK(cfg.set("remember-nodes", "0") == 0);
  TEST_CHECK(cfg.get("remember-nodes", &val) == 0);
  TEST_CHECK(val == "false");

  TEST_CHECK(cfg.set("remember-nodes", "86400") == 0);
  TEST_CHECK(cfg.get("remember-nodes", &val) == 0);
  TEST_CHECK(val == "true");

  TEST_CHECK(cfg.set("remember", "-1") == 0);
  TEST_CHECK(cfg.get("remember", &val) == 0);
  TEST_CHECK(val == "true");

  // remember-nodes also accepts bool strings
  TEST_CHECK(cfg.set("remember-nodes", "false") == 0);
  TEST_CHECK(cfg.get("remember-nodes", &val) == 0);
  TEST_CHECK(val == "false");

  // never-forget-nodes is bool; it reflects the feature state
  TEST_CHECK(cfg.set("never-forget-nodes", "true") == 0);
  TEST_CHECK(cfg.get("never-forget-nodes", &val) == 0);
  TEST_CHECK(val == "true");

  // Querying remember-nodes after never-forget-nodes=true returns true
  // (both alias the same fuse_cfg.remember_nodes)
  TEST_CHECK(cfg.get("remember-nodes", &val) == 0);
  TEST_CHECK(val == "true");

  // Empty string treated as true
  TEST_CHECK(cfg.set("noforget", "") == 0);
  TEST_CHECK(cfg.get("noforget", &val) == 0);
  TEST_CHECK(val == "true");

  // remember-nodes also accepts empty string as true
  fuse_cfg.remember_nodes = false;
  TEST_CHECK(cfg.set("remember-nodes", "") == 0);
  TEST_CHECK(cfg.get("remember-nodes", &val) == 0);
  TEST_CHECK(val == "true");

  // Invalid string rejected by remember-nodes
  TEST_CHECK(cfg.set("remember-nodes", "notabool") == -EINVAL);

  fuse_cfg.remember_nodes = old_remember_nodes;
}


void
test_config_readonly_before_init()
{
  Config cfg;
  std::string val;

  // version is RO even before finish_initializing()
  int rv = cfg.set("version", "9.9.9");
  TEST_CHECK(rv == -EROFS);
}

void
test_config_readonly_after_init()
{
  Config cfg;
  std::string val;

  // Before init: mutable fields can be set
  TEST_CHECK(cfg.set("async-read", "false") == 0);

  cfg.finish_initializing();

  // async-read is marked RO after init
  int rv = cfg.set("async-read", "true");
  TEST_CHECK(rv == -EROFS);

  // Value must be unchanged
  TEST_CHECK(cfg.get("async-read", &val) == 0);
  TEST_CHECK(val == "false");
}

void
test_config_mutable_after_init()
{
  Config cfg;
  std::string val;

  cfg.finish_initializing();

  // dropcacheonclose is NOT RO after init — should still be settable
  TEST_CHECK(cfg.set("dropcacheonclose", "true") == 0);
  TEST_CHECK(cfg.get("dropcacheonclose", &val) == 0);
  TEST_CHECK(val == "true");
}

void
test_config_from_stream_basic()
{
  Config cfg;
  std::string val;

  std::istringstream ss("async-read=false\ncache.attr=99\n");
  int rv = cfg.from_stream(ss);
  TEST_CHECK(rv == 0);

  TEST_CHECK(cfg.get("async-read", &val) == 0);
  TEST_CHECK(val == "false");

  TEST_CHECK(cfg.get("cache.attr", &val) == 0);
  TEST_CHECK(val == "99");
}

void
test_config_from_stream_comments_and_blanks()
{
  Config cfg;
  std::string val;

  std::istringstream ss(
    "# this is a comment\n"
    "\n"
    "   \n"
    "async-read=false\n"
    "# another comment\n"
    "cache.attr=7\n"
  );

  int rv = cfg.from_stream(ss);
  TEST_CHECK(rv == 0);

  TEST_CHECK(cfg.get("async-read", &val) == 0);
  TEST_CHECK(val == "false");

  TEST_CHECK(cfg.get("cache.attr", &val) == 0);
  TEST_CHECK(val == "7");
}

void
test_config_from_stream_bad_lines()
{
  Config cfg;

  std::istringstream ss("async-read=false\nbad-key-xyz=oops\ncache.attr=5\n");
  int rv = cfg.from_stream(ss);

  // Must report error but still apply valid lines
  TEST_CHECK(rv == -EINVAL);
  TEST_CHECK(!cfg.errs.empty());

  std::string val;
  TEST_CHECK(cfg.get("async-read", &val) == 0);
  TEST_CHECK(val == "false");

  TEST_CHECK(cfg.get("cache.attr", &val) == 0);
  TEST_CHECK(val == "5");
}

void
test_config_from_file_nonexistent()
{
  Config cfg;

  int rv = cfg.from_file("/this/path/does/not/exist/config.txt");
  TEST_CHECK(rv < 0);
  TEST_CHECK(!cfg.errs.empty());
}

void
test_config_is_rootdir()
{
  TEST_CHECK(Config::is_rootdir("") == true);
  TEST_CHECK(Config::is_rootdir("/") == false);
  TEST_CHECK(Config::is_rootdir("/foo") == false);
}

void
test_config_is_ctrl_file()
{
  TEST_CHECK(Config::is_ctrl_file(".mergerfs") == true);
  TEST_CHECK(Config::is_ctrl_file("/.mergerfs") == false);
  TEST_CHECK(Config::is_ctrl_file("mergerfs") == false);
  TEST_CHECK(Config::is_ctrl_file("") == false);
}

void
test_config_is_mergerfs_xattr()
{
  TEST_CHECK(Config::is_mergerfs_xattr("user.mergerfs.async-read") == true);
  TEST_CHECK(Config::is_mergerfs_xattr("user.mergerfs.") == true);
  TEST_CHECK(Config::is_mergerfs_xattr("user.mergerfs") == false);
  TEST_CHECK(Config::is_mergerfs_xattr("user.other.async-read") == false);
  TEST_CHECK(Config::is_mergerfs_xattr("") == false);
}

void
test_config_is_cmd_xattr()
{
  TEST_CHECK(Config::is_cmd_xattr("user.mergerfs.cmd.foo") == true);
  TEST_CHECK(Config::is_cmd_xattr("user.mergerfs.cmd.") == true);
  TEST_CHECK(Config::is_cmd_xattr("user.mergerfs.async-read") == false);
  TEST_CHECK(Config::is_cmd_xattr("user.mergerfs.cmd") == false);
  TEST_CHECK(Config::is_cmd_xattr("") == false);
}

void
test_config_prune_ctrl_xattr()
{
  TEST_CHECK(Config::prune_ctrl_xattr("user.mergerfs.async-read") == "async-read");
  TEST_CHECK(Config::prune_ctrl_xattr("user.mergerfs.cache.attr") == "cache.attr");
  // Exactly the prefix — nothing after it: returns empty
  TEST_CHECK(Config::prune_ctrl_xattr("user.mergerfs.") == "");
  // Shorter than prefix — returns empty
  TEST_CHECK(Config::prune_ctrl_xattr("user.mergerfs") == "");
  TEST_CHECK(Config::prune_ctrl_xattr("") == "");
}

void
test_config_prune_cmd_xattr()
{
  using sv = std::string_view;

  TEST_CHECK(Config::prune_cmd_xattr("user.mergerfs.cmd.foo") == sv("foo"));
  TEST_CHECK(Config::prune_cmd_xattr("user.mergerfs.cmd.bar.baz") == sv("bar.baz"));
  TEST_CHECK(Config::prune_cmd_xattr("user.mergerfs.cmd.") == sv(""));
  TEST_CHECK(Config::prune_cmd_xattr("user.mergerfs.cmd") == sv(""));
  TEST_CHECK(Config::prune_cmd_xattr("") == sv(""));
}

void
test_fs_copyfile_basic()
{
  int rv;
  int src_fd;
  struct stat st;
  fs::path src_path;
  fs::path dst_path;
  fs::path tmp_dir;
  char tmp_template[] = "/tmp/mergerfs-test-copyfile-basic-XXXXXX";

  if(::mkdtemp(tmp_template) == nullptr)
    {
      TEST_CHECK(false);
      return;
    }

  tmp_dir = tmp_template;
  src_path = tmp_dir / "src.bin";
  dst_path = tmp_dir / "dst.bin";

  src_fd = ::open(src_path.c_str(),O_RDWR|O_CREAT|O_TRUNC,0600);
  TEST_CHECK(src_fd >= 0);
  if(src_fd < 0)
    {
      std::filesystem::remove_all(tmp_dir);
      return;
    }

  rv = ::ftruncate(src_fd,(16 * 1024 * 1024));
  TEST_CHECK(rv == 0);
  rv = ::pwrite(src_fd,"A",1,0);
  TEST_CHECK(rv == 1);
  rv = ::pwrite(src_fd,"Z",1,((16 * 1024 * 1024) - 1));
  TEST_CHECK(rv == 1);

  rv = fs::copyfile(src_fd,dst_path,{.cleanup_failure = true});
  TEST_CHECK(rv == 0);

  rv = ::fstat(src_fd,&st);
  TEST_CHECK(rv == 0);

  struct stat dst_st;
  rv = ::stat(dst_path.c_str(),&dst_st);
  TEST_CHECK(rv == 0);
  if(rv == 0)
    TEST_CHECK(st.st_size == dst_st.st_size);

  char c;
  int dst_fd = ::open(dst_path.c_str(),O_RDONLY);
  TEST_CHECK(dst_fd >= 0);
  if(dst_fd >= 0)
    {
      rv = ::pread(dst_fd,&c,1,0);
      TEST_CHECK((rv == 1) && (c == 'A'));
      rv = ::pread(dst_fd,&c,1,(dst_st.st_size - 1));
      TEST_CHECK((rv == 1) && (c == 'Z'));
      ::close(dst_fd);
    }

  ::close(src_fd);
  std::filesystem::remove_all(tmp_dir);
}

void
test_fs_copyfile_source_changes_cleanup_tmpfiles()
{
  int rv;
  int src_fd;
  fs::path src_path;
  fs::path dst_path;
  fs::path tmp_dir;
  std::atomic<bool> stop_mutator{false};
  std::atomic<int> mutator_updates{0};
  char tmp_template[] = "/tmp/mergerfs-test-copyfile-race-XXXXXX";

  if(::mkdtemp(tmp_template) == nullptr)
    {
      TEST_CHECK(false);
      return;
    }

  tmp_dir = tmp_template;
  src_path = tmp_dir / "src.bin";
  dst_path = tmp_dir / "dst.bin";

  src_fd = ::open(src_path.c_str(),O_RDWR|O_CREAT|O_TRUNC,0600);
  TEST_CHECK(src_fd >= 0);
  if(src_fd < 0)
    {
      std::filesystem::remove_all(tmp_dir);
      return;
    }

  rv = ::ftruncate(src_fd,(64 * 1024 * 1024));
  TEST_CHECK(rv == 0);

  auto mutator = std::thread([&]()
  {
    for(int i = 0; i < 200 && !stop_mutator.load(); ++i)
      {
        struct timespec ts[2];

        ::clock_gettime(CLOCK_REALTIME,&ts[0]);
        ts[1] = ts[0];
        ::utimensat(AT_FDCWD,src_path.c_str(),ts,0);

        mutator_updates.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
  });

  rv = fs::copyfile(src_fd,dst_path,{.cleanup_failure = true});
  stop_mutator.store(true);
  mutator.join();

  TEST_CHECK(rv == 0);
  TEST_CHECK(mutator_updates.load() > 0);

  bool found_tmp_file = false;
  std::string tmp_prefix;
  tmp_prefix = ".";
  tmp_prefix += dst_path.filename().string();
  tmp_prefix += "_";
  for(const auto &entry : std::filesystem::directory_iterator(tmp_dir))
    {
      const std::string name = entry.path().filename().string();
      if(name.rfind(tmp_prefix,0) == 0)
        {
          found_tmp_file = true;
          break;
        }
    }
  TEST_CHECK(found_tmp_file == false);

  ::close(src_fd);
  std::filesystem::remove_all(tmp_dir);
}

void
test_hashset_put_and_size()
{
  HashSet set;
  const char *names[] = {
    "alpha",
    "bravo",
    "charlie",
    "delta",
    "echo",
    "foxtrot",
    "golf",
    "hotel",
    "india",
    "juliet",
    "kilo",
    "lima",
  };
  const size_t names_len = (sizeof(names) / sizeof(names[0]));
  const char *extra = "omega-long-filename";

  TEST_CHECK(set.size() == 0);
  TEST_CHECK(set.put(names[0]) == 1);
  TEST_CHECK(set.put(names[0],strlen(names[0])) == 0);

  for(size_t i = 1; i < names_len; ++i)
    TEST_CHECK(set.put(names[i],strlen(names[i])) == 1);

  TEST_CHECK(set.size() == (int)names_len);
  TEST_CHECK(set.put(names[7]) == 0);
  TEST_CHECK(set.put(names[11],strlen(names[11])) == 0);
  TEST_CHECK(set.size() == (int)names_len);

  TEST_CHECK(set.put(extra) == 1);
  TEST_CHECK(set.put(extra,strlen(extra)) == 0);
  TEST_CHECK(set.size() == (int)(names_len + 1));
}

void
test_str_eq_nullptr()
{
  TEST_CHECK(str::eq(nullptr, nullptr) == true);
  TEST_CHECK(str::eq(nullptr, "") == false);
  TEST_CHECK(str::eq("", nullptr) == false);
  TEST_CHECK(str::eq(nullptr, "hello") == false);
  TEST_CHECK(str::eq("hello", nullptr) == false);
  TEST_CHECK(str::eq(nullptr, nullptr) == true);
}

void
test_str_startswith_char_nullptr()
{
  TEST_CHECK(str::startswith((const char*)nullptr, "foo") == false);
  TEST_CHECK(str::startswith("foo", (const char*)nullptr) == false);
  TEST_CHECK(str::startswith((const char*)nullptr, (const char*)nullptr) == false);
  TEST_CHECK(str::startswith("foobar", "foo") == true);
  TEST_CHECK(str::startswith("foobar", "bar") == false);
  TEST_CHECK(str::startswith("", "") == true);
}

void
test_str_from_u64_suffixes()
{
  u64 val;

  TEST_CHECK(str::from("0", &val) == 0);
  TEST_CHECK(val == 0);

  TEST_CHECK(str::from("1024", &val) == 0);
  TEST_CHECK(val == 1024);

  TEST_CHECK(str::from("1K", &val) == 0);
  TEST_CHECK(val == 1024);
  TEST_CHECK(str::from("1k", &val) == 0);
  TEST_CHECK(val == 1024);

  TEST_CHECK(str::from("1M", &val) == 0);
  TEST_CHECK(val == 1024 * 1024);
  TEST_CHECK(str::from("1m", &val) == 0);
  TEST_CHECK(val == 1024 * 1024);

  TEST_CHECK(str::from("1G", &val) == 0);
  TEST_CHECK(val == 1024ULL * 1024ULL * 1024ULL);
  TEST_CHECK(str::from("1g", &val) == 0);
  TEST_CHECK(val == 1024ULL * 1024ULL * 1024ULL);

  TEST_CHECK(str::from("1T", &val) == 0);
  TEST_CHECK(val == 1024ULL * 1024ULL * 1024ULL * 1024ULL);
  TEST_CHECK(str::from("1t", &val) == 0);
  TEST_CHECK(val == 1024ULL * 1024ULL * 1024ULL * 1024ULL);

  TEST_CHECK(str::from("1024B", &val) == 0);
  TEST_CHECK(val == 1024);
  TEST_CHECK(str::from("1024b", &val) == 0);
  TEST_CHECK(val == 1024);

  TEST_CHECK(str::from("2K", &val) == 0);
  TEST_CHECK(val == 2048);
  TEST_CHECK(str::from("4M", &val) == 0);
  TEST_CHECK(val == 4ULL * 1024 * 1024);

  TEST_CHECK(str::from("1P", &val) == -EINVAL);
  TEST_CHECK(str::from("hello", &val) == -EINVAL);
  TEST_CHECK(str::from("", &val) == -EINVAL);

  TEST_CHECK(str::from("16777216T", &val) == -EOVERFLOW);

  TEST_CHECK(str::from("9999999999999999999999", &val) == -EINVAL);
}

void
test_str_from_s64()
{
  s64 val;

  TEST_CHECK(str::from("0", &val) == 0);
  TEST_CHECK(val == 0);

  TEST_CHECK(str::from("-1", &val) == 0);
  TEST_CHECK(val == -1);

  TEST_CHECK(str::from("1K", &val) == 0);
  TEST_CHECK(val == 1024);

  TEST_CHECK(str::from("1M", &val) == 0);
  TEST_CHECK(val == 1024 * 1024);

  TEST_CHECK(str::from("1G", &val) == 0);
  TEST_CHECK(val == 1024LL * 1024 * 1024);

  TEST_CHECK(str::from("1T", &val) == 0);
  TEST_CHECK(val == 1024LL * 1024 * 1024 * 1024);

  TEST_CHECK(str::from("1P", &val) == -EINVAL);
  TEST_CHECK(str::from("abc", &val) == -EINVAL);
}

void
test_str_from_int()
{
  int val;

  TEST_CHECK(str::from("0", &val) == 0);
  TEST_CHECK(val == 0);

  TEST_CHECK(str::from("123", &val) == 0);
  TEST_CHECK(val == 123);

  TEST_CHECK(str::from("-456", &val) == 0);
  TEST_CHECK(val == -456);

  TEST_CHECK(str::from("1K", &val) == 0);
  TEST_CHECK(val == 1);
  TEST_CHECK(str::from("abc", &val) == -EINVAL);
}

void
test_num_humanize()
{
  TEST_CHECK(num::humanize(0) == "0");
  TEST_CHECK(num::humanize(512) == "512");
  TEST_CHECK(num::humanize(1024) == "1K");
  TEST_CHECK(num::humanize(2048) == "2K");
  TEST_CHECK(num::humanize(1024 * 1024) == "1M");
  TEST_CHECK(num::humanize(2 * 1024 * 1024) == "2M");
  TEST_CHECK(num::humanize(1024ULL * 1024 * 1024) == "1G");
  TEST_CHECK(num::humanize(2ULL * 1024 * 1024 * 1024) == "2G");
  TEST_CHECK(num::humanize(1024ULL * 1024 * 1024 * 1024) == "1T");
  TEST_CHECK(num::humanize(2ULL * 1024 * 1024 * 1024 * 1024) == "2T");
  TEST_CHECK(num::humanize(1536) == "1536");
  TEST_CHECK(num::humanize(1025) == "1025");
}

struct RmdirErr
{
private:
  std::optional<int> _err;

public:
  RmdirErr() {}

  operator int()
  {
    return (_err.has_value() ? _err.value() : -ENOENT);
  }

  RmdirErr&
  operator=(int v_)
  {
    if(!_err.has_value())
      _err = ((v_ >= 0) ? 0 : v_);
    else if((v_ == -EEXIST) || (v_ == -ENOTEMPTY))
      _err = v_;
    else if((*_err == -EEXIST) || (*_err == -ENOTEMPTY))
      ;
    else if(v_ >= 0)
      _err = 0;
    else
      _err = v_;

    return *this;
  }

  bool
  operator==(int v_)
  {
    if(_err.has_value())
      return (_err.value() == v_);
    return false;
  }
};

void
test_rmdir_err_default_is_enoent()
{
  RmdirErr err;
  TEST_CHECK((int)err == -ENOENT);
}

void
test_rmdir_err_first_success_sets_zero()
{
  RmdirErr err;
  err = 0;
  TEST_CHECK((int)err == 0);
}

void
test_rmdir_err_first_error_sets_value()
{
  RmdirErr err;
  err = -EACCES;
  TEST_CHECK((int)err == -EACCES);
}

void
test_rmdir_err_enotempty_overwrites_other()
{
  RmdirErr err;
  err = -EACCES;
  err = -ENOTEMPTY;
  TEST_CHECK((int)err == -ENOTEMPTY);
}

void
test_rmdir_err_eexist_overwrites_other()
{
  RmdirErr err;
  err = -EACCES;
  err = -EEXIST;
  TEST_CHECK((int)err == -EEXIST);
}

void
test_rmdir_err_enotempty_not_overwritten_by_other()
{
  RmdirErr err;
  err = -ENOTEMPTY;
  err = -EACCES;
  TEST_CHECK((int)err == -ENOTEMPTY);
}

void
test_rmdir_err_eexist_not_overwritten_by_other()
{
  RmdirErr err;
  err = -EEXIST;
  err = -EACCES;
  TEST_CHECK((int)err == -EEXIST);
}

void
test_rmdir_err_success_does_not_overwrite_priority()
{
  RmdirErr err;
  err = -ENOTEMPTY;
  err = 0;
  TEST_CHECK((int)err == -ENOTEMPTY);

  RmdirErr err2;
  err2 = -EEXIST;
  err2 = 0;
  TEST_CHECK((int)err2 == -EEXIST);
}

void
test_rmdir_err_success_overwrites_generic_error()
{
  RmdirErr err;
  err = -EACCES;
  err = 0;
  TEST_CHECK((int)err == 0);
}

void
test_rmdir_err_enotempty_overwrites_enotempty()
{
  RmdirErr err;
  err = -ENOTEMPTY;
  err = -EEXIST;
  TEST_CHECK((int)err == -EEXIST);
}

void
test_hashset_inline_to_heap_growth()
{
  HashSet set;

  for(int i = 0; i < 20; ++i)
    {
      char buf[32];
      snprintf(buf, sizeof(buf), "key_%d", i);
      TEST_CHECK(set.put(buf) == 1);
    }

  TEST_CHECK(set.size() == 20);

  for(int i = 0; i < 20; ++i)
    {
      char buf[32];
      snprintf(buf, sizeof(buf), "key_%d", i);
      TEST_CHECK(set.put(buf) == 0);
    }

  TEST_CHECK(set.size() == 20);
}

void
test_hashset_empty_string()
{
  HashSet set;

  TEST_CHECK(set.put("") == 1);
  TEST_CHECK(set.put("") == 0);
  TEST_CHECK(set.size() == 1);
}

void
test_hashset_many_items()
{
  HashSet set;

  for(int i = 0; i < 100; ++i)
    {
      char buf[32];
      snprintf(buf, sizeof(buf), "longer_key_name_%d", i);
      TEST_CHECK(set.put(buf) == 1);
    }

  TEST_CHECK(set.size() == 100);

  for(int i = 0; i < 100; ++i)
    {
      char buf[32];
      snprintf(buf, sizeof(buf), "longer_key_name_%d", i);
      TEST_CHECK(set.put(buf) == 0);
    }

  TEST_CHECK(set.size() == 100);
}

void
test_fs_inode_set_algo_valid()
{
  const std::vector<std::string> algos = {
    "passthrough",
    "path-hash",
    "path-hash32",
    "devino-hash",
    "devino-hash32",
    "hybrid-hash",
    "hybrid-hash32",
  };

  for(const auto &algo : algos)
    {
      TEST_CHECK(fs::inode::set_algo(algo) == 0);
      TEST_CHECK(fs::inode::get_algo() == algo);
    }
}

void
test_fs_inode_set_algo_invalid()
{
  TEST_CHECK(fs::inode::set_algo("nope") == -EINVAL);
  TEST_CHECK(fs::inode::set_algo("") == -EINVAL);
  TEST_CHECK(fs::inode::set_algo("HYBRID-HASH") == -EINVAL);
}

void
test_fs_inode_passthrough_returns_raw_ino()
{
  TEST_CHECK(fs::inode::set_algo("passthrough") == 0);

  const fs::path branch("/mnt/disk1");
  const fs::path fusepath("some/file");

  TEST_CHECK(fs::inode::calc(branch, fusepath, S_IFREG, 42) == 42);
  TEST_CHECK(fs::inode::calc(branch, fusepath, S_IFDIR, 999) == 999);
  TEST_CHECK(fs::inode::calc(branch, fusepath, S_IFREG, 1) == 1);
}

void
test_fs_inode_different_algos_produce_distinct_inodes()
{
  const fs::path branch("/mnt/disk1");
  const fs::path fusepath("some/file");
  const std::vector<std::string> algos = {
    "path-hash",
    "devino-hash",
    "hybrid-hash",
  };

  std::vector<u64> inodes;
  for(const auto &algo : algos)
    {
      TEST_CHECK(fs::inode::set_algo(algo) == 0);
      inodes.push_back(fs::inode::calc(branch, fusepath, S_IFREG, 12345));
    }

  for(size_t i = 1; i < inodes.size(); ++i)
    TEST_CHECK(inodes[i] != inodes[0]);
}

void
test_rnd_rand64_basic()
{
  u64 v1 = RND::rand64();
  u64 v2 = RND::rand64();
  TEST_CHECK(v1 != v2);
}

void
test_rnd_rand64_max()
{
  for(int i = 0; i < 1000; ++i)
    {
      u64 v = RND::rand64(100);
      TEST_CHECK(v < 100);
    }
}

void
test_rnd_rand64_range()
{
  for(int i = 0; i < 1000; ++i)
    {
      u64 v = RND::rand64(10, 50);
      TEST_CHECK(v >= 10);
      TEST_CHECK(v < 50);
    }
}

void
test_rnd_shrink_to_rand_elem()
{
  std::vector<int> v = {1};
  RND::shrink_to_rand_elem(v);
  TEST_CHECK(v.size() == 1);

  std::vector<int> v2;
  RND::shrink_to_rand_elem(v2);
  TEST_CHECK(v2.empty());
}

void
test_branch_copy_assignment_relinks_default_minfreespace()
{
  uint64_t default_a = 1234;
  uint64_t default_b = 5678;
  Branches::Impl impl_a(&default_a);
  Branches::Impl impl_b(&default_b);

  TEST_CHECK(impl_a.from_string("/a") == 0);
  TEST_CHECK(impl_b.from_string("/b") == 0);

  impl_a = impl_b;

  TEST_CHECK(impl_a.size() == 1);
  for(auto &branch : impl_a)
    {
      if(std::holds_alternative<const u64*>(branch._minfreespace))
        TEST_CHECK(std::get<const u64*>(branch._minfreespace) == &default_a);
    }
}

void
test_branch_move_assignment_relinks_default_minfreespace()
{
  uint64_t default_a = 1234;
  uint64_t default_b = 5678;
  Branches::Impl impl_a(&default_a);
  Branches::Impl impl_b(&default_b);

  TEST_CHECK(impl_a.from_string("/a") == 0);
  TEST_CHECK(impl_b.from_string("/b") == 0);

  impl_a = std::move(impl_b);

  TEST_CHECK(impl_a.size() == 1);
  for(auto &branch : impl_a)
    {
      if(std::holds_alternative<const u64*>(branch._minfreespace))
        TEST_CHECK(std::get<const u64*>(branch._minfreespace) == &default_a);
    }
}

void
test_rapidhash_withSeed_preserves_default_output()
{
  uint8_t buffer[192];
  const uint64_t seeds[] = {
    0,
    1,
    0x0123456789abcdefull,
    0xffffffffffffffffull,
    0xa0761d6478bd642full,
  };

  for(size_t i = 0; i < sizeof(buffer); ++i)
    buffer[i] = (uint8_t)(((i * 131u) + 17u) & 0xffu);

  for(size_t offset = 0; offset < 16; ++offset)
    {
      const uint8_t *key = buffer + offset;

      for(size_t len = 0; len <= 160; ++len)
        {
          for(const uint64_t seed : seeds)
            {
              const uint64_t canonical =
                rapidhash_internal(key,len,seed,rapid_secret);

              TEST_CHECK(rapidhash_withSeed(key,len,seed) == canonical);

              if(len <= 48)
                TEST_CHECK(rapidhashNano_withSeed(key,len,seed) == canonical);

              if(len <= 80)
                TEST_CHECK(rapidhashMicro_withSeed(key,len,seed) == canonical);
            }
        }
    }
}

TEST_LIST =
  {
   {"nop",test_nop},
   {"config_bool",test_config_bool},
   {"config_uint64",test_config_uint64},
   {"config_int",test_config_int},
   {"config_str",test_config_str},
   {"hashset_put_and_size",test_hashset_put_and_size},
   {"rapidhash_withSeed_preserves_default_output",test_rapidhash_withSeed_preserves_default_output},
   {"config_branches",test_config_branches},
   {"branch_default_ctor_mode",test_branch_default_ctor_mode},
   {"branch_to_string_modes",test_branch_to_string_modes},
   {"branch_to_string_with_minfreespace",test_branch_to_string_with_minfreespace},
   {"branch_mode_predicates",test_branch_mode_predicates},
   {"branch_minfreespace_pointer_vs_value",test_branch_minfreespace_pointer_vs_value},
   {"branch_copy_constructor",test_branch_copy_constructor},
   {"branch_copy_assignment",test_branch_copy_assignment},
   {"branches_default_minfreespace",test_branches_default_minfreespace},
   {"branches_per_branch_minfreespace_overrides_default",test_branches_per_branch_minfreespace_overrides_default},
   {"branches_add_end_operator",test_branches_add_end_operator},
   {"branches_add_begin_operator",test_branches_add_begin_operator},
   {"branches_set_operator",test_branches_set_operator},
   {"branches_erase_end_operator",test_branches_erase_end_operator},
   {"branches_erase_begin_operator",test_branches_erase_begin_operator},
   {"branches_erase_fnmatch_operator",test_branches_erase_fnmatch_operator},
   {"branches_erase_fnmatch_multiple_patterns",test_branches_erase_fnmatch_multiple_patterns},
   {"branches_invalid_mode",test_branches_invalid_mode},
   {"branches_invalid_minfreespace",test_branches_invalid_minfreespace},
   {"branches_to_paths",test_branches_to_paths},
   {"branches_cow_semantics",test_branches_cow_semantics},
   {"branches_mode_round_trip",test_branches_mode_round_trip},
   {"branches_srcmounts_to_string",test_branches_srcmounts_to_string},
   {"branches_srcmounts_empty",test_branches_srcmounts_empty},
   {"branch_to_string_1t",test_branch_to_string_1t},
   {"branch_to_string_non_exact_multiple",test_branch_to_string_non_exact_multiple},
   {"branch_to_string_zero_owned_minfreespace",test_branch_to_string_zero_owned_minfreespace},
   {"branch_copy_pointer_aliasing",test_branch_copy_pointer_aliasing},
   {"branches_mode_case_sensitivity",test_branches_mode_case_sensitivity},
   {"branches_minfreespace_lowercase_suffixes",test_branches_minfreespace_lowercase_suffixes},
   {"branches_minfreespace_bytes_suffix",test_branches_minfreespace_bytes_suffix},
   {"branches_minfreespace_zero",test_branches_minfreespace_zero},
   {"branches_minfreespace_negative_invalid",test_branches_minfreespace_negative_invalid},
   {"branches_minfreespace_overflow",test_branches_minfreespace_overflow},
   {"branches_minfreespace_round_trip",test_branches_minfreespace_round_trip},
   {"branches_minfreespace_round_trip_1t",test_branches_minfreespace_round_trip_1t},
   {"branches_clear_with_equals_alone",test_branches_clear_with_equals_alone},
   {"branches_empty_string_clears_same_as_equals",test_branches_empty_string_clears_same_as_equals},
   {"branches_plus_and_plus_greater_equivalent",test_branches_plus_and_plus_greater_equivalent},
   {"branches_unknown_instruction_einval",test_branches_unknown_instruction_einval},
   {"branches_empty_options_after_equals_einval",test_branches_empty_options_after_equals_einval},
   {"branches_path_with_equals_in_name",test_branches_path_with_equals_in_name},
   {"branches_add_end_multiple_paths",test_branches_add_end_multiple_paths},
   {"branches_add_begin_multiple_paths",test_branches_add_begin_multiple_paths},
   {"branches_erase_empty_pattern_noop",test_branches_erase_empty_pattern_noop},
   {"branches_erase_wildcard_removes_all",test_branches_erase_wildcard_removes_all},
   {"branches_erase_nonmatching_pattern_noop",test_branches_erase_nonmatching_pattern_noop},
   {"branches_erase_multi_pattern_first_miss_second_hit",test_branches_erase_multi_pattern_first_miss_second_hit},
   {"branches_erase_end_single_element",test_branches_erase_end_single_element},
   {"branches_erase_begin_single_element",test_branches_erase_begin_single_element},
   {"branches_erase_end_empty_returns_enoent",test_branches_erase_end_empty_returns_enoent},
   {"branches_erase_begin_empty_returns_enoent",test_branches_erase_begin_empty_returns_enoent},
   {"branches_clear_then_add",test_branches_clear_then_add},
   {"branches_add_begin_then_end_then_erase_both",test_branches_add_begin_then_end_then_erase_both},
   {"branches_error_in_multipath_add_leaves_list_unchanged",test_branches_error_in_multipath_add_leaves_list_unchanged},
   {"branches_error_in_set_leaves_list_unchanged",test_branches_error_in_set_leaves_list_unchanged},
   {"branches_global_minfreespace_live_tracking",test_branches_global_minfreespace_live_tracking},
   {"branches_srcmounts_from_string_noop",test_branches_srcmounts_from_string_noop},
   {"branches_srcmounts_reflects_mutations",test_branches_srcmounts_reflects_mutations},
   {"config_cachefiles",test_config_cachefiles},
   {"config_inodecalc",test_config_inodecalc},
   {"fs_inode_readdir_calc_matches_calc",test_fs_inode_readdir_calc_matches_calc},
   {"config_moveonenospc",test_config_moveonenospc},
   {"config_nfsopenhack",test_config_nfsopenhack},
   {"config_readdir",test_config_readdir},
   {"config_statfs",test_config_statfs},
   {"config_statfsignore",test_config_statfs_ignore},
   {"config_xattr",test_config_xattr},
   {"config",test_config},
   {"config_has_key",test_config_has_key},
   {"config_keys",test_config_keys},
   {"config_keys_xattr",test_config_keys_xattr},
   {"config_keys_listxattr_size",test_config_keys_listxattr_size},
   {"config_keys_listxattr_fill",test_config_keys_listxattr_fill},
   {"config_keys_listxattr_erange",test_config_keys_listxattr_erange},
   {"config_get_set_roundtrip",test_config_get_set_roundtrip},
   {"config_get_unknown_key",test_config_get_unknown_key},
   {"config_set_unknown_key",test_config_set_unknown_key},
   {"config_set_invalid_value",test_config_set_invalid_value},
   {"config_set_underscore_normalization",test_config_set_underscore_normalization},
   {"config_get_underscore_normalization",test_config_get_underscore_normalization},
   {"config_set_kv_form",test_config_set_kv_form},
   {"config_remember_nodes_bool_numeric",test_config_remember_nodes_bool_numeric},
   {"config_readonly_before_init",test_config_readonly_before_init},
   {"config_readonly_after_init",test_config_readonly_after_init},
   {"config_mutable_after_init",test_config_mutable_after_init},
   {"config_from_stream_basic",test_config_from_stream_basic},
   {"config_from_stream_comments_and_blanks",test_config_from_stream_comments_and_blanks},
   {"config_from_stream_bad_lines",test_config_from_stream_bad_lines},
   {"config_from_file_nonexistent",test_config_from_file_nonexistent},
   {"config_is_rootdir",test_config_is_rootdir},
   {"config_is_ctrl_file",test_config_is_ctrl_file},
   {"config_is_mergerfs_xattr",test_config_is_mergerfs_xattr},
   {"config_is_cmd_xattr",test_config_is_cmd_xattr},
    {"config_prune_ctrl_xattr",test_config_prune_ctrl_xattr},
    {"config_prune_cmd_xattr",test_config_prune_cmd_xattr},
    {"fs_copyfile_basic",test_fs_copyfile_basic},
    {"fs_copyfile_source_changes_cleanup_tmpfiles",test_fs_copyfile_source_changes_cleanup_tmpfiles},
    {"str_eq_nullptr",test_str_eq_nullptr},
    {"str_startswith_char_nullptr",test_str_startswith_char_nullptr},
    {"str_from_u64_suffixes",test_str_from_u64_suffixes},
    {"str_from_s64",test_str_from_s64},
    {"str_from_int",test_str_from_int},
    {"num_humanize",test_num_humanize},
    {"rmdir_err_default_is_enoent",test_rmdir_err_default_is_enoent},
    {"rmdir_err_first_success_sets_zero",test_rmdir_err_first_success_sets_zero},
    {"rmdir_err_first_error_sets_value",test_rmdir_err_first_error_sets_value},
    {"rmdir_err_enotempty_overwrites_other",test_rmdir_err_enotempty_overwrites_other},
    {"rmdir_err_eexist_overwrites_other",test_rmdir_err_eexist_overwrites_other},
    {"rmdir_err_enotempty_not_overwritten_by_other",test_rmdir_err_enotempty_not_overwritten_by_other},
    {"rmdir_err_eexist_not_overwritten_by_other",test_rmdir_err_eexist_not_overwritten_by_other},
    {"rmdir_err_success_does_not_overwrite_priority",test_rmdir_err_success_does_not_overwrite_priority},
    {"rmdir_err_success_overwrites_generic_error",test_rmdir_err_success_overwrites_generic_error},
    {"rmdir_err_enotempty_overwrites_enotempty",test_rmdir_err_enotempty_overwrites_enotempty},
    {"hashset_inline_to_heap_growth",test_hashset_inline_to_heap_growth},
    {"hashset_empty_string",test_hashset_empty_string},
    {"hashset_many_items",test_hashset_many_items},
    {"fs_inode_set_algo_valid",test_fs_inode_set_algo_valid},
    {"fs_inode_set_algo_invalid",test_fs_inode_set_algo_invalid},
    {"fs_inode_passthrough_returns_raw_ino",test_fs_inode_passthrough_returns_raw_ino},
    {"fs_inode_different_algos_produce_distinct_inodes",test_fs_inode_different_algos_produce_distinct_inodes},
    {"rnd_rand64_basic",test_rnd_rand64_basic},
    {"rnd_rand64_max",test_rnd_rand64_max},
    {"rnd_rand64_range",test_rnd_rand64_range},
    {"rnd_shrink_to_rand_elem",test_rnd_shrink_to_rand_elem},
    {"branch_copy_assignment_relinks_default_minfreespace",test_branch_copy_assignment_relinks_default_minfreespace},
    {"branch_move_assignment_relinks_default_minfreespace",test_branch_move_assignment_relinks_default_minfreespace},
    {"str",test_str_stuff},
   {"tp_construct_default",test_tp_construct_default},
   {"tp_construct_named",test_tp_construct_named},
   {"tp_construct_zero_threads_throws",test_tp_construct_zero_threads_throws},
   {"tp_enqueue_work",test_tp_enqueue_work},
   {"tp_enqueue_work_with_ptoken",test_tp_enqueue_work_with_ptoken},
   {"tp_try_enqueue_work",test_tp_try_enqueue_work},
   {"tp_try_enqueue_work_with_ptoken",test_tp_try_enqueue_work_with_ptoken},
   {"tp_try_enqueue_work_for",test_tp_try_enqueue_work_for},
   {"tp_try_enqueue_work_for_with_ptoken",test_tp_try_enqueue_work_for_with_ptoken},
   {"tp_enqueue_task_int",test_tp_enqueue_task_int},
   {"tp_enqueue_task_void",test_tp_enqueue_task_void},
   {"tp_enqueue_task_string",test_tp_enqueue_task_string},
   {"tp_enqueue_task_exception",test_tp_enqueue_task_exception},
   {"tp_enqueue_task_multiple",test_tp_enqueue_task_multiple},
   {"tp_threads_returns_correct_count",test_tp_threads_returns_correct_count},
   {"tp_threads_unique_ids",test_tp_threads_unique_ids},
   {"tp_add_thread",test_tp_add_thread},
   {"tp_remove_thread",test_tp_remove_thread},
   {"tp_remove_thread_refuses_last",test_tp_remove_thread_refuses_last},
   {"tp_set_threads_grow",test_tp_set_threads_grow},
   {"tp_set_threads_shrink",test_tp_set_threads_shrink},
   {"tp_set_threads_zero",test_tp_set_threads_zero},
   {"tp_set_threads_same",test_tp_set_threads_same},
   {"tp_work_after_add_thread",test_tp_work_after_add_thread},
   {"tp_work_after_remove_thread",test_tp_work_after_remove_thread},
   {"tp_worker_exception_no_crash",test_tp_worker_exception_no_crash},
   {"tp_concurrent_enqueue",test_tp_concurrent_enqueue},
   {"tp_ptoken_creation",test_tp_ptoken_creation},
   {"tp_destruction_drains_queue",test_tp_destruction_drains_queue},
   {"tp_move_only_callable",test_tp_move_only_callable},
   {"tp_work_ordering_single_thread",test_tp_work_ordering_single_thread},
   {"tp_try_enqueue_work_fails_when_full",test_tp_try_enqueue_work_fails_when_full},
   {"tp_try_enqueue_work_for_times_out_when_full",test_tp_try_enqueue_work_for_times_out_when_full},
   {"tp_enqueue_work_blocks_until_slot_available",test_tp_enqueue_work_blocks_until_slot_available},
   {"tp_worker_nonstd_exception_no_crash",test_tp_worker_nonstd_exception_no_crash},
   {"tp_remove_thread_under_load",test_tp_remove_thread_under_load},
   {"tp_set_threads_concurrent_with_enqueue",test_tp_set_threads_concurrent_with_enqueue},
   {"tp_repeated_resize_stress",test_tp_repeated_resize_stress},
   {"tp_many_producers_many_tasks_stress",test_tp_many_producers_many_tasks_stress},
   {"tp_heavy_mixed_resize_and_enqueue_stress",test_tp_heavy_mixed_resize_and_enqueue_stress},
   {"tp_heavy_try_enqueue_pressure",test_tp_heavy_try_enqueue_pressure},
   {"tp_heavy_repeated_construct_destroy",test_tp_heavy_repeated_construct_destroy},
   {"tp_heavy_enqueue_task_mixed_outcomes",test_tp_heavy_enqueue_task_mixed_outcomes},
   {"tp_heavy_add_remove_churn_under_enqueue",test_tp_heavy_add_remove_churn_under_enqueue},
   {NULL,NULL}
  };
