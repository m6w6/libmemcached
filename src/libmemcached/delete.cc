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

#include "libmemcached/common.h"

memcached_return_t memcached_delete(memcached_st *shell, const char *key, size_t key_length,
                                    time_t expiration) {
  return memcached_delete_by_key(shell, key, key_length, key, key_length, expiration);
}

static inline memcached_return_t meta_delete(memcached_instance_st *instance,
                                             const char *key, size_t key_length,
                                             time_t expiration) {

  char ex_buf[32] = " I T";
  size_t io_num = 0, ex_len = strlen(ex_buf);
  libmemcached_io_vector_st io_vec[6] = {};
  io_vec[io_num++] = {memcached_literal_param("md ")};
  io_vec[io_num++] = {memcached_array_string(instance->root->_namespace),
                      memcached_array_size(instance->root->_namespace)};
  io_vec[io_num++] = {key, key_length};
  if (!memcached_is_replying(instance->root)) {
    io_vec[io_num++] = {" q", 2};
  }
  if (expiration) {
    ex_len += snprintf(ex_buf + ex_len, sizeof(ex_buf) - ex_len, "%llu", (unsigned long long) expiration);
    io_vec[io_num++] = {ex_buf, ex_len};
  }
  io_vec[io_num++] = {memcached_literal_param("\r\n")};

  /* Send command header, only flush if we are NOT buffering */
  return memcached_vdo(instance, io_vec, io_num, !memcached_is_buffering(instance->root));
}

static inline memcached_return_t ascii_delete(memcached_instance_st *instance, uint32_t,
                                              const char *key, const size_t key_length,
                                              const bool reply, const bool is_buffering) {
  libmemcached_io_vector_st vector[] = {
      {NULL, 0},
      {memcached_literal_param("delete ")},
      {memcached_array_string(instance->root->_namespace),
       memcached_array_size(instance->root->_namespace)},
      {key, key_length},
      {" noreply", reply ? 0 : memcached_literal_param_size(" noreply")},
      {memcached_literal_param("\r\n")}};

  /* Send command header, only flush if we are NOT buffering */
  return memcached_vdo(instance, vector, 6, is_buffering ? false : true);
}

static inline memcached_return_t binary_delete(memcached_instance_st *instance, uint32_t server_key,
                                               const char *key, const size_t key_length,
                                               const bool reply, const bool is_buffering) {
  protocol_binary_request_delete request = {};

  bool should_flush = is_buffering ? false : true;

  initialize_binary_request(instance, request.message.header);

  if (reply) {
    request.message.header.request.opcode = PROTOCOL_BINARY_CMD_DELETE;
  } else {
    request.message.header.request.opcode = PROTOCOL_BINARY_CMD_DELETEQ;
  }
  request.message.header.request.keylen =
      htons(uint16_t(key_length + memcached_array_size(instance->root->_namespace)));
  request.message.header.request.datatype = PROTOCOL_BINARY_RAW_BYTES;
  request.message.header.request.bodylen =
      htonl(uint32_t(key_length + memcached_array_size(instance->root->_namespace)));

  libmemcached_io_vector_st vector[] = {{NULL, 0},
                                        {request.bytes, sizeof(request.bytes)},
                                        {memcached_array_string(instance->root->_namespace),
                                         memcached_array_size(instance->root->_namespace)},
                                        {key, key_length}};

  memcached_return_t rc = memcached_vdo(instance, vector, 4, should_flush);

  if (memcached_has_replicas(instance)) {
    request.message.header.request.opcode = PROTOCOL_BINARY_CMD_DELETEQ;

    for (uint32_t x = 0; x < memcached_has_replicas(instance); ++x) {
      ++server_key;

      if (server_key == memcached_server_count(instance->root)) {
        server_key = 0;
      }

      memcached_instance_st *replica = memcached_instance_fetch(instance->root, server_key);

      if (memcached_success(memcached_vdo(replica, vector, 4, should_flush))) {
        memcached_server_response_decrement(replica);
      }
    }
  }

  return rc;
}

memcached_return_t memcached_delete_by_key(memcached_st *memc, const char *group_key,
                                           size_t group_key_length, const char *key,
                                           size_t key_length, time_t expiration) {
  LIBMEMCACHED_MEMCACHED_DELETE_START();

  memcached_return_t rc;
  if (memcached_fatal(rc = initialize_query(memc, true))) {
    return rc;
  }

  if (memcached_fatal(rc = memcached_key_test(*memc, (const char **) &key, &key_length, 1))) {
    return memcached_last_error(memc);
  }

  if (expiration && !memcached_is_meta(memc)) {
    return memcached_set_error(
        *memc, MEMCACHED_INVALID_ARGUMENTS, MEMCACHED_AT,
        memcached_literal_param(
            "Memcached server version does not allow expiration of deleted items"));
  }

  auto server_key = memcached_generate_hash_with_redistribution(memc, group_key, group_key_length);
  auto *instance = memcached_instance_fetch(memc, server_key);

  bool is_buffering = memcached_is_buffering(instance->root);
  bool is_replying = memcached_is_replying(instance->root);

  // If a delete trigger exists, we need a response, so no buffering/noreply
  if (memc->delete_trigger) {
    if (is_buffering) {
      return memcached_set_error(
          *memc, MEMCACHED_INVALID_ARGUMENTS, MEMCACHED_AT,
          memcached_literal_param("Delete triggers cannot be used if buffering is enabled"));
    }

    if (is_replying == false) {
      return memcached_set_error(
          *memc, MEMCACHED_INVALID_ARGUMENTS, MEMCACHED_AT,
          memcached_literal_param(
              "Delete triggers cannot be used if MEMCACHED_BEHAVIOR_NOREPLY is set"));
    }
  }

  if (memcached_is_binary(memc)) {
    rc = binary_delete(instance, server_key, key, key_length, is_replying, is_buffering);
  } else if (memcached_is_meta(memc)) {
    rc = meta_delete(instance, key, key_length, expiration);
  } else {
    rc = ascii_delete(instance, server_key, key, key_length, is_replying, is_buffering);
  }

  if (rc == MEMCACHED_SUCCESS) {
    if (is_buffering == true) {
      rc = MEMCACHED_BUFFERED;
    } else if (is_replying == false) {
      rc = MEMCACHED_SUCCESS;
    } else {
      char buffer[MEMCACHED_DEFAULT_COMMAND_SIZE];
      rc = memcached_response(instance, buffer, MEMCACHED_DEFAULT_COMMAND_SIZE, NULL);
      if (rc == MEMCACHED_DELETED) {
        rc = MEMCACHED_SUCCESS;
        if (memc->delete_trigger) {
          memc->delete_trigger(memc, key, key_length);
        }
      }
    }
  }

  LIBMEMCACHED_MEMCACHED_DELETE_END();
  return rc;
}
