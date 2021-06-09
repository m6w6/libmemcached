/*
    +--------------------------------------------------------------------+
    | libmemcached-awesome - C/C++ Client Library for memcached          |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted under the terms of the BSD license.    |
    | You should have received a copy of the license in a bundled file   |
    | named LICENSE; in case you did not receive a copy you can review   |
    | the terms online at: https://opensource.org/licenses/BSD-3-Clause  |
    +--------------------------------------------------------------------+
    | Copyright (c) 2006-2014 Brian Aker   https://datadifferential.com/ |
    | Copyright (c) 2020-2021 Michael Wallner        https://awesome.co/ |
    +--------------------------------------------------------------------+
*/

#include "mem_config.h"

#define PROGRAM_NAME        "memslap"
#define PROGRAM_DESCRIPTION "Generate load against a cluster of memcached servers."
#define PROGRAM_VERSION     "1.1"

#define DEFAULT_INITIAL_LOAD   10000ul
#define DEFAULT_EXECUTE_NUMBER 10000ul
#define DEFAULT_CONCURRENCY    1ul

#include "common/options.hpp"
#include "common/checks.hpp"
#include "common/time.hpp"
#include "common/random.hpp"

#include <atomic>
#include <thread>
#include <iomanip>

static std::atomic_bool wakeup;

static memcached_return_t counter(const memcached_st *, memcached_result_st *, void *ctx) {
  auto c = static_cast<size_t *>(ctx);
  ++(*c);
  return MEMCACHED_SUCCESS;
}

struct keyval_st {
  struct data {
    std::vector<char *> chr;
    std::vector<size_t> len;
    explicit data(size_t num)
        : chr(num)
        , len(num)
    {}
  };

  data key;
  data val;

  size_t num;
  random64 rnd;

  explicit keyval_st(size_t num_)
  : key{num_}
  , val{num_}
  , num{num_}
  , rnd{}
  {
    for (auto i = 0u; i < num; ++i) {
      gen(key.chr[i], key.len[i], val.chr[i], val.len[i]);
    }
  }

  ~keyval_st() {
    for (auto i = 0u; i < num; ++i) {
      delete [] key.chr[i];
      delete [] val.chr[i];
    }
  }

private:
  void gen_key(char *&key_chr, size_t &key_len) {
    key_len = rnd(20,60);
    key_chr = new char[key_len + 1];
    rnd.fill(key_chr, key_len);
    key_chr[key_len] = 0;
  }
  void gen_val(const char *key_chr, const size_t key_len, char *&val_chr, size_t &val_len) {
    val_len = rnd(50, 5000);
    val_chr = new char[val_len];

    for (auto len = 0u; len < val_len; len += key_len) {
      auto val_pos = val_chr + len;
      auto rem_len = len + key_len > val_len ? val_len % key_len : key_len;
      memcpy(val_pos, key_chr, rem_len);
    }
  }
  void gen(char *&key_chr, size_t &key_len, char *&val_chr, size_t &val_len) {
    gen_key(key_chr, key_len);
    gen_val(key_chr, key_len, val_chr, val_len);
  }
};

static size_t execute_get(const client_options &opt, memcached_st &memc, const keyval_st &kv) {
  size_t retrieved = 0;
  random64 rnd{};

  for (auto i = 0u; i < kv.num; ++i) {
    memcached_return_t rc;
    auto r = rnd(0, kv.num);
    free(memcached_get(&memc, kv.key.chr[r], kv.key.len[r], nullptr, nullptr, &rc));

    if (check_return(opt, memc, kv.key.chr[r], rc)) {
      ++retrieved;
    }
  }

  return retrieved;
}

static size_t execute_mget(const client_options &opt, memcached_st &memc, const keyval_st &kv) {
  size_t retrieved = 0;
  memcached_execute_fn cb[] = {&counter};
  memcached_return_t rc;

  if (memcached_is_binary(&memc)) {
    rc = memcached_mget_execute(&memc, kv.key.chr.data(), kv.key.len.data(), kv.num, cb,
                                     &retrieved, 1);

    while (rc != MEMCACHED_END && memcached_success(rc)) {
      rc = memcached_fetch_execute(&memc, cb, &retrieved, 1);
    }
  } else {
    memcached_result_st res;
    memcached_result_create(&memc, &res);

    rc = memcached_mget(&memc, kv.key.chr.data(), kv.key.len.data(), kv.num);

    while (rc != MEMCACHED_END && memcached_success(rc)) {
      if (memcached_fetch_result(&memc, &res, &rc)) {
        ++retrieved;
      }
    }
    memcached_result_free(&res);
  }
  if (memcached_fatal(rc)) {
    if (!opt.isset("quiet")) {
      std::cerr << "Failed mget: " << memcached_strerror(&memc, rc) << ": "
                << memcached_last_error_message(&memc);
    }
  }
  return retrieved;
}

static size_t execute_set(const client_options &opt, memcached_st &memc, const keyval_st &kv) {
  for (auto i = 0u; i < kv.num; ++i) {
    auto rc = memcached_set(&memc, kv.key.chr[i], kv.key.len[i], kv.val.chr[i], kv.val.len[i], 0, 0);

    if (!check_return(opt, memc, kv.key.chr[i], rc)) {
      memcached_quit(&memc);
      return i;
    }
  }

  return kv.num;
}

class thread_context {
public:
  thread_context(const client_options &opt_, const memcached_st &memc_, const keyval_st &kv_)
  : opt{opt_}
  , kv{kv_}
  , count{}
  , root(memc_)
  , memc{}
  , thread([this]{ execute(); })
  {}

  ~thread_context() {
    memcached_free(&memc);
  }

  size_t complete() {
    thread.join();
    return count;
  }

private:
  const client_options &opt;
  const keyval_st &kv;
  size_t count;
  const memcached_st &root;
  memcached_st memc;
  std::thread thread;

  void execute() {
    memcached_clone(&memc, &root);

    while (!wakeup.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    if (!strcmp("set", opt.argof("test"))) {
      count = execute_set(opt, memc, kv);
    } else if (!strcmp("mget", opt.argof("test"))) {
      count = execute_mget(opt, memc, kv);
    } else {
      if (strcmp("get", opt.argof("test"))) {
        if (!opt.isset("quiet")) {
          std::cerr << "Unknown --test: '" << opt.argof("test") << "'.\n";
        }
      }
      count = execute_get(opt, memc, kv);
    }
  }
};

using opt_apply = std::function<bool(const client_options &, const client_options::extended_option &ext, memcached_st *)>;

static opt_apply wrap_stoul(unsigned long &ul) {
  return [&ul](const client_options &, const client_options::extended_option &ext, memcached_st *) {
    if (ext.arg && *ext.arg) {
      auto c = std::stoul(ext.arg);
      if (c) {
        ul = c;
      }
    }
    return true;
  };
}

static std::ostream &align(std::ostream &io) {
  return io << std::right << std::setw(8);
}

int main(int argc, char *argv[]) {
  client_options opt{PROGRAM_NAME, PROGRAM_VERSION, PROGRAM_DESCRIPTION};
  auto concurrency = DEFAULT_CONCURRENCY;
  auto load_count = DEFAULT_INITIAL_LOAD;
  auto test_count = DEFAULT_EXECUTE_NUMBER;

  for (const auto &def : opt.defaults) {
    opt.add(def);
  }

  opt.add("noreply", 'R', no_argument, "Enable the NOREPLY behavior for storage commands.")
      .apply = [](const client_options &opt_, const client_options::extended_option &ext, memcached_st *memc) {
    if (MEMCACHED_SUCCESS != memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_NOREPLY, ext.set)) {
      if (!opt_.isset("quiet")) {
        std::cerr << memcached_last_error_message(memc);
      }
      return false;
    }
    return true;
  };
  opt.add("udp", 'U', no_argument, "Use UDP.")
      .apply = [](const client_options &opt_, const client_options::extended_option &ext, memcached_st *memc) {
    if (MEMCACHED_SUCCESS != memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_USE_UDP, ext.set)) {
      if (!opt_.isset("quiet")) {
        std::cerr << memcached_last_error_message(memc) << "\n";
      }
      return false;
    }
    return true;
  };
  opt.add("flush", 'F', no_argument, "Flush all servers prior test.");
  opt.add("test", 't', required_argument, "Test to perform (options: get,mget,set; default: get).");
  opt.add("concurrency", 'c', required_argument, "Concurrency (number of threads to start; default: 1).")
      .apply = wrap_stoul(concurrency);
  opt.add("execute-number", 'e', required_argument, "Number of times to execute the tests (default: 10000).")
      .apply = wrap_stoul(test_count);
  opt.add("initial-load", 'l', required_argument, "Number of keys to load before executing tests (default: 10000)."
                                                  "\n\t\tDEPRECATED: --execute-number takes precedence.")
      .apply = wrap_stoul(load_count);

  char set[] = "set";
  opt.set("test", true, set);

  if (!opt.parse(argc, argv)) {
    exit(EXIT_FAILURE);
  }

  memcached_st memc;
  if (!check_memcached(opt, memc)) {
    exit(EXIT_FAILURE);
  }

  if (!opt.apply(&memc)) {
    memcached_free(&memc);
    exit(EXIT_FAILURE);
  }

  auto total_start = time_clock::now();
  std::cout << std::fixed << std::setprecision(3);

  if (opt.isset("flush")) {
    if (opt.isset("verbose")) {
      std::cout << "- Flushing servers ...\n";
    }
    auto flush_start = time_clock::now();
    auto rc = memcached_flush(&memc, 0);
    auto flush_elapsed = time_clock::now() - flush_start;
    if (!memcached_success(rc)) {
      if (!opt.isset("quiet")) {
        std::cerr << "Failed to FLUSH: " << memcached_last_error_message(&memc) << "\n";
      }
      memcached_free(&memc);
      exit(EXIT_FAILURE);
    }
    if (!opt.isset("quiet")) {
      std::cout << "Time to flush      " << align
                << memcached_server_count(&memc)
                << " servers:               "
                << align << time_format(flush_elapsed).count()
                << " seconds.\n";
    }
  }

  if (opt.isset("verbose")) {
    std::cout << "- Generating random test data ...\n";
  }
  auto keyval_start = time_clock::now();
  keyval_st kv{test_count};
  auto keyval_elapsed = time_clock::now() - keyval_start;

  if (!opt.isset("quiet")) {
    std::cout << "Time to generate   "
              << align << test_count
              << " test keys:             "
              << align << time_format(keyval_elapsed).count()
              << " seconds.\n";
  }

  if (strcmp(opt.argof("test"), "set")) {
    if (opt.isset("verbose")) {
      std::cout << "- Feeding initial load to servers ...\n";
    }
    auto feed_start = time_clock::now();
    auto count = execute_set(opt, memc, kv);
    check_return(opt, memc, memcached_flush_buffers(&memc));
    auto feed_elapsed = time_clock::now() - feed_start;

    if (!opt.isset("quiet")) {
      std::cout << "Time to set        "
                << align << count
                << " keys:                  "
                << align << time_format(feed_elapsed).count()
                << " seconds.\n";
      }
  }

  if (opt.isset("verbose")) {
    std::cout << "- Starting " << concurrency << " threads ...\n";
  }
  auto thread_start = time_clock::now();
  std::vector<thread_context *> threads{};
  threads.reserve(concurrency);
  for (auto i = 0ul; i < concurrency; ++i) {
    threads.push_back(new thread_context(opt, memc, kv));
  }
  auto thread_elapsed = time_clock::now() - thread_start;
  if (!opt.isset("quiet")) {
    std::cout << "Time to start      "
              << align << concurrency
              << " threads:                  "
              << time_format(thread_elapsed).count()
              << " seconds.\n";
  }
  if (opt.isset("verbose")) {
    std::cout << "- Starting test: " << test_count
              << " x " << opt.argof("test")
              << " x " << concurrency
              << " ...\n";
  }
  auto count = 0ul;
  auto test_start = time_clock::now();
  wakeup.store(true, std::memory_order_release);
  for (auto &thread : threads) {
    count += thread->complete();
    delete thread;
  }
  auto test_elapsed = time_clock::now() - test_start;

  if (!opt.isset("quiet")) {
    std::cout << "--------------------------------------------------------------------\n"
              << "Time to " << std::left << std::setw(4)
              << opt.argof("test") << "       "
              << align << count
              << " keys by "
              << std::setw(4)
              << concurrency << " threads:  "
              << align << time_format(test_elapsed).count()
              << " seconds.\n";

    std::cout << "--------------------------------------------------------------------\n"
              << "Time total:                                    "
              << align << std::setw(12)
              << time_format(time_clock::now() - total_start).count()
              << " seconds.\n";
  }
  exit(EXIT_SUCCESS);
}
