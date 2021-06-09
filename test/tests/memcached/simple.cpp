#include "test/lib/common.hpp"
#include "test/lib/MemcachedCluster.hpp"

TEST_CASE("memcached_simple") {
  pair<string, MemcachedCluster> tests[] = {
      {"network", MemcachedCluster::network()},
      {"socket", MemcachedCluster::socket()}
  };

  LOOPED_SECTION(tests) {
    auto memc = &test.memc;

    SECTION("flush") {
      uint64_t id = memcached_query_id(memc);
      REQUIRE_SUCCESS(memcached_flush(memc, 0));
      REQUIRE(id + 1 == memcached_query_id(memc));
    }

    uint64_t buffered = GENERATE(0, 1);
    auto proto = GENERATE(as<memcached_behavior_t>{}, 0, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, MEMCACHED_BEHAVIOR_META_PROTOCOL);

    REQUIRE_SUCCESS(memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_BUFFER_REQUESTS, buffered));
    if (proto) {
      REQUIRE_SUCCESS(memcached_behavior_set(memc, proto, 1));
    }

    if (proto == MEMCACHED_BEHAVIOR_META_PROTOCOL && !test.isGEVersion(1, 6)) {
      continue;
    }

    DYNAMIC_SECTION("set (buffered=" << buffered << ",proto=" << proto << ")") {
      for (auto i = 0; i < 10; ++i) {
        REQUIRE_RC(buffered ? MEMCACHED_BUFFERED : MEMCACHED_SUCCESS,
            memcached_set(memc, S(__func__), S(__func__), 0, 0));
      }
    }

    DYNAMIC_SECTION("add (buffered=" << buffered << " " << (proto ? libmemcached_string_behavior(proto) + sizeof("MEMCACHED_BEHAVIOR") : "ASCII_PROTOCOL") << ")") {
      memcached_return_t rc;

      Malloced empty(memcached_get(memc, S(__func__), nullptr, nullptr, &rc));
      REQUIRE_FALSE(*empty);
      REQUIRE_RC(MEMCACHED_NOTFOUND, rc);

      REQUIRE_RC(buffered ? MEMCACHED_BUFFERED : MEMCACHED_SUCCESS,
          memcached_set(memc, S(__func__), S(__func__), 0, 0));

      // flush any pending requests
      memcached_quit(memc);

      uint32_t flags;
      size_t len;
      Malloced val(memcached_get(memc, S(__func__), &len, &flags, &rc));

      REQUIRE(*val);
      REQUIRE(string(__func__) == string(*val, len));

      REQUIRE_RC(proto == MEMCACHED_BEHAVIOR_BINARY_PROTOCOL ? MEMCACHED_DATA_EXISTS : MEMCACHED_NOTSTORED,
          memcached_add(memc, S(__func__), S("update"), 0, 0));
    }

    DYNAMIC_SECTION("replace (buffered=" << buffered << " " << (proto ? libmemcached_string_behavior(proto) + sizeof("MEMCACHED_BEHAVIOR") : "ASCII_PROTOCOL") << ")") {
      REQUIRE_RC(buffered ? MEMCACHED_BUFFERED : MEMCACHED_SUCCESS,
          memcached_set(memc, S(__func__), S(__func__), 0, 0));

      REQUIRE_SUCCESS(memcached_replace(memc, S(__func__), S("replaced"), 0, 0));
    }

    DYNAMIC_SECTION("not found (buffered=" << buffered << " " << (proto ? libmemcached_string_behavior(proto) + sizeof("MEMCACHED_BEHAVIOR") : "ASCII_PROTOCOL") << ")") {
      memcached_return_t rc;
      Malloced val(memcached_get(memc, S("not-found"), nullptr, nullptr, &rc));
      REQUIRE_RC(MEMCACHED_NOTFOUND, rc);
      REQUIRE_FALSE(*val);

      val = memcached_get_by_key(memc, S("not-found"), S("not-found"), nullptr, nullptr, &rc);
      REQUIRE_RC(MEMCACHED_NOTFOUND, rc);
      REQUIRE_FALSE(*val);
    }

    DYNAMIC_SECTION("verbosity (buffered=" << buffered << " " << (proto ? libmemcached_string_behavior(proto) + sizeof("MEMCACHED_BEHAVIOR") : "ASCII_PROTOCOL") << ")") {
      REQUIRE_SUCCESS(memcached_verbosity(memc, 0));
    }

    DYNAMIC_SECTION("version (buffered=" << buffered << " " << (proto ? libmemcached_string_behavior(proto) + sizeof("MEMCACHED_BEHAVIOR") : "ASCII_PROTOCOL") << ")") {
      REQUIRE_SUCCESS(memcached_version(memc));
    }
  }
}
