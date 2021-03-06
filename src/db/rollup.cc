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

#include "db/rollup.h"

namespace viya {
namespace db {

Granularity::Granularity(const std::string &name)
    : time_unit_(util::time_unit_by_name(name)) {}

RollupRule::RollupRule(const util::Config &config)
    : granularity_(config.str("granularity")), after_(config.str("after")) {}
} // namespace db
} // namespace viya
