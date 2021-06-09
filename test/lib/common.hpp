/*
    +--------------------------------------------------------------------+
    | libmemcached - C/C++ Client Library for memcached                  |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted under the terms of the BSD license.    |
    | You should have received a copy of the license in a bundled file   |
    | named LICENSE; in case you did not receive a copy you can review   |
    | the terms online at: https://opensource.org/licenses/BSD-3-Clause  |
    +--------------------------------------------------------------------+
    | Copyright (c) 2006-2014 Brian Aker   https://datadifferential.com/ |
    | Copyright (c) 2020 Michael Wallner   <mike@php.net>                |
    +--------------------------------------------------------------------+
*/

#pragma once

#include <chrono>
#include <cstring>
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
#include "test/lib/env.hpp"
#include "test/lib/random.hpp"

#include "libmemcached/memcached.h"

using namespace std;
using socket_or_port_t = variant<string, int>;

/**
 * Useful macros for testing
 */
#define S(s) (s), strlen(s)
#define LOOPED_SECTION(tests) \
  for (auto &[name, test] : tests) DYNAMIC_SECTION("test " << name)
#define REQUIRE_SUCCESS(rc) \
  do { \
    REQUIRE_THAT(rc, test.returns.success()); \
  } while (0)
#define REQUIRE_RC(rc, call) \
  do { \
    REQUIRE_THAT(call, test.returns(rc)); \
  } while (0)

inline memcached_return_t fetch_all_results(memcached_st *memc, unsigned int &keys_returned,
                                            memcached_return_t &rc) {
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
    *copy(S(templ_) + templ_, fn) = '\0';
    fd = mkstemp(fn);
  }
  ~Tempfile() {
    close(fd);
    unlink(fn);
  }
  [[nodiscard]] int getFd() const { return fd; }
  [[nodiscard]] const char *getFn() const { return fn; }
  bool put(const char *buf, size_t len) const {
    return static_cast<ssize_t>(len) == write(fd, buf, len);
  }
  string get() const {
    string all;
    char buf[200];
    ssize_t len;

    lseek(fd, 0, SEEK_SET);
    while (0 < (len = read(fd, buf, sizeof(buf)))) {
      all.append(buf, len);
    }
    return all;
  }

private:
  char fn[80];
  int fd;
};

class MemcachedPtr {
public:
  memcached_st *memc;

  explicit MemcachedPtr(memcached_st *memc_) {
    memc = memc_;
  }
  MemcachedPtr()
  : MemcachedPtr(memcached_create(nullptr)) {}
  ~MemcachedPtr() {
    memcached_free(memc);
  }
  memcached_st *operator*() const {
    return memc;
  }
  auto operator->() const {
    return memc;
  }
};

template<class T>
class Malloced {
  T *ptr;

public:
  explicit Malloced(T *ptr_)
  : ptr{ptr_} {}
  Malloced &operator=(T *ptr_) {
    if (ptr)
      free(ptr);
    ptr = ptr_;
    return *this;
  }
  ~Malloced() {
    if (ptr)
      free(ptr);
  }
  auto operator*() const { return ptr; }
  auto operator->() const { return ptr; }
};
