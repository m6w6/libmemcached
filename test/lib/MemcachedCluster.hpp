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

#include "common.hpp"
#include "Cluster.hpp"
#include "ReturnMatcher.hpp"

class MemcachedCluster {
public:
  using behavior_t = pair<memcached_behavior_t, uint64_t>;
  using behaviors_t = vector<behavior_t>;

  Cluster cluster;
  memcached_st memc{empty_memc};
  ReturnMatcher returns{&memc};

  MemcachedCluster();
  explicit MemcachedCluster(Cluster &&cluster, behaviors_t to_set = {});
  ~MemcachedCluster();

  MemcachedCluster(const MemcachedCluster &) = delete;
  MemcachedCluster &operator=(const MemcachedCluster &) = delete;

  MemcachedCluster(MemcachedCluster &&mc) noexcept;
  MemcachedCluster &operator=(MemcachedCluster &&mc) noexcept;

  void enableBinaryProto(bool enable = true);
  void enableMetaProto(bool enable = true);
  void enableBuffering(bool enable = true);
  void enableReplication();
  void flush();

  static MemcachedCluster mixed();
  static MemcachedCluster network();
  static MemcachedCluster socket();
  static MemcachedCluster udp();

#if LIBMEMCACHED_WITH_SASL_SUPPORT
  static MemcachedCluster sasl();
#endif

  void killOneServer() const;
  bool isGEVersion(uint8_t major, uint8_t minor = 0, uint8_t micro = 0);

private:
  behaviors_t to_set;
  static const memcached_st empty_memc;

  void init();
};
