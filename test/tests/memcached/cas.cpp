#include "test/lib/common.hpp"
#include "test/lib/MemcachedCluster.hpp"

TEST_CASE("memcached_cas") {
  pair<string, MemcachedCluster> tests[] = {
      {"network", MemcachedCluster::network()},
      {"socket", MemcachedCluster::socket()}
  };

  LOOPED_SECTION(tests) {
    auto memc = &test.memc;
    const char *keys[2] = {__func__, NULL};
    size_t keylengths[2] = {strlen(__func__), 0};
    auto proto = GENERATE(as<enum memcached_behavior_t>(), 0, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, MEMCACHED_BEHAVIOR_META_PROTOCOL);

    DYNAMIC_SECTION((proto ? libmemcached_string_behavior(proto) + sizeof("MEMCACHED_BEHAVIOR") : "ASCII_PROTOCOL")) {

      if (proto != MEMCACHED_BEHAVIOR_META_PROTOCOL || test.isGEVersion(1, 6)) {
        REQUIRE_SUCCESS(memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_SUPPORT_CAS, true));
        REQUIRE_SUCCESS(
            memcached_set(memc, S(__func__), S("we the people"), (time_t) 0, (uint32_t) 0));
        REQUIRE_SUCCESS(memcached_mget(memc, keys, keylengths, 1));

        memcached_result_st *results = memcached_result_create(memc, nullptr);
        REQUIRE(results);

        memcached_return_t rc;
        results = memcached_fetch_result(memc, results, &rc);
        REQUIRE(results);
        REQUIRE_SUCCESS(rc);

        REQUIRE(memcached_result_cas(results));
        REQUIRE("we the people"s
                == string(memcached_result_value(results), memcached_result_length(results)));

        uint64_t cas = memcached_result_cas(results);
        REQUIRE(
            memcached_success(memcached_cas(memc, S(__func__), S("change the value"), 0, 0, cas)));

        /*
       * The item will have a new cas value, so try to set it again with the old
       * value. This should fail!
         */
        REQUIRE_RC(MEMCACHED_DATA_EXISTS,
                   memcached_cas(memc, S(__func__), S("change the value"), 0, 0, cas));

        memcached_result_free(results);
      }
    }
  }
}
