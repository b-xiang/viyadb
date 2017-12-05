#include <stdexcept>
#include <algorithm>
#include <glog/logging.h>
#include <cpr/cpr.h>
#include "cluster/consul/consul.h"

namespace viya {
namespace cluster {
namespace consul {

Consul::Consul(const util::Config& config) {
  url_ = config.str("consul_url");
  url_.erase(std::find_if(url_.rbegin(), url_.rend(), [](unsigned char ch) {
    return ch != '/';
  }).base(), url_.end());

  prefix_ = config.str("consul_prefix", "viyadb");
}

std::unique_ptr<Session> Consul::CreateSession(const std::string& name,
    std::function<void(const Session&)> on_create, uint32_t ttl_sec) const {
  return std::make_unique<Session>(*this, name, on_create, ttl_sec);
}

std::unique_ptr<Service> Consul::RegisterService(const std::string& name, uint16_t port, uint32_t ttl_sec, bool auto_hc) const {
  return std::make_unique<Service>(*this, name, port, ttl_sec, auto_hc);
}

std::unique_ptr<LeaderElector> Consul::ElectLeader(const Session& session, const std::string& key) const {
  return std::make_unique<LeaderElector>(*this, session, key);
}

std::unique_ptr<Watch> Consul::WatchKey(const std::string& key) const {
  return std::make_unique<Watch>(*this, key);
}

std::vector<std::string> Consul::ListKeys(const std::string& key) const {
  auto r = cpr::Get(
    cpr::Url { url_ + "/v1/kv/" + prefix_ + "/" + key },
    cpr::Parameters {{ "keys", "true" }}
  );
  switch (r.status_code) {
    case 200: break;
    case 0:
      throw std::runtime_error("Can't contact Consul at: " + url_ + " (host is unreachable)");
    case 404:
      return std::move(std::vector<std::string> {});
    default:
      throw std::runtime_error("Can't list keys (" + r.text + ")");
  }
  auto keys = json::parse(r.text).get<std::vector<std::string>>();
  std::for_each(keys.begin(), keys.end(), [](std::string& key) {
    key.erase(0, key.rfind("/") + 1);
  });
  return std::move(keys);
}

std::string Consul::GetKey(const std::string& key, bool throw_if_not_exists, std::string default_value) const {
  auto r = cpr::Get(
    cpr::Url { url_ + "/v1/kv/" + prefix_ + "/" + key },
    cpr::Parameters {{ "raw", "true" }}
  );
  switch (r.status_code) {
    case 200: break;
    case 0:
      throw std::runtime_error("Can't contact Consul at: " + url_ + " (host is unreachable)");
    case 404:
      if (throw_if_not_exists) {
        throw std::runtime_error("Key doesn't exist: " + prefix_ + "/" + key);
      }
      return default_value;
    default:
      throw std::runtime_error("Can't get key contents (" + r.text + ")");
  }
  return r.text;
}

void Consul::PutKey(const std::string& key, const std::string& content) const {
  auto r = cpr::Put(
    cpr::Url { url_ + "/v1/kv/" + prefix_ + "/" + key },
    cpr::Body { content }
  );
  if (r.status_code != 200) {
    if (r.status_code == 0) {
      throw std::runtime_error("Can't contact Consul at: " + url_ + " (host is unreachable)");
    }
    throw std::runtime_error("Can't put key contents (" + r.text + ")");
  }
}

}}}
