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

#include "mem_config.h"

#include "utilities.h"

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

long int timedif(struct timeval a, struct timeval b) {
  long us, s;

  us = (int) (a.tv_usec - b.tv_usec);
  us /= 1000;
  s = (int) (a.tv_sec - b.tv_sec);
  s *= 1000;
  return s + us;
}

void version_command(const char *command_name) {
  printf("%s v%u.%u\n", command_name, 1U, 0U);
  exit(EXIT_SUCCESS);
}

void close_stdio(void) {
  int fd;
  if ((fd = open("/dev/null", O_RDWR, 0)) < 0) {
    return;
  } else {
    if (dup2(fd, STDIN_FILENO) < 0) {
      return;
    }

    if (dup2(fd, STDOUT_FILENO) < 0) {
      return;
    }

    if (dup2(fd, STDERR_FILENO) < 0) {
      return;
    }

    if (fd > STDERR_FILENO) {
      close(fd);
    }
  }
}

static const char *lookup_help(memcached_options option) {
  switch (option) {
  case OPT_SERVERS:
    return ("List which servers you wish to connect to.");
  case OPT_VERSION:
    return ("Display the version of the application and then exit.");
  case OPT_HELP:
    return ("Display this message and then exit.");
  case OPT_VERBOSE:
    return ("Give more details on the progression of the application.");
  case OPT_QUIET:
    return ("stderr and stdin will be closed at application startup.");
  case OPT_DEBUG:
    return ("Provide output only useful for debugging.");
  case OPT_FLAG:
    return ("Provide flag information for storage operation.");
  case OPT_EXPIRE:
    return ("Set the expire option for the object.");
  case OPT_SET:
    return ("Use set command with memcached when storing.");
  case OPT_REPLACE:
    return ("Use replace command with memcached when storing.");
  case OPT_ADD:
    return ("Use add command with memcached when storing.");
  case OPT_SLAP_EXECUTE_NUMBER:
    return ("Number of times to execute the given test.");
  case OPT_SLAP_INITIAL_LOAD:
    return ("Number of key pairs to load before executing tests.");
  case OPT_SLAP_TEST:
    return ("Test to run (currently \"get\" or \"set\").");
  case OPT_SLAP_CONCURRENCY:
    return ("Number of users to simulate with load.");
  case OPT_SLAP_NON_BLOCK:
    return ("Set TCP up to use non-blocking IO.");
  case OPT_SLAP_TCP_NODELAY:
    return ("Set TCP socket up to use nodelay.");
  case OPT_FLUSH:
    return ("Flush servers before running tests.");
  case OPT_HASH:
    return ("Select hash type.");
  case OPT_BINARY:
    return ("Switch to binary protocol.");
  case OPT_ANALYZE:
    return ("Analyze the provided servers.");
  case OPT_UDP:
    return ("Use UDP protocol when communicating with server.");
  case OPT_BUFFER:
    return ("Enable request buffering.");
  case OPT_USERNAME:
    return "Username to use for SASL authentication";
  case OPT_PASSWD:
    return "Password to use for SASL authentication";
  case OPT_FILE:
    return "Path to file in which to save result";
  case OPT_STAT_ARGS:
    return "Argument for statistics";
  case OPT_SERVER_VERSION:
    return "Memcached daemon software version";
  default:
    break;
  };

  assert(0);
  return "forgot to document this function :)";
}

void help_command(const char *command_name, const char *description,
                  const struct option *long_options, memcached_programs_help_st *options) {
  unsigned int x;
  (void) options;

  printf("%s v%u.%u\n\n", command_name, 1U, 0U);
  printf("\t%s\n\n", description);
  printf("Current options. A '=' means the option takes a value.\n\n");

  for (x = 0; long_options[x].name; x++) {
    const char *help_message;

    printf("\t --%s%c\n", long_options[x].name, long_options[x].has_arg ? '=' : ' ');
    if ((help_message = lookup_help(memcached_options(long_options[x].val))))
      printf("\t\t%s\n", help_message);
  }

  printf("\n");
  exit(EXIT_SUCCESS);
}

void process_hash_option(memcached_st *memc, char *opt_hash) {
  uint64_t set;
  memcached_return_t rc;

  if (opt_hash == NULL) {
    return;
  }

  set = MEMCACHED_HASH_DEFAULT; /* Just here to solve warning */
  if (!strcasecmp(opt_hash, "CRC")) {
    set = MEMCACHED_HASH_CRC;
  } else if (!strcasecmp(opt_hash, "FNV1_64")) {
    set = MEMCACHED_HASH_FNV1_64;
  } else if (!strcasecmp(opt_hash, "FNV1A_64")) {
    set = MEMCACHED_HASH_FNV1A_64;
  } else if (!strcasecmp(opt_hash, "FNV1_32")) {
    set = MEMCACHED_HASH_FNV1_32;
  } else if (!strcasecmp(opt_hash, "FNV1A_32")) {
    set = MEMCACHED_HASH_FNV1A_32;
  } else {
    fprintf(stderr, "hash: type not recognized %s\n", opt_hash);
    exit(EXIT_FAILURE);
  }

  rc = memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_HASH, set);
  if (rc != MEMCACHED_SUCCESS) {
    fprintf(stderr, "hash: memcache error %s\n", memcached_strerror(memc, rc));
    exit(EXIT_FAILURE);
  }
}

void initialize_sockets(void) {
  /* Define the function for all platforms to avoid #ifdefs in each program */
#if defined(_WIN32)
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 0), &wsaData)) {
    fprintf(stderr, "Socket Initialization Error. Program aborted\n");
    exit(EXIT_FAILURE);
  }
#endif // #if defined(_WIN32)
}
