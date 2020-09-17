#pragma once

#include <chrono>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <variant>
#include <vector>

#include "test/conf.h"
#include "test/lib/catch.hpp"
#include "test/lib/random.hpp"

#include "libmemcached/memcached.h"

using namespace std;
using socket_or_port_t = variant<string, int>;

/**
 * Useful macros for testing
 */
#define S(s) (s),strlen(s)
#define DECLARE_STREQUAL static auto strequal = equal_to<string>();
#define LOOPED_SECTION(tests) \
  for (auto &[name, test] : tests) DYNAMIC_SECTION("test " << name)
#define REQUIRE_SUCCESS(rc) do { \
    INFO("expected: SUCCESS");   \
    REQUIRE_THAT(rc, test.returns.success()); \
  } while(0)
#define REQUIRE_RC(rc, call) do { \
    INFO("expected: " << memcached_strerror(nullptr, rc)); \
    REQUIRE_THAT(call, test.returns(rc));                  \
  } while(0)

const char *getenv_else(const char *var, const char *defval);

inline memcached_return_t fetch_all_results(memcached_st *memc, unsigned int &keys_returned, memcached_return_t &rc) {
  keys_returned = 0;

  memcached_result_st *result = nullptr;
  while ((result = memcached_fetch_result(memc, result, &rc))) {
    REQUIRE(MEMCACHED_SUCCESS == rc);
    keys_returned += 1;
  }
  memcached_result_free(result);
  return MEMCACHED_SUCCESS;
}

inline memcached_return_t fetch_all_results(memcached_st *memc, unsigned int &keys_returned) {
  memcached_return_t rc;
  fetch_all_results(memc, keys_returned, rc);
  return rc;
}

#include <cstdlib>
#include <unistd.h>

class Tempfile {
public:
  explicit Tempfile(const char templ_[] = "memc.test.XXXXXX") {
    *copy(S(templ_)+templ_, fn) = '\0';
    fd = mkstemp(fn);
  }
  ~Tempfile() {
    close(fd);
    unlink(fn);
  }
  int getFd() const {
    return fd;
  }
  const char *getFn() const {
    return fn;
  }
  bool put(const char *buf, size_t len) const {
    return static_cast<ssize_t >(len) == write(fd, buf, len);
  }
private:
  char fn[80];
  int fd;
};

class MemcachedPtr {
public:
  memcached_st *memc;

  explicit
  MemcachedPtr(memcached_st *memc_) {
    memc = memc_;
  }
  MemcachedPtr()
  : MemcachedPtr(memcached_create(nullptr))
  {}
  ~MemcachedPtr() {
    memcached_free(memc);
  }
  memcached_st *operator * () {
    return memc;
  }
};

template<class T>
class Malloced {
  T *ptr;
public:
  explicit
  Malloced(T *ptr_)
  : ptr{ptr_}
  {}
  ~Malloced() {
    if(ptr)
      free(ptr);
  }
  auto operator *() {
    return ptr;
  }
  auto operator ->() {
    return ptr;
  }
};