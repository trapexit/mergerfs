#include "acutest/acutest.h"

#include "config.hpp"
#include "str.hpp"
#include "thread_pool.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <numeric>
#include <thread>

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
