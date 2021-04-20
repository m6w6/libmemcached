#include "test/lib/common.hpp"
#include "test/lib/MemcachedCluster.hpp"

TEST_CASE("memcached_touch") {
  auto test = MemcachedCluster::mixed();
  auto memc = &test.memc;
  memcached_return_t rc;
  auto proto = GENERATE(as<memcached_behavior_t>{}, 0, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, MEMCACHED_BEHAVIOR_META_PROTOCOL);

  if (proto) {
    REQUIRE_SUCCESS(memcached_behavior_set(memc, proto, 1));
  }

  if (proto == MEMCACHED_BEHAVIOR_META_PROTOCOL && !test.isGEVersion(1, 6)) {
    return;
  }

  DYNAMIC_SECTION("touch " << (proto ? libmemcached_string_behavior(proto) + sizeof("MEMCACHED_BEHAVIOR") : "ASCII_PROTOCOL")) {
    REQUIRE_FALSE(memcached_get(memc, S(__func__), nullptr, nullptr, &rc));
    REQUIRE_RC(MEMCACHED_NOTFOUND, rc);

    REQUIRE_SUCCESS(memcached_set(memc, S(__func__), S(__func__), 2, 0));

    Malloced val(memcached_get(memc, S(__func__), nullptr, nullptr, &rc));
    REQUIRE_SUCCESS(rc);
    REQUIRE(*val);

    REQUIRE_SUCCESS(memcached_touch(memc, S(__func__), 60));
    val = memcached_get(memc, S(__func__), nullptr, nullptr, &rc);
    REQUIRE_SUCCESS(rc);
    REQUIRE(*val);

    REQUIRE_SUCCESS(memcached_touch(memc, S(__func__), time(nullptr) - 2));
    val = memcached_get(memc, S(__func__), nullptr, nullptr, &rc);
    REQUIRE_RC(MEMCACHED_NOTFOUND, rc);
    REQUIRE_FALSE(*val);
  }

  DYNAMIC_SECTION("touch_by_key " << (proto ? libmemcached_string_behavior(proto) + sizeof("MEMCACHED_BEHAVIOR") : "ASCII_PROTOCOL")) {
    REQUIRE_RC(MEMCACHED_NOTFOUND, memcached_touch_by_key(memc, S(__func__), S(__func__), 60));
    REQUIRE_SUCCESS(memcached_set_by_key(memc, S(__func__), S(__func__), S(__func__), 2, 0));

    Malloced val(memcached_get_by_key(memc, S(__func__), S(__func__), nullptr, nullptr, &rc));
    REQUIRE_SUCCESS(rc);
    REQUIRE(*val);

    REQUIRE_SUCCESS(memcached_touch_by_key(memc, S(__func__), S(__func__), time(nullptr) - 2));
    val = memcached_get_by_key(memc, S(__func__), S(__func__), nullptr, nullptr, &rc);
    REQUIRE_RC(MEMCACHED_NOTFOUND, rc);
    REQUIRE_FALSE(*val);

    REQUIRE_RC(MEMCACHED_NOTFOUND, memcached_touch_by_key(memc, S(__func__), S(__func__), 60));
  }
}
