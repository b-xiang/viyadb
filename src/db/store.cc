/*
 * Copyright (c) 2017 ViyaDB Group
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

#include "codegen/db/store.h"
#include "db/database.h"
#include "db/table.h"
#include "db/store.h"

namespace viya {
namespace db {

namespace cg = viya::codegen;

SegmentStore::SegmentStore(Database& database, Table& table) {
  create_segment_ = cg::CreateSegment(database.compiler(), table).Function();
}

SegmentStore::~SegmentStore() {
  for (auto s : segments_) {
    delete s;
  }
}

}}
