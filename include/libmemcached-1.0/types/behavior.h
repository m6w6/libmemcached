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

#pragma once

enum memcached_behavior_t {
  MEMCACHED_BEHAVIOR_NO_BLOCK,
  MEMCACHED_BEHAVIOR_TCP_NODELAY,
  MEMCACHED_BEHAVIOR_HASH,
  MEMCACHED_BEHAVIOR_KETAMA,
  MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE,
  MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE,
  MEMCACHED_BEHAVIOR_CACHE_LOOKUPS,
  MEMCACHED_BEHAVIOR_SUPPORT_CAS,
  MEMCACHED_BEHAVIOR_POLL_TIMEOUT,
  MEMCACHED_BEHAVIOR_DISTRIBUTION,
  MEMCACHED_BEHAVIOR_BUFFER_REQUESTS,
  MEMCACHED_BEHAVIOR_USER_DATA,
  MEMCACHED_BEHAVIOR_SORT_HOSTS,
  MEMCACHED_BEHAVIOR_VERIFY_KEY,
  MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT,
  MEMCACHED_BEHAVIOR_RETRY_TIMEOUT,
  MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED,
  MEMCACHED_BEHAVIOR_KETAMA_HASH,
  MEMCACHED_BEHAVIOR_BINARY_PROTOCOL,
  MEMCACHED_BEHAVIOR_SND_TIMEOUT,
  MEMCACHED_BEHAVIOR_RCV_TIMEOUT,
  MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT,
  MEMCACHED_BEHAVIOR_IO_MSG_WATERMARK,
  MEMCACHED_BEHAVIOR_IO_BYTES_WATERMARK,
  MEMCACHED_BEHAVIOR_IO_KEY_PREFETCH,
  MEMCACHED_BEHAVIOR_HASH_WITH_PREFIX_KEY,
  MEMCACHED_BEHAVIOR_NOREPLY,
  MEMCACHED_BEHAVIOR_USE_UDP,
  MEMCACHED_BEHAVIOR_AUTO_EJECT_HOSTS,
  MEMCACHED_BEHAVIOR_NUMBER_OF_REPLICAS,
  MEMCACHED_BEHAVIOR_RANDOMIZE_REPLICA_READ,
  MEMCACHED_BEHAVIOR_CORK,
  MEMCACHED_BEHAVIOR_TCP_KEEPALIVE,
  MEMCACHED_BEHAVIOR_TCP_KEEPIDLE,
  MEMCACHED_BEHAVIOR_LOAD_FROM_FILE,
  MEMCACHED_BEHAVIOR_REMOVE_FAILED_SERVERS,
  MEMCACHED_BEHAVIOR_DEAD_TIMEOUT,
  MEMCACHED_BEHAVIOR_SERVER_TIMEOUT_LIMIT,
  MEMCACHED_BEHAVIOR_META_PROTOCOL,
  MEMCACHED_BEHAVIOR_MAX
};

#ifndef __cplusplus
typedef enum memcached_behavior_t memcached_behavior_t;
#endif
