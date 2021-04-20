#include "MemcachedCluster.hpp"
#include "Retry.hpp"

const memcached_st MemcachedCluster::empty_memc{};

void MemcachedCluster::init() {
  REQUIRE(cluster.start());

  while (!cluster.ensureListening()) {
    cluster.restart();
  }

  if (auto br = getenv_else("MEMCACHED_BREAK", "0")) {
    if (*br && *br != '0') {
      string in;

      cout << "Started servers:\n";
      for (const auto &server : cluster.getServers()) {
        cout << server.getPid() << " ";
      }
      cout << "\nPress ENTER to continue... " << ::flush;
      cin.get();
    }
  }

  REQUIRE(memcached_create(&memc));

  for (const auto &[behavior, value] : to_set) {
    REQUIRE(MEMCACHED_SUCCESS == memcached_behavior_set(&memc, behavior, value));
  }

  for (const auto &server : cluster.getServers()) {
    auto target = server.getSocketOrPort();
    if (holds_alternative<string>(target)) {
      REQUIRE(MEMCACHED_SUCCESS == memcached_server_add_unix_socket(&memc, get<string>(target).c_str()));
    } else {
      REQUIRE(MEMCACHED_SUCCESS == memcached_server_add(&memc, "localhost", get<int>(target)));
    }
  }

}

MemcachedCluster::~MemcachedCluster() {
  if (!!memcmp(&memc, &empty_memc, sizeof(memc))) {
    memcached_free(&memc);
  }
}

void MemcachedCluster::flush() {
  REQUIRE(MEMCACHED_SUCCESS == memcached_flush(&memc, 0));
}

MemcachedCluster::MemcachedCluster()
: cluster{Server{
  MEMCACHED_BINARY,
  {random_socket_or_port_arg()}
}}
{
  init();
}

MemcachedCluster::MemcachedCluster(Cluster &&cluster_, behaviors_t to_set_)
: cluster{move(cluster_)}
, to_set{move(to_set_)}
{
  init();
}

MemcachedCluster::MemcachedCluster(MemcachedCluster &&mc) noexcept
    : cluster{Server{}}
{
  *this = move(mc);
}

MemcachedCluster &MemcachedCluster::operator=(MemcachedCluster &&mc) noexcept {
  cluster = move(mc.cluster);
  memcached_clone(&memc, &mc.memc);
  returns = &memc;
  return *this;
}

MemcachedCluster MemcachedCluster::mixed() {
  return MemcachedCluster{};
}

MemcachedCluster MemcachedCluster::network() {
  return MemcachedCluster{Cluster{Server{
    MEMCACHED_BINARY,
    {"-p", random_socket_or_port_string}
  }}};
}

MemcachedCluster MemcachedCluster::socket() {
  return MemcachedCluster{Cluster{Server{
    MEMCACHED_BINARY,
    {"-s", random_socket_or_port_string}
  }}};
}

MemcachedCluster MemcachedCluster::udp() {
  return MemcachedCluster{Cluster{Server{
    MEMCACHED_BINARY,
    {
      Server::arg_pair_t{"-U", random_socket_or_port_string},
      Server::arg_t{"-v"}
    }
  }}, {
      {MEMCACHED_BEHAVIOR_USE_UDP, 1}
  }};
}

#if LIBMEMCACHED_WITH_SASL_SUPPORT
MemcachedCluster MemcachedCluster::sasl() {
  auto mc = MemcachedCluster{Cluster{Server{
      MEMCACHED_BINARY,
      {
        Server::arg_pair_t{"-p", random_socket_or_port_string},
        Server::arg_t{"-S"}
      }
  }}, {
      {MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1},
  }};
  REQUIRE(MEMCACHED_SUCCESS == memcached_set_sasl_auth_data(&mc.memc,
      "memcached", "memcached"));
  return mc;
}
#endif

void MemcachedCluster::enableBinaryProto(bool enable) {
  REQUIRE(MEMCACHED_SUCCESS == memcached_behavior_set(&memc,
      MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, enable));
}

void MemcachedCluster::enableMetaProto(bool enable) {
  REQUIRE(MEMCACHED_SUCCESS == memcached_behavior_set(&memc,
      MEMCACHED_BEHAVIOR_META_PROTOCOL, enable));
}

void MemcachedCluster::enableBuffering(bool enable) {
  REQUIRE(MEMCACHED_SUCCESS == memcached_behavior_set(&memc,
      MEMCACHED_BEHAVIOR_BUFFER_REQUESTS, enable));
}

void MemcachedCluster::enableReplication() {
  REQUIRE(MEMCACHED_SUCCESS == memcached_behavior_set(&memc,
      MEMCACHED_BEHAVIOR_NUMBER_OF_REPLICAS, memcached_server_count(&memc) - 1));
}

void MemcachedCluster::killOneServer() const {
  const auto &servers = cluster.getServers();
  const auto &victim = servers[random_num((size_t)0, servers.size() - 1)];
  ::kill(victim.getPid(), SIGKILL);
}

bool MemcachedCluster::isGEVersion(uint8_t major, uint8_t minor, uint8_t micro) {
  REQUIRE(MEMCACHED_SUCCESS == memcached_version(&memc));
  auto inst = memcached_server_instance_by_position(&memc, 0);
  auto maj = memcached_server_major_version(inst);
  auto min = memcached_server_minor_version(inst);
  auto mic = memcached_server_micro_version(inst);

  return (maj > major)
      || (maj == major && min > minor)
      || (maj == major && min == minor && mic >= micro);
}
