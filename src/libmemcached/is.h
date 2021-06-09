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

/* These are private */
#define memcached_is_allocated(__object)        ((__object)->options.is_allocated)
#define memcached_is_encrypted(__object)        ((__object)->hashkit._key)
#define memcached_is_initialized(__object)      ((__object)->options.is_initialized)
#define memcached_is_purging(__object)          ((__object)->state.is_purging)
#define memcached_is_processing_input(__object) ((__object)->state.is_processing_input)

#define memcached_is_aes(__object)                    ((__object)->flags.is_aes)
#define memcached_is_udp(__object)                    ((__object)->flags.use_udp)
#define memcached_is_verify_key(__object)             ((__object)->flags.verify_key)
#define memcached_is_binary(__object)                 ((__object)->flags.binary_protocol)
#define memcached_is_meta(__object)                   ((__object)->flags.meta_protocol)
#define memcached_is_fetching_version(__object)       ((__object)->flags.is_fetching_version)
#define memcached_is_buffering(__object)              ((__object)->flags.buffer_requests)
#define memcached_is_replying(__object)               ((__object)->flags.reply)
#define memcached_is_cas(__object)                    ((__object)->flags.reply)
#define memcached_is_randomize_replica_read(__object) ((__object)->flags.randomize_replica_read)
#define memcached_is_no_block(__object)               ((__object)->flags.no_block)
#define memcached_is_hash_with_namespace(__object)    ((__object)->flags.hash_with_namespace)
#define memcached_is_tcp_nodelay(__object)            ((__object)->flags.tcp_nodelay)
#define memcached_is_auto_eject_hosts(__object)       ((__object)->flags.auto_eject_hosts)
#define memcached_is_use_sort_hosts(__object)         ((__object)->flags.use_sort_hosts)

#define memcached_is_ready(__object) ((__object)->options.ready)

#define memcached_is_weighted_ketama(__object) ((__object)->ketama.weighted_)

#define memcached_set_ready(__object, __flag) ((__object)->options.ready = (__flag))

#define memcached_set_aes(__object, __flag)        ((__object).flags.is_aes = __flag)
#define memcached_set_udp(__object, __flag)        ((__object).flags.use_udp = __flag)
#define memcached_set_verify_key(__object, __flag) ((__object).flags.verify_key = __flag)
#define memcached_set_binary(__object, __flag)     ((__object).flags.binary_protocol = __flag)
#define memcached_set_meta(__object, __flag)       ((__object).flags.meta_protocol = __flag)
#define memcached_set_fetching_version(__object, __flag) \
  ((__object).flags.is_fetching_version = __flag)
#define memcached_set_buffering(__object, __flag) ((__object).flags.buffer_requests = __flag)
#define memcached_set_replying(__object, __flag)  ((__object).flags.reply = __flag)
#define memcached_set_cas(__object, __flag)       ((__object).flags.reply = __flag)
#define memcached_set_randomize_replica_read(__object, __flag) \
  ((__object).flags.randomize_replica_read = __flag)
#define memcached_set_no_block(__object, __flag) ((__object).flags.no_block = __flag)
#define memcached_set_hash_with_namespace(__object, __flag) \
  ((__object).flags.hash_with_namespace = __flag)
#define memcached_set_tcp_nodelay(__object, __flag) ((__object).flags.tcp_nodelay = __flag)
#define memcached_set_auto_eject_hosts(__object, __flag) \
  ((__object).flags.auto_eject_hosts = __flag)
#define memcached_set_use_sort_hosts(__object, __flag) ((__object).flags.use_sort_hosts = __flag)

#define memcached_has_root(__object) ((__object)->root)

#define memcached_has_error(__object) ((__object)->error_messages)

#define memcached_has_replicas(__object) ((__object)->root->number_of_replicas)

#define memcached_set_processing_input(__object, __value) \
  ((__object)->state.is_processing_input = (__value))
#define memcached_set_initialized(__object, __value) \
  ((__object)->options.is_initialized = (__value))
#define memcached_set_allocated(__object, __value) ((__object)->options.is_allocated = (__value))

#define memcached_set_weighted_ketama(__object, __value) ((__object)->ketama.weighted_ = (__value))

#define memcached2Memcached(__obj) (__obj)
