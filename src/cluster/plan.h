/*
 * Copyright (c) 2017-present ViyaDB Group
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VIYA_CLUSTER_PLAN_H_
#define VIYA_CLUSTER_PLAN_H_

#include "util/config.h"
#include "util/macros.h"
#include <map>
#include <nlohmann/json_fwd.hpp>
#include <vector>

namespace viya {
namespace cluster {

namespace util = viya::util;

using json = nlohmann::json;

class Placement {
public:
  Placement(const std::string &hostname, uint16_t port)
      : hostname_(hostname), port_(port) {}

  bool operator==(const Placement &other) const {
    return hostname_ == other.hostname_ && port_ == other.port_;
  }

  const std::string &hostname() const { return hostname_; }
  uint16_t port() const { return port_; }

private:
  std::string hostname_;
  uint16_t port_;
};

using Replicas = std::vector<Placement>;
using Partitions = std::vector<Replicas>;

class Plan {
public:
  Plan(const json &plan);
  Plan(const Partitions &partitions);
  DISALLOW_COPY(Plan);
  Plan(Plan &&other) = default;

  const Partitions &partitions() const { return partitions_; }
  const std::map<std::string, uint32_t> &workers_partitions() const {
    return workers_partitions_;
  }
  const std::vector<std::vector<std::string>> &partitions_workers() const {
    return partitions_workers_;
  }

  bool operator==(const Plan &other) const {
    return partitions_ == other.partitions_;
  }

  json ToJson() const;

private:
  void AssignPartitionsToWorkers();

private:
  Partitions partitions_;
  std::map<std::string, uint32_t> workers_partitions_;
  std::vector<std::vector<std::string>> partitions_workers_;
};

class PlanGenerator {
public:
  PlanGenerator() {}
  DISALLOW_COPY_AND_MOVE(PlanGenerator);

  Plan Generate(size_t partitions_num, size_t replication_factor,
                const std::map<std::string, util::Config> &workers_configs);
};

} // namespace cluster
} // namespace viya

#endif // VIYA_CLUSTER_PLAN_H_
