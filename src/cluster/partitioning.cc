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

#include "cluster/partitioning.h"
#include <nlohmann/json.hpp>

namespace viya {
namespace cluster {

Partitioning::Partitioning(const json &json) {
  mapping_ = json["mapping"].get<std::vector<uint32_t>>();
  total_ = json["total"].get<size_t>();
  columns_ = json["columns"].get<std::vector<std::string>>();
}

Partitioning::Partitioning(const std::vector<uint32_t> &mapping, size_t total,
                           const std::vector<std::string> &columns)
    : mapping_(mapping), total_(total), columns_(columns) {}

json Partitioning::ToJson() const {
  return json(
      {{"mapping", mapping_}, {"total", total_}, {"columns", columns_}});
}

} // namespace cluster
} // namespace viya
