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

static void auto_response(memcached_instance_st *instance, const bool reply, memcached_return_t &rc,
                          uint64_t *value) {
  // If the message was successfully sent, then get the response, otherwise
  // fail.
  if (memcached_success(rc)) {
    if (reply == false) {
      *value = UINT64_MAX;
      return;
    }

    rc = memcached_response(instance, &instance->root->result);
  }

  if (memcached_fatal(rc)) {
    assert(memcached_last_error(instance->root) != MEMCACHED_SUCCESS);
    *value = UINT64_MAX;
  } else if (memcached_failed(rc)) {
    *value = UINT64_MAX;
  } else {
    assert(memcached_last_error(instance->root) != MEMCACHED_NOTFOUND);
    *value = instance->root->result.numeric_value;
  }
}

static memcached_return_t meta_incr_decr(memcached_instance_st *instance, bool is_incr, bool w_init,
                                         const char *key, size_t key_len,
                                         uint64_t offset, uint64_t initial, uint32_t expiration) {
  char new_buf[32] = " N", inl_buf[32] = " J", dlt_buf[32] = " D", exp_buf[32] = " T";
  size_t new_len = strlen(new_buf), inl_len = strlen(inl_buf), dlt_len = strlen(dlt_buf), exp_len = strlen(exp_buf);
  size_t io_num = 0;
  libmemcached_io_vector_st io_vec[10] = {};

  io_vec[io_num++] = {memcached_literal_param("ma ")};
  io_vec[io_num++] = {memcached_array_string(instance->root->_namespace),
                      memcached_array_size(instance->root->_namespace)},
  io_vec[io_num++] = {key, key_len};

  if (!is_incr) {
    io_vec[io_num++] = {memcached_literal_param(" MD")};
  }
  if (w_init) {
    new_len += snprintf(new_buf + new_len, sizeof(new_buf) - new_len, "%" PRIu32, expiration);
    io_vec[io_num++] = {new_buf, new_len};
    inl_len += snprintf(inl_buf + inl_len, sizeof(inl_buf) - inl_len, "%" PRIu64, initial);
    io_vec[io_num++] = {inl_buf, inl_len};
  }
  if (offset != 1) {
    dlt_len += snprintf(dlt_buf + dlt_len, sizeof(dlt_buf) - dlt_len, "%" PRIu64, offset);
    io_vec[io_num++] = {dlt_buf, dlt_len};
  }
  if (expiration) {
    exp_len += snprintf(exp_buf + exp_len, sizeof(exp_buf) - exp_len, "%" PRIu32, expiration);
    io_vec[io_num++] = {exp_buf, exp_len};
  }

  if (memcached_is_replying(instance->root)) {
    io_vec[io_num++] = {memcached_literal_param(" v")};
  } else {
    io_vec[io_num++] = {memcached_literal_param(" q")};
  }
  io_vec[io_num++] = {memcached_literal_param(" O+\r\n")};

  return memcached_vdo(instance, io_vec, io_num, true);
}

static memcached_return_t text_incr_decr(memcached_instance_st *instance, const bool is_incr,
                                         const char *key, size_t key_length, const uint64_t offset,
                                         const bool reply) {
  char buffer[MEMCACHED_DEFAULT_COMMAND_SIZE];

  int send_length = snprintf(buffer, sizeof(buffer), " %" PRIu64, offset);
  if (size_t(send_length) >= sizeof(buffer) or send_length < 0) {
    return memcached_set_error(*instance, MEMCACHED_MEMORY_ALLOCATION_FAILURE, MEMCACHED_AT,
                               memcached_literal_param("snprintf(MEMCACHED_DEFAULT_COMMAND_SIZE)"));
  }

  libmemcached_io_vector_st vector[] = {
      {NULL, 0},
      {memcached_literal_param("incr ")},
      {memcached_array_string(instance->root->_namespace),
       memcached_array_size(instance->root->_namespace)},
      {key, key_length},
      {buffer, size_t(send_length)},
      {" noreply", reply ? 0 : memcached_literal_param_size(" noreply")},
      {memcached_literal_param("\r\n")}};

  if (is_incr == false) {
    vector[1].buffer = "decr ";
  }

  return memcached_vdo(instance, vector, 7, true);
}

static memcached_return_t binary_incr_decr(memcached_instance_st *instance,
                                           protocol_binary_command cmd, const char *key,
                                           const size_t key_length, const uint64_t offset,
                                           const uint64_t initial, const uint32_t expiration,
                                           const bool reply) {
  if (reply == false) {
    if (cmd == PROTOCOL_BINARY_CMD_DECREMENT) {
      cmd = PROTOCOL_BINARY_CMD_DECREMENTQ;
    }

    if (cmd == PROTOCOL_BINARY_CMD_INCREMENT) {
      cmd = PROTOCOL_BINARY_CMD_INCREMENTQ;
    }
  }
  protocol_binary_request_incr request = {}; // = {.bytes= {0}};

  initialize_binary_request(instance, request.message.header);

  request.message.header.request.opcode = cmd;
  request.message.header.request.keylen =
      htons((uint16_t)(key_length + memcached_array_size(instance->root->_namespace)));
  request.message.header.request.extlen = 20;
  request.message.header.request.datatype = PROTOCOL_BINARY_RAW_BYTES;
  request.message.header.request.bodylen =
      htonl((uint32_t)(key_length + memcached_array_size(instance->root->_namespace)
                       + request.message.header.request.extlen));
  request.message.body.delta = memcached_htonll(offset);
  request.message.body.initial = memcached_htonll(initial);
  request.message.body.expiration = htonl((uint32_t) expiration);

  libmemcached_io_vector_st vector[] = {{NULL, 0},
                                        {request.bytes, sizeof(request.bytes)},
                                        {memcached_array_string(instance->root->_namespace),
                                         memcached_array_size(instance->root->_namespace)},
                                        {key, key_length}};

  return memcached_vdo(instance, vector, 4, true);
}

memcached_return_t memcached_increment(memcached_st *memc, const char *key, size_t key_length,
                                       uint32_t offset, uint64_t *value) {
  return memcached_increment_by_key(memc, key, key_length, key, key_length, offset, value);
}

static memcached_return_t increment_decrement_by_key(const protocol_binary_command command,
                                                     Memcached *memc, const char *group_key,
                                                     size_t group_key_length, const char *key,
                                                     size_t key_length, uint64_t offset,
                                                     uint64_t *value) {
  uint64_t local_value;
  if (value == NULL) {
    value = &local_value;
  }

  memcached_return_t rc;
  if (memcached_failed(rc = initialize_query(memc, true))) {
    return rc;
  }

  if (memcached_is_encrypted(memc)) {
    return memcached_set_error(
        *memc, MEMCACHED_NOT_SUPPORTED, MEMCACHED_AT,
        memcached_literal_param("Operation not allowed while encyrption is enabled"));
  }

  if (memcached_failed(rc = memcached_key_test(*memc, (const char **) &key, &key_length, 1))) {
    return memcached_last_error(memc);
  }

  uint32_t server_key =
      memcached_generate_hash_with_redistribution(memc, group_key, group_key_length);
  memcached_instance_st *instance = memcached_instance_fetch(memc, server_key);

  bool reply = memcached_is_replying(instance->root);

  if (memcached_is_binary(memc)) {
    rc = binary_incr_decr(instance, command, key, key_length, uint64_t(offset), 0,
                          MEMCACHED_EXPIRATION_NOT_ADD, reply);
  } else if (memcached_is_meta(memc)) {
    rc = meta_incr_decr(instance, command == PROTOCOL_BINARY_CMD_INCREMENT, false, key, key_length, offset, 0, 0);
  } else {
    rc = text_incr_decr(instance, command == PROTOCOL_BINARY_CMD_INCREMENT ? true : false, key,
                        key_length, offset, reply);
  }

  auto_response(instance, reply, rc, value);

  return rc;
}

static memcached_return_t
increment_decrement_with_initial_by_key(const protocol_binary_command command, Memcached *memc,
                                        const char *group_key, size_t group_key_length,
                                        const char *key, size_t key_length, uint64_t offset,
                                        uint64_t initial, time_t expiration, uint64_t *value) {
  uint64_t local_value;
  if (value == NULL) {
    value = &local_value;
  }

  memcached_return_t rc;
  if (memcached_failed(rc = initialize_query(memc, true))) {
    return rc;
  }

  if (memcached_is_encrypted(memc)) {
    return memcached_set_error(
        *memc, MEMCACHED_NOT_SUPPORTED, MEMCACHED_AT,
        memcached_literal_param("Operation not allowed while encryption is enabled"));
  }

  if (memcached_failed(rc = memcached_key_test(*memc, (const char **) &key, &key_length, 1))) {
    return memcached_last_error(memc);
  }

  uint32_t server_key =
      memcached_generate_hash_with_redistribution(memc, group_key, group_key_length);
  memcached_instance_st *instance = memcached_instance_fetch(memc, server_key);

  bool reply = memcached_is_replying(instance->root);

  if (memcached_is_binary(memc)) {
    rc = binary_incr_decr(instance, command, key, key_length, offset, initial, uint32_t(expiration),
                          reply);
  } else if (memcached_is_meta(memc)) {
    rc = meta_incr_decr(instance, command == PROTOCOL_BINARY_CMD_INCREMENT, true,
                        key, key_length, offset, initial, uint32_t(expiration));
  } else {
    rc = memcached_set_error(
        *memc, MEMCACHED_INVALID_ARGUMENTS, MEMCACHED_AT,
        memcached_literal_param(
            "memcached_increment_with_initial_by_key() is not supported via the ASCII protocol"));
  }

  auto_response(instance, reply, rc, value);

  return rc;
}

memcached_return_t memcached_decrement(memcached_st *memc, const char *key, size_t key_length,
                                       uint32_t offset, uint64_t *value) {
  return memcached_decrement_by_key(memc, key, key_length, key, key_length, offset, value);
}

memcached_return_t memcached_increment_by_key(memcached_st *shell, const char *group_key,
                                              size_t group_key_length, const char *key,
                                              size_t key_length, uint64_t offset, uint64_t *value) {
  Memcached *memc = memcached2Memcached(shell);
  LIBMEMCACHED_MEMCACHED_INCREMENT_START();
  memcached_return_t rc =
      increment_decrement_by_key(PROTOCOL_BINARY_CMD_INCREMENT, memc, group_key, group_key_length,
                                 key, key_length, offset, value);

  LIBMEMCACHED_MEMCACHED_INCREMENT_END();

  return rc;
}

memcached_return_t memcached_decrement_by_key(memcached_st *shell, const char *group_key,
                                              size_t group_key_length, const char *key,
                                              size_t key_length, uint64_t offset, uint64_t *value) {
  Memcached *memc = memcached2Memcached(shell);
  LIBMEMCACHED_MEMCACHED_DECREMENT_START();
  memcached_return_t rc =
      increment_decrement_by_key(PROTOCOL_BINARY_CMD_DECREMENT, memc, group_key, group_key_length,
                                 key, key_length, offset, value);
  LIBMEMCACHED_MEMCACHED_DECREMENT_END();

  return rc;
}

memcached_return_t memcached_increment_with_initial(memcached_st *memc, const char *key,
                                                    size_t key_length, uint64_t offset,
                                                    uint64_t initial, time_t expiration,
                                                    uint64_t *value) {
  return memcached_increment_with_initial_by_key(memc, key, key_length, key, key_length, offset,
                                                 initial, expiration, value);
}

memcached_return_t memcached_increment_with_initial_by_key(
    memcached_st *shell, const char *group_key, size_t group_key_length, const char *key,
    size_t key_length, uint64_t offset, uint64_t initial, time_t expiration, uint64_t *value) {
  LIBMEMCACHED_MEMCACHED_INCREMENT_WITH_INITIAL_START();
  Memcached *memc = memcached2Memcached(shell);
  memcached_return_t rc = increment_decrement_with_initial_by_key(
      PROTOCOL_BINARY_CMD_INCREMENT, memc, group_key, group_key_length, key, key_length, offset,
      initial, expiration, value);
  LIBMEMCACHED_MEMCACHED_INCREMENT_WITH_INITIAL_END();

  return rc;
}

memcached_return_t memcached_decrement_with_initial(memcached_st *memc, const char *key,
                                                    size_t key_length, uint64_t offset,
                                                    uint64_t initial, time_t expiration,
                                                    uint64_t *value) {
  return memcached_decrement_with_initial_by_key(memc, key, key_length, key, key_length, offset,
                                                 initial, expiration, value);
}

memcached_return_t memcached_decrement_with_initial_by_key(
    memcached_st *shell, const char *group_key, size_t group_key_length, const char *key,
    size_t key_length, uint64_t offset, uint64_t initial, time_t expiration, uint64_t *value) {
  LIBMEMCACHED_MEMCACHED_INCREMENT_WITH_INITIAL_START();
  Memcached *memc = memcached2Memcached(shell);
  memcached_return_t rc = increment_decrement_with_initial_by_key(
      PROTOCOL_BINARY_CMD_DECREMENT, memc, group_key, group_key_length, key, key_length, offset,
      initial, expiration, value);

  LIBMEMCACHED_MEMCACHED_INCREMENT_WITH_INITIAL_END();

  return rc;
}
