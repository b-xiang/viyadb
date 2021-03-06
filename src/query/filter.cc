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

#include "query/filter.h"
#include "util/config.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace viya {
namespace query {

void RelOpFilter::Accept(FilterVisitor &visitor) const { visitor.Visit(this); }

void InFilter::Accept(FilterVisitor &visitor) const { visitor.Visit(this); }

void CompositeFilter::Accept(FilterVisitor &visitor) const {
  visitor.Visit(this);
}

void EmptyFilter::Accept(FilterVisitor &visitor) const { visitor.Visit(this); }

Filter *FilterFactory::Create(const util::Config &config, bool negate) {
  if (!config.exists("op")) {
    return new EmptyFilter();
  }
  std::string op = config.str("op");
  if (op == "and" || op == "or") {
    std::vector<Filter *> filters;
    for (util::Config &filter_conf : config.sublist("filters")) {
      filters.push_back(Create(filter_conf, negate));
    }
    std::sort(filters.begin(), filters.end(),
              [](const Filter *a, const Filter *b) -> bool {
                return a->precedence() < b->precedence();
              });
    if (negate) {
      return new CompositeFilter(op == "and" ? CompositeFilter::Operator::OR
                                             : CompositeFilter::Operator::AND,
                                 filters);
    }
    return new CompositeFilter(op == "and" ? CompositeFilter::Operator::AND
                                           : CompositeFilter::Operator::OR,
                               filters);
  }
  if (op == "not") {
    return Create(config.sub("filter"), !negate);
  }

  const auto column = config.str("column");
  if (op == "in") {
    auto values = config.strlist("values");
    return new InFilter(column, values, !negate);
  }

  auto value = config.str("value");
  if (op == "eq") {
    if (negate) {
      return new RelOpFilter(RelOpFilter::Operator::NOT_EQUAL, column, value);
    }
    return new RelOpFilter(RelOpFilter::Operator::EQUAL, column, value);
  }
  if (op == "ne") {
    if (negate) {
      return new RelOpFilter(RelOpFilter::Operator::EQUAL, column, value);
    }
    return new RelOpFilter(RelOpFilter::Operator::NOT_EQUAL, column, value);
  }
  if (op == "lt") {
    if (negate) {
      return new RelOpFilter(RelOpFilter::Operator::GREATER_EQUAL, column,
                             value);
    }
    return new RelOpFilter(RelOpFilter::Operator::LESS, column, value);
  }
  if (op == "le") {
    if (negate) {
      return new RelOpFilter(RelOpFilter::Operator::GREATER, column, value);
    }
    return new RelOpFilter(RelOpFilter::Operator::LESS_EQUAL, column, value);
  }
  if (op == "gt") {
    if (negate) {
      return new RelOpFilter(RelOpFilter::Operator::LESS_EQUAL, column, value);
    }
    return new RelOpFilter(RelOpFilter::Operator::GREATER, column, value);
  }
  if (op == "ge") {
    if (negate) {
      return new RelOpFilter(RelOpFilter::Operator::LESS, column, value);
    }
    return new RelOpFilter(RelOpFilter::Operator::GREATER_EQUAL, column, value);
  }
  throw std::invalid_argument("Unsupported filter operataor: " + op);
}

void ColumnsCollector::Visit(const query::RelOpFilter *filter) {
  columns_.insert(filter->column());
}

void ColumnsCollector::Visit(const query::InFilter *filter) {
  columns_.insert(filter->column());
}

void ColumnsCollector::Visit(const query::CompositeFilter *filter) {
  for (auto f : filter->filters()) {
    f->Accept(*this);
  }
}

void ColumnsCollector::Visit(const query::EmptyFilter *filter
                             __attribute__((unused))) {}

} // namespace query
} // namespace viya
