#include "test/lib/common.hpp"
#include "test/lib/MemcachedCluster.hpp"

memcached_return_t dump_cb(const memcached_st *, const char *, size_t, void *ctx) {
  auto *c = reinterpret_cast<size_t *>(ctx);
  ++(*c);
  return MEMCACHED_SUCCESS;
}

TEST_CASE("memcached_dump") {
  pair<string, MemcachedCluster> tests[]{
    {"mixed", MemcachedCluster::mixed()},
    {"network", MemcachedCluster::network()},
    {"socket", MemcachedCluster::socket()}
  };

  LOOPED_SECTION(tests) {
    auto memc = &test.memc;
    auto meta = GENERATE(0, 1);

    DYNAMIC_SECTION("meta=" << meta) {
      if (!meta || test.isGEVersion(1, 6)) {
        SECTION("prepared with 64 KVs") {
          REQUIRE_SUCCESS(memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_META_PROTOCOL, meta));

          for (int i = 0; i < 64; ++i) {
            char key[8];
            int len = snprintf(key, sizeof(key) - 1, "k_%d", i);

            CHECKED_IF(len) {
              REQUIRE_SUCCESS(memcached_set(memc, key, len, key, len, 0, 0));
            }
          }

          memcached_quit(memc);

          // let memcached sort itself
          using namespace chrono_literals;
          this_thread::sleep_for(3s);

          SECTION("dumps 64 KVs") {
            size_t counter = 0;
            memcached_dump_fn fn[] = {dump_cb};

            REQUIRE_SUCCESS(memcached_dump(memc, fn, &counter, 1));
            REQUIRE(counter == 64);
          }
        }
      }
    }
  }
}
