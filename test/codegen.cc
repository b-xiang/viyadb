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

#include "codegen/compiler.h"
#include "util/config.h"
#include <gtest/gtest.h>

namespace cg = viya::codegen;
namespace util = viya::util;

TEST(Codegen, Basic) {
  std::string code = "#include \"util/config.h\"\nint viya_foo() "
                     "__attribute__((__visibility__(\"default\"))); int "
                     "viya_foo() { return 123; }";
  util::Config config;
  cg::Compiler compiler(config);
  auto library = compiler.Compile(code);
  auto func = library->GetFunction<int (*)()>(std::string("_Z8viya_foov"));

  EXPECT_EQ(123, func());
}
