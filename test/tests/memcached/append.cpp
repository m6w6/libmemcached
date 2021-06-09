#include "test/lib/common.hpp"
#include "test/lib/MemcachedCluster.hpp"

TEST_CASE("memcached_append") {
  pair<string, MemcachedCluster> tests[] = {
      {"bin_mixed", MemcachedCluster::mixed()},
      {"meta_mixed", MemcachedCluster::mixed()},
      {"ascii_mixed", MemcachedCluster::mixed()}
  };

  tests[0].second.enableBinaryProto();
  tests[1].second.enableMetaProto();

  LOOPED_SECTION(tests) {
    auto memc = &test.memc;

    if (name == "meta_mixed" && !test.isGEVersion(1, 6)) {
      continue;
    }

    SECTION("text") {
      const char *values[] = {
          "one", "two", "three", "four"
      };

      for (auto key : values) {
        string cmp{key};

        REQUIRE_SUCCESS(memcached_set(memc, S(key), S(key), 0, 0));
        for (auto value : values) {
          REQUIRE_SUCCESS(memcached_append(memc, S(key), S(value), 0, 0));
          cmp += value;
        }

        memcached_return_t rc;
        size_t len;
        uint32_t flags;
        Malloced got(memcached_get(memc, S(key), &len, &flags, &rc));

        REQUIRE(cmp == *got);
        REQUIRE_SUCCESS(rc);
      }
    }

    SECTION("bytes") {
        vector<uint32_t> store_list{ 23, 56, 499, 98, 32847, 0 };
        const size_t raw_len = sizeof(decltype(store_list)::value_type) * store_list.size();
        const char *raw_ptr = reinterpret_cast<char *>(store_list.data());

        REQUIRE_SUCCESS(memcached_set(memc, S(__func__), nullptr, 0, 0, 0));

        for (auto item : store_list) {
          auto val = reinterpret_cast<char *>(&item);
          REQUIRE_SUCCESS(memcached_append(memc, S(__func__), val, sizeof(item), 0, 0));
        }

        memcached_return_t rc;
        size_t len;
        uint32_t flags;
        Malloced got(memcached_get(memc, S(__func__), &len, &flags, &rc));

        REQUIRE(len == raw_len);
        REQUIRE(string{raw_ptr, raw_len} == string{*got, len});
        REQUIRE_SUCCESS(rc);
    }
  }
}
