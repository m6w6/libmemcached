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

struct memcached_st {
  /**
    @note these are static and should not change without a call to behavior.
  */
  struct {
    bool is_purging : 1;
    bool is_processing_input : 1;
    bool is_time_for_rebuild : 1;
    bool is_parsing : 1;
  } state;

  struct {
    // Everything below here is pretty static.
    bool auto_eject_hosts : 1;
    bool binary_protocol : 1;
    bool buffer_requests : 1;
    bool hash_with_namespace : 1;
    bool no_block : 1; // Don't block
    bool reply : 1;
    bool randomize_replica_read : 1;
    bool support_cas : 1;
    bool tcp_nodelay : 1;
    bool use_sort_hosts : 1;
    bool use_udp : 1;
    bool verify_key : 1;
    bool tcp_keepalive : 1;
    bool is_aes : 1;
    bool is_fetching_version : 1;
    bool meta_protocol : 1;
  } flags;

  memcached_server_distribution_t distribution;
  hashkit_st hashkit;
  struct {
    unsigned int version;
  } server_info;
  uint32_t number_of_hosts;
  memcached_instance_st *servers;
  memcached_instance_st *last_disconnected_server;
  int32_t snd_timeout;
  int32_t rcv_timeout;
  uint32_t server_failure_limit;
  uint32_t server_timeout_limit;
  uint32_t io_msg_watermark;
  uint32_t io_bytes_watermark;
  uint32_t io_key_prefetch;
  uint32_t tcp_keepidle;
  int32_t poll_timeout;
  int32_t connect_timeout; // How long we will wait on connect() before we will timeout
  int32_t retry_timeout;
  int32_t dead_timeout;
  int send_size;
  int recv_size;
  void *user_data;
  uint64_t query_id;
  uint32_t number_of_replicas;
  memcached_result_st result;

  struct {
    bool weighted_;
    uint32_t continuum_count;                      // Ketama
    uint32_t continuum_points_counter;             // Ketama
    time_t next_distribution_rebuild;              // Ketama
    struct memcached_continuum_item_st *continuum; // Ketama
  } ketama;

  struct memcached_virtual_bucket_t *virtual_bucket;

  struct memcached_allocator_t allocators;

  memcached_clone_fn on_clone;
  memcached_cleanup_fn on_cleanup;
  memcached_trigger_key_fn get_key_failure;
  memcached_trigger_delete_key_fn delete_trigger;
  memcached_callback_st *callbacks;
  struct memcached_sasl_st sasl;
  struct memcached_error_t *error_messages;
  struct memcached_array_st *_namespace;
  struct {
    uint32_t initial_pool_size;
    uint32_t max_pool_size;
    int32_t
        version; // This is used by pool and others to determine if the memcached_st is out of date.
    struct memcached_array_st *filename;
  } configure;
  struct {
    bool is_allocated : 1;
  } options;
};
