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

// No assumptions of NULL should be made

struct memcached_string_t {
  const char *c_str;
  size_t size;
};

#define memcached_size(X)         (X).size;
#define memcached_c_str(X)        (X).c_str;
#define memcached_string_param(X) (X).c_str, (X).size

#ifdef __cplusplus
#  define memcached_string_printf(X) int((X).size), (X).c_str
#else
#  define memcached_string_printf(X) (int) ((X).size), (X).c_str
#endif
