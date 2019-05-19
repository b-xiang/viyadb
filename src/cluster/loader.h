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

#ifndef VIYA_CLUSTER_LOADER_H_
#define VIYA_CLUSTER_LOADER_H_

#include "util/config.h"
#include "util/macros.h"
#include <ThreadPool/ThreadPool.h>
#include <boost/filesystem.hpp>
#include <unordered_map>
#include <vector>

namespace viya {
namespace cluster {

namespace fs = boost::filesystem;

class Controller;

class Loader {
public:
  Loader(const Controller &controller, const std::string &load_prefix);
  DISALLOW_COPY_AND_MOVE(Loader);

  void Load(const util::Config &load_desc,
            const std::string &worker_id = std::string());

private:
  void LoadToAll(const util::Config &load_desc);

  util::Config GetPartitionFilter(const std::string &table_name,
                                  const std::string &worker_id);

  void SendRequest(const std::string &url, const std::string &data);

  std::string ExtractFiles(const std::string &path);

  void ListFiles(const std::string &path, const std::vector<std::string> &exts,
                 std::vector<fs::path> &files);

private:
  const Controller &controller_;
  const std::string load_prefix_;
  ThreadPool load_pool_;
};

} // namespace cluster
} // namespace viya

#endif // VIYA_CLUSTER_LOADER_H_
