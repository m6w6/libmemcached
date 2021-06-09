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

enum memcached_storage_action_t { SET_OP, REPLACE_OP, ADD_OP, PREPEND_OP, APPEND_OP, CAS_OP };

/* Inline this */
static inline const char *storage_op_string(memcached_storage_action_t verb) {
  switch (verb) {
  case REPLACE_OP:
    return "replace ";

  case ADD_OP:
    return "add ";

  case PREPEND_OP:
    return "prepend ";

  case APPEND_OP:
    return "append ";

  case CAS_OP:
    return "cas ";

  case SET_OP:
    break;
  }

  return "set ";
}

static inline bool can_by_encrypted(const memcached_storage_action_t verb) {
  switch (verb) {
  case SET_OP:
  case ADD_OP:
  case CAS_OP:
  case REPLACE_OP:
    return true;

  case APPEND_OP:
  case PREPEND_OP:
    break;
  }

  return false;
}

static inline uint8_t get_com_code(const memcached_storage_action_t verb, const bool reply) {
  if (reply == false) {
    switch (verb) {
    case SET_OP:
      return PROTOCOL_BINARY_CMD_SETQ;

    case ADD_OP:
      return PROTOCOL_BINARY_CMD_ADDQ;

    case CAS_OP: /* FALLTHROUGH */
    case REPLACE_OP:
      return PROTOCOL_BINARY_CMD_REPLACEQ;

    case APPEND_OP:
      return PROTOCOL_BINARY_CMD_APPENDQ;

    case PREPEND_OP:
      return PROTOCOL_BINARY_CMD_PREPENDQ;
    }
  }

  switch (verb) {
  case SET_OP:
    break;

  case ADD_OP:
    return PROTOCOL_BINARY_CMD_ADD;

  case CAS_OP: /* FALLTHROUGH */
  case REPLACE_OP:
    return PROTOCOL_BINARY_CMD_REPLACE;

  case APPEND_OP:
    return PROTOCOL_BINARY_CMD_APPEND;

  case PREPEND_OP:
    return PROTOCOL_BINARY_CMD_PREPEND;
  }

  return PROTOCOL_BINARY_CMD_SET;
}

static memcached_return_t memcached_send_binary(Memcached *ptr, memcached_instance_st *server,
                                                uint32_t server_key, const char *key,
                                                const size_t key_length, const char *value,
                                                const size_t value_length, const time_t expiration,
                                                const uint32_t flags, const uint64_t cas,
                                                const bool flush, const bool reply,
                                                memcached_storage_action_t verb) {
  protocol_binary_request_set request = {};
  size_t send_length = sizeof(request.bytes);

  initialize_binary_request(server, request.message.header);

  request.message.header.request.opcode = get_com_code(verb, reply);
  request.message.header.request.keylen =
      htons((uint16_t)(key_length + memcached_array_size(ptr->_namespace)));
  request.message.header.request.datatype = PROTOCOL_BINARY_RAW_BYTES;
  if (verb == APPEND_OP or verb == PREPEND_OP) {
    send_length -= 8; /* append & prepend does not contain extras! */
  } else {
    request.message.header.request.extlen = 8;
    request.message.body.flags = htonl(flags);
    request.message.body.expiration = htonl((uint32_t) expiration);
  }

  request.message.header.request.bodylen =
      htonl((uint32_t)(key_length + memcached_array_size(ptr->_namespace) + value_length
                       + request.message.header.request.extlen));

  if (cas) {
    request.message.header.request.cas = memcached_htonll(cas);
  }

  libmemcached_io_vector_st vector[] = {
      {NULL, 0},
      {request.bytes, send_length},
      {memcached_array_string(ptr->_namespace), memcached_array_size(ptr->_namespace)},
      {key, key_length},
      {value, value_length}};

  /* write the header */
  memcached_return_t rc;
  if ((rc = memcached_vdo(server, vector, 5, flush)) != MEMCACHED_SUCCESS) {
    assert(memcached_last_error(server->root) != MEMCACHED_SUCCESS);
    return memcached_last_error(server->root);
  }

  if (verb == SET_OP and ptr->number_of_replicas > 0) {
    request.message.header.request.opcode = PROTOCOL_BINARY_CMD_SETQ;
    WATCHPOINT_STRING("replicating");

    for (uint32_t x = 0; x < ptr->number_of_replicas; x++) {
      ++server_key;
      if (server_key == memcached_server_count(ptr)) {
        server_key = 0;
      }

      memcached_instance_st *instance = memcached_instance_fetch(ptr, server_key);

      if (memcached_success(memcached_vdo(instance, vector, 5, false))) {
        memcached_server_response_decrement(instance);
      }
    }
  }

  if (flush == false) {
    return MEMCACHED_BUFFERED;
  }

  // No reply always assumes success
  if (reply == false) {
    return MEMCACHED_SUCCESS;
  }

  return memcached_response(server, NULL, 0, NULL);
}

static memcached_return_t
memcached_send_meta(memcached_st *ptr, memcached_instance_st *instance,
                    const char *key, size_t key_len,
                    const char *val, size_t val_len,
                    time_t expiration, uint32_t flags, uint64_t cas,
                    bool flush, memcached_storage_action_t verb) {
  static const char modes[] = "SREPAS";
  char fl_buf[32] = " F", cs_buf[32] = " C", ex_buf[32] = " T", sz_buf[32] = " S";
  size_t io_num = 0, fl_len = strlen(fl_buf), cs_len = strlen(cs_buf), ex_len = strlen(ex_buf), sz_len = strlen(sz_buf);
  libmemcached_io_vector_st io_vec[16] = {};

  io_vec[io_num++] = {memcached_literal_param("ms ")};
  io_vec[io_num++] = {memcached_array_string(ptr->_namespace),
                      memcached_array_size(ptr->_namespace)};
  io_vec[io_num++] = {key, key_len};

  if (verb != SET_OP) {
    io_vec[io_num++] = {memcached_literal_param(" M")};
    io_vec[io_num++] = {&modes[verb], 1};
  }

  if (!memcached_is_replying(ptr)) {
    io_vec[io_num++] = { memcached_literal_param(" q")};
  }

  fl_len += snprintf(fl_buf + fl_len, sizeof(fl_buf) - fl_len, "%" PRIu32, flags);
  io_vec[io_num++] = {fl_buf, fl_len};
  if (expiration) {
    ex_len += snprintf(ex_buf + ex_len, sizeof(ex_buf) - ex_len, "%" PRIi64, (int64_t) expiration);
    io_vec[io_num++] = {ex_buf, ex_len};
  }
  if (cas) {
    cs_len += snprintf(cs_buf + cs_len, sizeof(cs_buf) - cs_len, "%" PRIu64, cas);
    io_vec[io_num++] = {cs_buf, cs_len};
  }

  /* we have to send a data block even if it's empty, else memcached errors out with ITEM TOO BIG */
  sz_len += snprintf(sz_buf + sz_len, sizeof(sz_buf) - sz_len, "%" PRIu64, (uint64_t) val_len);
  io_vec[io_num++] = {sz_buf, sz_len};
  io_vec[io_num++] = {memcached_literal_param("\r\n")};
  io_vec[io_num++] = {val, val_len};
  io_vec[io_num++] = {memcached_literal_param("\r\n")};

  /* Send command header */
  memcached_return_t rc = memcached_vdo(instance, io_vec, io_num, flush);

  // If we should not reply, return with MEMCACHED_SUCCESS, unless error
  if (!memcached_is_replying(ptr)) {
    return memcached_success(rc) ? MEMCACHED_SUCCESS : rc;
  }

  if (!flush) {
    return memcached_success(rc) ? MEMCACHED_BUFFERED : rc;
  }

  if (rc == MEMCACHED_SUCCESS) {
    char buffer[MEMCACHED_DEFAULT_COMMAND_SIZE];
    rc = memcached_response(instance, buffer, sizeof(buffer), NULL);

    if (rc == MEMCACHED_SUCCESS) {
      return MEMCACHED_SUCCESS;
    }
  }

  assert(memcached_failed(rc));

  return rc;
}

static memcached_return_t
memcached_send_ascii(Memcached *ptr, memcached_instance_st *instance, const char *key,
                     const size_t key_length, const char *value, const size_t value_length,
                     const time_t expiration, const uint32_t flags, const uint64_t cas,
                     const bool flush, const bool reply, const memcached_storage_action_t verb) {
  char flags_buffer[MEMCACHED_MAXIMUM_INTEGER_DISPLAY_LENGTH + 1];
  int flags_buffer_length = snprintf(flags_buffer, sizeof(flags_buffer), " %u", flags);
  if (size_t(flags_buffer_length) >= sizeof(flags_buffer) or flags_buffer_length < 0) {
    return memcached_set_error(
        *instance, MEMCACHED_MEMORY_ALLOCATION_FAILURE, MEMCACHED_AT,
        memcached_literal_param("snprintf(MEMCACHED_MAXIMUM_INTEGER_DISPLAY_LENGTH)"));
  }

  char expiration_buffer[MEMCACHED_MAXIMUM_INTEGER_DISPLAY_LENGTH + 1];
  int expiration_buffer_length = snprintf(expiration_buffer, sizeof(expiration_buffer), " %llu",
                                          (unsigned long long) expiration);
  if (size_t(expiration_buffer_length) >= sizeof(expiration_buffer) or expiration_buffer_length < 0)
  {
    return memcached_set_error(
        *instance, MEMCACHED_MEMORY_ALLOCATION_FAILURE, MEMCACHED_AT,
        memcached_literal_param("snprintf(MEMCACHED_MAXIMUM_INTEGER_DISPLAY_LENGTH)"));
  }

  char value_buffer[MEMCACHED_MAXIMUM_INTEGER_DISPLAY_LENGTH + 1];
  int value_buffer_length =
      snprintf(value_buffer, sizeof(value_buffer), " %llu", (unsigned long long) value_length);
  if (size_t(value_buffer_length) >= sizeof(value_buffer) or value_buffer_length < 0) {
    return memcached_set_error(
        *instance, MEMCACHED_MEMORY_ALLOCATION_FAILURE, MEMCACHED_AT,
        memcached_literal_param("snprintf(MEMCACHED_MAXIMUM_INTEGER_DISPLAY_LENGTH)"));
  }

  char cas_buffer[MEMCACHED_MAXIMUM_INTEGER_DISPLAY_LENGTH + 1];
  int cas_buffer_length = 0;
  if (cas) {
    cas_buffer_length = snprintf(cas_buffer, sizeof(cas_buffer), " %llu", (unsigned long long) cas);
    if (size_t(cas_buffer_length) >= sizeof(cas_buffer) or cas_buffer_length < 0) {
      return memcached_set_error(
          *instance, MEMCACHED_MEMORY_ALLOCATION_FAILURE, MEMCACHED_AT,
          memcached_literal_param("snprintf(MEMCACHED_MAXIMUM_INTEGER_DISPLAY_LENGTH)"));
    }
  }

  libmemcached_io_vector_st vector[] = {
      {NULL, 0},
      {storage_op_string(verb), strlen(storage_op_string(verb))},
      {memcached_array_string(ptr->_namespace), memcached_array_size(ptr->_namespace)},
      {key, key_length},
      {flags_buffer, size_t(flags_buffer_length)},
      {expiration_buffer, size_t(expiration_buffer_length)},
      {value_buffer, size_t(value_buffer_length)},
      {cas_buffer, size_t(cas_buffer_length)},
      {" noreply", reply ? 0 : memcached_literal_param_size(" noreply")},
      {memcached_literal_param("\r\n")},
      {value, value_length},
      {memcached_literal_param("\r\n")}};

  /* Send command header */
  memcached_return_t rc = memcached_vdo(instance, vector, 12, flush);

  // If we should not reply, return with MEMCACHED_SUCCESS, unless error
  if (reply == false) {
    return memcached_success(rc) ? MEMCACHED_SUCCESS : rc;
  }

  if (flush == false) {
    return memcached_success(rc) ? MEMCACHED_BUFFERED : rc;
  }

  if (rc == MEMCACHED_SUCCESS) {
    char buffer[MEMCACHED_DEFAULT_COMMAND_SIZE];
    rc = memcached_response(instance, buffer, sizeof(buffer), NULL);

    if (rc == MEMCACHED_STORED) {
      return MEMCACHED_SUCCESS;
    }
  }

  assert(memcached_failed(rc));
#if 0
  if (memcached_has_error(ptr) == false)
  {
    return memcached_set_error(*ptr, rc, MEMCACHED_AT);
  }
#endif

  return rc;
}

static inline memcached_return_t
memcached_send(memcached_st *shell, const char *group_key, size_t group_key_length, const char *key,
               size_t key_length, const char *value, size_t value_length, const time_t expiration,
               const uint32_t flags, const uint64_t cas, memcached_storage_action_t verb) {
  Memcached *ptr = memcached2Memcached(shell);
  memcached_return_t rc;
  if (memcached_failed(rc = initialize_query(ptr, true))) {
    return rc;
  }

  if (memcached_failed(memcached_key_test(*ptr, (const char **) &key, &key_length, 1))) {
    return memcached_last_error(ptr);
  }

  uint32_t server_key =
      memcached_generate_hash_with_redistribution(ptr, group_key, group_key_length);
  memcached_instance_st *instance = memcached_instance_fetch(ptr, server_key);

  WATCHPOINT_SET(instance->io_wait_count.read = 0);
  WATCHPOINT_SET(instance->io_wait_count.write = 0);

  bool flush = true;
  if (memcached_is_buffering(instance->root) and verb == SET_OP) {
    flush = false;
  }

  bool reply = memcached_is_replying(ptr);

  hashkit_string_st *destination = NULL;

  if (memcached_is_encrypted(ptr)) {
    if (can_by_encrypted(verb) == false) {
      return memcached_set_error(
          *ptr, MEMCACHED_NOT_SUPPORTED, MEMCACHED_AT,
          memcached_literal_param("Operation not allowed while encyrption is enabled"));
    }

    if ((destination = hashkit_encrypt(&ptr->hashkit, value, value_length)) == NULL) {
      return rc;
    }
    value = hashkit_string_c_str(destination);
    value_length = hashkit_string_length(destination);
  }

  if (memcached_is_binary(ptr)) {
    rc = memcached_send_binary(ptr, instance, server_key, key, key_length, value, value_length,
                               expiration, flags, cas, flush, reply, verb);
  } else if (memcached_is_meta(ptr)) {
    rc = memcached_send_meta(ptr, instance, key, key_length, value, value_length, expiration,
                             flags, cas, flush, verb);
  } else {
    rc = memcached_send_ascii(ptr, instance, key, key_length, value, value_length, expiration,
                              flags, cas, flush, reply, verb);
  }

  hashkit_string_free(destination);

  return rc;
}

memcached_return_t memcached_set(memcached_st *ptr, const char *key, size_t key_length,
                                 const char *value, size_t value_length, time_t expiration,
                                 uint32_t flags) {
  memcached_return_t rc;
  LIBMEMCACHED_MEMCACHED_SET_START();
  rc = memcached_send(ptr, key, key_length, key, key_length, value, value_length, expiration, flags,
                      0, SET_OP);
  LIBMEMCACHED_MEMCACHED_SET_END();
  return rc;
}

memcached_return_t memcached_add(memcached_st *ptr, const char *key, size_t key_length,
                                 const char *value, size_t value_length, time_t expiration,
                                 uint32_t flags) {
  memcached_return_t rc;
  LIBMEMCACHED_MEMCACHED_ADD_START();
  rc = memcached_send(ptr, key, key_length, key, key_length, value, value_length, expiration, flags,
                      0, ADD_OP);

  LIBMEMCACHED_MEMCACHED_ADD_END();
  return rc;
}

memcached_return_t memcached_replace(memcached_st *ptr, const char *key, size_t key_length,
                                     const char *value, size_t value_length, time_t expiration,
                                     uint32_t flags) {
  memcached_return_t rc;
  LIBMEMCACHED_MEMCACHED_REPLACE_START();
  rc = memcached_send(ptr, key, key_length, key, key_length, value, value_length, expiration, flags,
                      0, REPLACE_OP);
  LIBMEMCACHED_MEMCACHED_REPLACE_END();
  return rc;
}

memcached_return_t memcached_prepend(memcached_st *ptr, const char *key, size_t key_length,
                                     const char *value, size_t value_length, time_t expiration,
                                     uint32_t flags) {
  memcached_return_t rc;
  rc = memcached_send(ptr, key, key_length, key, key_length, value, value_length, expiration, flags,
                      0, PREPEND_OP);
  return rc;
}

memcached_return_t memcached_append(memcached_st *ptr, const char *key, size_t key_length,
                                    const char *value, size_t value_length, time_t expiration,
                                    uint32_t flags) {
  memcached_return_t rc;
  rc = memcached_send(ptr, key, key_length, key, key_length, value, value_length, expiration, flags,
                      0, APPEND_OP);
  return rc;
}

memcached_return_t memcached_cas(memcached_st *ptr, const char *key, size_t key_length,
                                 const char *value, size_t value_length, time_t expiration,
                                 uint32_t flags, uint64_t cas) {
  memcached_return_t rc;
  rc = memcached_send(ptr, key, key_length, key, key_length, value, value_length, expiration, flags,
                      cas, CAS_OP);
  return rc;
}

memcached_return_t memcached_set_by_key(memcached_st *ptr, const char *group_key,
                                        size_t group_key_length, const char *key, size_t key_length,
                                        const char *value, size_t value_length, time_t expiration,
                                        uint32_t flags) {
  memcached_return_t rc;
  LIBMEMCACHED_MEMCACHED_SET_START();
  rc = memcached_send(ptr, group_key, group_key_length, key, key_length, value, value_length,
                      expiration, flags, 0, SET_OP);
  LIBMEMCACHED_MEMCACHED_SET_END();
  return rc;
}

memcached_return_t memcached_add_by_key(memcached_st *ptr, const char *group_key,
                                        size_t group_key_length, const char *key, size_t key_length,
                                        const char *value, size_t value_length, time_t expiration,
                                        uint32_t flags) {
  memcached_return_t rc;
  LIBMEMCACHED_MEMCACHED_ADD_START();
  rc = memcached_send(ptr, group_key, group_key_length, key, key_length, value, value_length,
                      expiration, flags, 0, ADD_OP);
  LIBMEMCACHED_MEMCACHED_ADD_END();
  return rc;
}

memcached_return_t memcached_replace_by_key(memcached_st *ptr, const char *group_key,
                                            size_t group_key_length, const char *key,
                                            size_t key_length, const char *value,
                                            size_t value_length, time_t expiration,
                                            uint32_t flags) {
  memcached_return_t rc;
  LIBMEMCACHED_MEMCACHED_REPLACE_START();
  rc = memcached_send(ptr, group_key, group_key_length, key, key_length, value, value_length,
                      expiration, flags, 0, REPLACE_OP);
  LIBMEMCACHED_MEMCACHED_REPLACE_END();
  return rc;
}

memcached_return_t memcached_prepend_by_key(memcached_st *ptr, const char *group_key,
                                            size_t group_key_length, const char *key,
                                            size_t key_length, const char *value,
                                            size_t value_length, time_t expiration,
                                            uint32_t flags) {
  return memcached_send(ptr, group_key, group_key_length, key, key_length, value, value_length,
                        expiration, flags, 0, PREPEND_OP);
}

memcached_return_t memcached_append_by_key(memcached_st *ptr, const char *group_key,
                                           size_t group_key_length, const char *key,
                                           size_t key_length, const char *value,
                                           size_t value_length, time_t expiration, uint32_t flags) {
  return memcached_send(ptr, group_key, group_key_length, key, key_length, value, value_length,
                        expiration, flags, 0, APPEND_OP);
}

memcached_return_t memcached_cas_by_key(memcached_st *ptr, const char *group_key,
                                        size_t group_key_length, const char *key, size_t key_length,
                                        const char *value, size_t value_length, time_t expiration,
                                        uint32_t flags, uint64_t cas) {
  return memcached_send(ptr, group_key, group_key_length, key, key_length, value, value_length,
                        expiration, flags, cas, CAS_OP);
}
