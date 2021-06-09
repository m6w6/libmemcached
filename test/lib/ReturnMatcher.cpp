#include "ReturnMatcher.hpp"
#include "libmemcached/error.hpp"

bool ReturnMatcher::match(const memcached_return_t &arg) const {
  actual->v = arg;
  if (arg != expected) {
    if (expected == MEMCACHED_SUCCESS && arg == MEMCACHED_BUFFERED && memc) {
      return memcached_behavior_get(memc, MEMCACHED_BEHAVIOR_BUFFER_REQUESTS);
    }
    return false;
  }
  return true;
}

ReturnMatcher ReturnMatcher::success() {
  return ReturnMatcher{memc};
}

ReturnMatcher ReturnMatcher::operator()(memcached_return_t expected_) {
  return ReturnMatcher{memc, expected_};
}

ReturnMatcher &ReturnMatcher::operator=(memcached_st *memc_) {
  memc = memc_;
  return *this;
}

string ReturnMatcher::describe() const {
  return "is " + to_string(expected) + "\n"
      +  "expected:\n"
      +  "  " + memcached_strerror(memc, expected) + "\n"
      +  "actual:\n"
      +  "  " + memcached_strerror(memc, actual->v) + "\n"
      +  "  " + (memcached_has_current_error(*memc) ? memcached_last_error_message(memc) : "");
}

