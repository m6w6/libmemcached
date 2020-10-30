#include "Cluster.hpp"
#include "Retry.hpp"

#include <algorithm>
#include <sys/wait.h>

Cluster::Cluster(Server serv, size_t cnt)
: count{cnt}
, proto{move(serv)}
{
  if (!count) {
    count = 1;
  }
  for (size_t i = 0; i < count; ++i) {
    cluster.push_back(proto);
  }
}

Cluster::~Cluster() {
  stop();
  wait();
}

const vector<Server> &Cluster::getServers() const {
  return cluster;
}

bool Cluster::start() {
  bool started = true;

  for (auto &server : cluster) {
    if (!server.start()) {
      started = false;
      continue;
    }
    pids[server.getPid()] = &server;
  }

  return started;
}

void Cluster::stop(bool graceful) {
  for (auto &server : cluster) {
    server.drain();
    if (graceful) {
      server.stop();
    } else {
      // no cookies for memcached; TERM is just too slow
      server.signal(SIGKILL);
    }
  }
}

bool Cluster::isStopped() {
  return none_of(
#if HAVE_EXECUTION
    execution::par,
#endif
      cluster.begin(), cluster.end(), [](Server &s) {
    return s.getPid() && !s.tryWait();
  });
}

bool Cluster::isListening() const {
  return all_of(
#if HAVE_EXECUTION
    execution::par,
#endif
      cluster.cbegin(), cluster.cend(), [](const Server &s) {
    return s.isListening();
  });
}

bool Cluster::ensureListening() {
  if (!start()) {
    return false;
  }
  auto listening = all_of(
#if HAVE_EXECUTION
    execution::par,
#endif
  cluster.begin(), cluster.end(), [](Server &s) {
    return s.ensureListening();
  });

  pids.clear();
  for (auto &server : cluster) {
    pids[server.getPid()] = &server;
  }

  return listening;
}

void Cluster::wait() {
  siginfo_t inf;

  while (!isStopped()) {
    if (waitid(P_ALL, 0, &inf, WEXITED | WNOWAIT)) {
      perror("Cluster::wait waitid()");
      return;
    }

    auto server = pids.find(inf.si_pid);
    if (server != pids.end()) {
      server->second->wait();
    }
  }
}

bool Cluster::restart() {
  stop();
  wait();
  return start();
}
