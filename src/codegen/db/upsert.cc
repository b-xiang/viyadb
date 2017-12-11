#include "db/defs.h"
#include "db/table.h"
#include "db/database.h"
#include "db/column.h"
#include "input/loader_desc.h"
#include "codegen/db/upsert.h"
#include "codegen/db/store.h"
#include "codegen/db/rollup.h"

namespace viya {
namespace codegen {

namespace db = viya::db;

void ValueParser::Visit(const db::StrDimension* dimension) {
  auto dim_idx = std::to_string(dimension->index());
  code_<<" auto& value = values["<<tuple_idx_map_[value_idx_++]<<"];\n";

  auto max_length = dimension->length();
  if (max_length != -1) {
    code_<<" if (UNLIKELY(value.length() > "<<std::to_string(max_length)<<")) {\n";
    code_<<"  value.erase("<<std::to_string(max_length)<<");\n";
    code_<<" }\n";
  }

  code_<<" auto it = v2c"<<dim_idx<<"->find(value);\n";
  code_<<" if (LIKELY(it != v2c"<<dim_idx<<"->end())) {\n";
  code_<<"  upsert_dims._"<<dim_idx<<" = it->second;\n";
  code_<<" } else {\n";
  code_<<"  dict"<<dim_idx<<"->lock().lock();\n";
  code_<<"  auto code = dict"<<dim_idx<<"->c2v().size();\n";

  auto cardinality = dimension->cardinality();
  bool check_cardinality = cardinality < UINT64_MAX - 1;
  if (check_cardinality) {
    code_<<"  if(LIKELY(code <= "<<cardinality<<"U)) {\n";
  }
  code_<<"  upsert_dims._"<<dim_idx<<" = code;\n";
  code_<<"  v2c"<<dim_idx<<"->insert(std::make_pair(value, code));\n";
  code_<<"  dict"<<dim_idx<<"->c2v().push_back(value);\n";
  if (check_cardinality) {
    code_<<"  } else {\n";
    code_<<"   upsert_dims._"<<dim_idx<<" = 0;\n";
    code_<<"  }\n";
  }

  code_<<"  dict"<<dim_idx<<"->lock().unlock();\n";
  code_<<" }\n";
}

void ValueParser::Visit(const db::NumDimension* dimension) {
  code_<<" upsert_dims._"<<std::to_string(dimension->index())
    <<" = "<<dimension->num_type().cpp_parse_fn()<<"(values["<<tuple_idx_map_[value_idx_++]<<"]);\n";
}

void ValueParser::Visit(const db::TimeDimension* dimension) {
  auto dim_idx = std::to_string(dimension->index());

  auto format = dimension->format();
  bool is_posix_ts = (format == "posix");
  bool is_milli_ts = (format == "millis");
  bool is_micro_ts = (format == "micros");

  bool is_num_input = format.empty() || is_posix_ts || is_milli_ts || is_micro_ts;
  if (is_num_input) {
    code_<<" {\n";
    code_<<"  uint64_t ts_val = std::stoull(values["<<tuple_idx_map_[value_idx_]<<"]);\n";
    if (dimension->micro_precision()) {
      if (is_posix_ts) {
        code_<<"  ts_val *= 1000000L;\n";
      } else if (is_milli_ts) {
        code_<<"  ts_val *= 1000L;\n";
      }
    } else {
      if (is_milli_ts) {
        code_<<"  ts_val /= 1000L;\n";
      } else if (is_micro_ts) {
        code_<<"  ts_val /= 1000000L;\n";
      }
    }
    code_<<"  upsert_dims._"<<dim_idx<<" = ts_val;\n";
    code_<<" }\n";
    if (!dimension->rollup_rules().empty() || !dimension->granularity().empty()) {
      code_<<" time"<<dim_idx<<".set_ts(upsert_dims._"<<dim_idx<<");\n";
    }
  } else {
    code_<<" time"<<dim_idx<<".parse(\""<<format<<"\", values["<<tuple_idx_map_[value_idx_]<<"]);\n";
    if (!dimension->rollup_rules().empty()) {
      code_<<" upsert_dims._"<<dim_idx<<" = time"<<dim_idx<<".get_ts();\n";
    }
  }

  if (!dimension->rollup_rules().empty()) {
    TimestampRollup ts_rollup(dimension, "upsert_dims._" + dim_idx);
    code_<<ts_rollup.GenerateCode();
  }
  else if (!dimension->granularity().empty()) {
    code_<<"time"<<dim_idx<<".trunc<static_cast<util::TimeUnit>("
      <<static_cast<int>(dimension->granularity().time_unit())<<")>();\n";
  }

  if (!is_num_input || !dimension->rollup_rules().empty() || !dimension->granularity().empty()) {
    code_<<" upsert_dims._"<<dim_idx<<" = time"<<dim_idx<<".get_ts();\n";
  }

  ++value_idx_;
}

void ValueParser::Visit(const db::BoolDimension* dimension) {
  code_<<" upsert_dims._"<<std::to_string(dimension->index())
    <<" = values["<<tuple_idx_map_[value_idx_++]<<"] == \"true\";\n";
}

void ValueParser::Visit(const db::ValueMetric* metric) {
  auto metric_idx = std::to_string(metric->index());
  code_<<" upsert_metrics._"<<metric_idx<<" = ";
  if (metric->agg_type() == db::Metric::AggregationType::COUNT) {
    code_<<"1";
  } else {
    code_<<metric->num_type().cpp_parse_fn()<<"(values["<<tuple_idx_map_[value_idx_++]<<"])";
  }
  code_<<";\n";
}

void ValueParser::Visit(const db::BitsetMetric* metric) {
  auto metric_idx = std::to_string(metric->index());
  code_<<" "<<metric->num_type().cpp_type()<<" metric_val"<<metric_idx<<" = "
    <<metric->num_type().cpp_parse_fn()<<"(values["<<tuple_idx_map_[value_idx_++]<<"]);\n";
  code_<<" upsert_metrics._"<<metric_idx<<".add(metric_val"<<metric_idx<<");\n";
}

UpsertGenerator::UpsertGenerator(const input::LoaderDesc& desc):
  FunctionGenerator(const_cast<db::Database&>(desc.table().database()).compiler()),
  desc_(desc) {
}

Code UpsertGenerator::SetupFunctionCode() const {
  Code code;
  auto& table = desc_.table();
  auto& cardinality_guards = table.cardinality_guards();

  code.AddHeaders({
    "vector", "string", "db/store.h", "db/table.h", "db/dictionary.h",
    "util/likely.h", "input/loader_desc.h"
  });

  if (!cardinality_guards.empty()) {
    code.AddHeaders({"util/bitset.h"});
  }
  if (desc_.has_partition_filter()) {
    code.AddHeaders({"util/crc32.h"});
  }

  code<<"namespace input = viya::input;\n";

  bool add_optimize = AddOptimize();
  bool has_time_dim = std::any_of(table.dimensions().cbegin(), table.dimensions().cend(),
                [] (const db::Dimension* dim) { return dim->dim_type() == db::Dimension::DimType::TIME; });

  StoreDefs store_defs(table);
  code<<store_defs.GenerateCode();

  code<<"static db::Table* table;\n";
  code<<"static db::UpsertStats stats;\n";
  code<<"static Dimensions upsert_dims;\n";
  code<<"static Metrics upsert_metrics;\n";
  code<<"static std::unordered_map<Dimensions,size_t,DimensionsHasher> tuple_offsets;\n";
  if (add_optimize) {
    code<<"uint32_t updates_before_optimize = 1000000L;\n";
  }

  if (has_time_dim) {
    RollupDefs rollup_defs(table.dimensions());
    code<<rollup_defs.GenerateCode();
  }

  for (auto& guard : cardinality_guards) {
    auto dim_idx = std::to_string(guard.dim()->index());
    std::string struct_name = "CardDimKey" + dim_idx;
    DimensionsStruct card_key_struct(guard.dimensions(), struct_name);
    code<<card_key_struct.GenerateCode();
    code<<"static "<<struct_name<<" card_dim_key"<<dim_idx<<";\n";

    code<<"static std::unordered_map<"<<struct_name<<","
      <<"Bitset<"<<std::to_string(guard.dim()->num_type().size())<<">"
      <<","<<struct_name<<"Hasher> card_stats"<<dim_idx<<";\n";
  }

  for (auto* dimension : table.dimensions()) {
    auto dim_idx = std::to_string(dimension->index());
    if (dimension->dim_type() == db::Dimension::DimType::STRING) {
      code<<"static db::DimensionDict* dict"<<dim_idx<<";\n";
      code<<"static db::DictImpl<"<<dimension->num_type().cpp_type()<<">* v2c"<<dim_idx<<";\n";
    }
  }

  for (auto* metric : table.metrics()) {
    if (metric->agg_type() == db::Metric::AggregationType::BITSET) {
      code<<"Bitset<"<<std::to_string(metric->num_type().size())
        <<"> empty_bitset"<<std::to_string(metric->index())<<";\n";
    }
  }

  if (desc_.has_partition_filter()) {
    code<<"static bool partition_values["<<desc_.partition_filter().total_partitions()<<"] = {false};\n";
  }

  code<<OptimizeFunctionCode();

  code<<"extern \"C\" void viya_upsert_setup(const input::LoaderDesc& desc) __attribute__((__visibility__(\"default\")));\n";
  code<<"extern \"C\" void viya_upsert_setup(const input::LoaderDesc& desc) {\n";
  code<<" table = const_cast<db::Table*>(&(desc.table()));\n";
  for (auto* dimension : table.dimensions()) {
    if (dimension->dim_type() == db::Dimension::DimType::STRING) {
      auto dim_idx = std::to_string(dimension->index());
      code<<" dict"<<dim_idx<<" = static_cast<const db::StrDimension*>(table->dimension("<<dim_idx<<"))->dict();\n";
      code<<" v2c"<<dim_idx<<" = reinterpret_cast<db::DictImpl<"<<dimension->num_type().cpp_type()
        <<">*>(dict"<<dim_idx<<"->v2c());\n";
    }
  }
  if (desc_.has_partition_filter()) {
    code<<" for(auto& v : desc.partition_filter().values()) partition_values[v] = true;\n";
  }
  code<<"}\n";

  code<<"extern \"C\" void viya_upsert_before() __attribute__((__visibility__(\"default\")));\n";
  code<<"extern \"C\" void viya_upsert_before() {\n";
  code<<" stats = db::UpsertStats();\n";
  if (has_time_dim) {
    RollupReset rollup_reset(table.dimensions());
    code<<rollup_reset.GenerateCode();
  }
  code<<"}\n";

  code<<"extern \"C\" db::UpsertStats viya_upsert_after() __attribute__((__visibility__(\"default\")));\n";
  code<<"extern \"C\" db::UpsertStats viya_upsert_after() {\n";
  code<<" viya_upsert_optimize();\n";
  code<<" return stats;\n";
  code<<"}\n";

  return code;
}

bool UpsertGenerator::AddOptimize() const {
  auto& table = desc_.table();
  bool has_bitset_metric = std::any_of(table.metrics().cbegin(), table.metrics().cend(),
                [] (const db::Metric* metric) { return metric->agg_type() == db::Metric::AggregationType::BITSET; });
  return has_bitset_metric || !table.cardinality_guards().empty();
}

Code UpsertGenerator::OptimizeFunctionCode() const {
  auto& table = desc_.table();
  Code code;
  code<<"void viya_upsert_optimize() {\n";

  bool add_optimize = AddOptimize();
  if (add_optimize) {
    for (auto& guard : table.cardinality_guards()) {
      auto dim_idx = std::to_string(guard.dim()->index());
      code<<" for (auto it = card_stats"<<dim_idx<<".begin(); it != card_stats"<<dim_idx<<".end(); ++it) {\n";
      code<<"  it->second.optimize();\n";
      code<<" }\n";
    }

    std::vector<const db::Metric*> bitset_metrics;
    for (auto* metric : table.metrics()) {
      if (metric->agg_type() == db::Metric::AggregationType::BITSET) {
        bitset_metrics.push_back(metric);
      }
    }
    if (!bitset_metrics.empty()) {
      code<<" for (auto* s : table->store()->segments_copy()) {\n";
      code<<"  auto segment_size = s->size();\n";
      code<<"  auto metrics = static_cast<Segment*>(s)->m;\n";
      code<<"  for (size_t tuple_idx = 0; tuple_idx < segment_size; ++tuple_idx) {\n";
      for (auto* metric : bitset_metrics) {
        code<<"   (metrics + tuple_idx)->_"<<std::to_string(metric->index())<<".optimize();\n";
      }
      code<<"  }\n";
      code<<" }\n";
    }

    code<<" updates_before_optimize = 1000000L;\n";
  }
  code<<"}\n";
  return code;
}

Code UpsertGenerator::CardinalityProtection() const {
  Code code;
  auto& table = desc_.table();

  for (auto& guard : table.cardinality_guards()) {
    code<<"{\n";
    auto dim_idx = std::to_string(guard.dim()->index());
    for (auto per_dim : guard.dimensions()) {
      auto per_dim_idx = std::to_string(per_dim->index());
      code<<" card_dim_key"<<dim_idx<<"._"<<per_dim_idx<<" = upsert_dims._"<<per_dim_idx<<";\n";
    }
    code<<" auto it = card_stats"<<dim_idx<<".find(card_dim_key"<<dim_idx<<");\n";
    code<<" if (it == card_stats"<<dim_idx<<".end()) {\n";
    code<<"  Bitset<"<<std::to_string(guard.dim()->num_type().size())<<"> bitset;\n";
    code<<"  bitset.add(upsert_dims._"<<dim_idx<<");\n";
    code<<"  card_stats"<<dim_idx<<".insert(std::make_pair(card_dim_key"<<dim_idx<<", std::move(bitset)));\n";
    code<<" } else {\n";
    code<<"  auto& bitset = it->second;\n";
    code<<"  if (UNLIKELY(bitset.cardinality() >= "<<guard.limit()<<")) {\n";
    code<<"   if (UNLIKELY(!bitset.contains(upsert_dims._"<<dim_idx<<"))) {\n";
    code<<"    upsert_dims._"<<dim_idx<<" = 0;\n";
    code<<"   }\n";
    code<<"  } else {\n";
    code<<"   bitset.add(upsert_dims._"<<dim_idx<<");\n";
    code<<"  }\n";
    code<<" }\n";
    code<<"}\n";
  }
  return code;
}

Code UpsertGenerator::PartitionFilter() const {
  Code code;
  if (desc_.has_partition_filter()) {
    auto& part_filter = desc_.partition_filter();
    auto & tuple_idx_map = desc_.tuple_idx_map();
    auto& table = desc_.table();
    code<<"{\n";
    code<<" uint32_t hash = 0;\n";
    for (auto& key_col : part_filter.columns()) {
      auto dim = table.dimension(key_col);
      code<<" {\n";
      code<<"  auto& value = values["<<tuple_idx_map[dim->index()]<<"];\n";
      code<<"  hash = crc32(hash, reinterpret_cast<const unsigned char*>(value.c_str()), value.size());\n";
      code<<" }\n";
    }
    code<<" if(!partition_values[hash % "<<std::to_string(part_filter.total_partitions())<<"]) return;\n";
    code<<"}\n";
  }
  return code;
}

Code UpsertGenerator::GenerateCode() const {
  Code code;
  auto& table = desc_.table();

  code<<SetupFunctionCode();

  code<<"extern \"C\" void viya_upsert_do(std::vector<std::string>& values) __attribute__((__visibility__(\"default\")));\n";
  code<<"extern \"C\" void viya_upsert_do(std::vector<std::string>& values) {\n";

  code<<PartitionFilter();

  size_t value_idx = 0;
  bool add_optimize = AddOptimize();
  ValueParser value_parser(code, desc_.tuple_idx_map(), value_idx);

  for (auto* dimension : table.dimensions()) {
    code<<"{\n";
    dimension->Accept(value_parser);
    code<<"}\n";
  }

  bool has_count_metric = false;
  bool has_avg_metric = false;
  for (auto* metric : table.metrics()) {
    if (metric->agg_type() == db::Metric::AggregationType::AVG) {
      has_avg_metric = true;
    } else if (metric->agg_type() == db::Metric::AggregationType::COUNT) {
      has_count_metric = true;
    }
    metric->Accept(value_parser);
  }
  if (has_avg_metric && !has_count_metric) {
    code<<" upsert_metrics._count = 1;\n";
  }

  code<<CardinalityProtection();

  code<<" auto* store = table->store();\n";
  code<<" auto& segments = store->segments();\n";
  code<<" auto offset_it = tuple_offsets.find(upsert_dims);\n";
  code<<" if (offset_it != tuple_offsets.end()) {\n";
  auto segment_size = std::to_string(table.segment_size());
  code<<"  size_t global_idx = offset_it->second;\n";
  code<<"  size_t segment_idx = global_idx / " <<segment_size<<";\n";
  code<<"  size_t tuple_idx = global_idx % " <<segment_size<<";\n";
  code<<"  Metrics* metrics = static_cast<Segment*>(segments[segment_idx])->m;\n";
  code<<"  (metrics + tuple_idx)->Update(upsert_metrics);\n";
  if (add_optimize) {
    code<<"  if (--updates_before_optimize == 0) {\n";
    code<<"   viya_upsert_optimize();\n";
    code<<"  }\n";
  }
  code<<" } else {\n";
  code<<"  auto last_segment = static_cast<Segment*>(store->last());\n";
  code<<"  size_t segment_idx = segments.size() - 1;\n";
  code<<"  size_t tuple_idx = last_segment->size();\n";
  code<<"  last_segment->insert(upsert_dims, upsert_metrics);\n";
  code<<"  last_segment->stats.Update(upsert_dims);\n";
  code<<"  stats.new_recs++;\n";
  code<<"  tuple_offsets.insert(std::make_pair(upsert_dims, segment_idx * "<<segment_size<<" + tuple_idx));\n";
  code<<" }\n";

  // Empty temporary roaring bitsets:
  for (auto* metric : table.metrics()) {
    if (metric->agg_type() == db::Metric::AggregationType::BITSET) {
      auto metric_idx = std::to_string(metric->index());
      code<<" upsert_metrics._"<<metric_idx<<" = empty_bitset"<<metric_idx<<";\n";
    }
  }
  code<<"}\n";
  return code;
}

UpsertSetupFn UpsertGenerator::SetupFunction() {
  return GenerateFunction<UpsertSetupFn>(std::string("viya_upsert_setup"));
}

BeforeUpsertFn UpsertGenerator::BeforeFunction() {
  return GenerateFunction<BeforeUpsertFn>(std::string("viya_upsert_before"));
}

AfterUpsertFn UpsertGenerator::AfterFunction() {
  return GenerateFunction<AfterUpsertFn>(std::string("viya_upsert_after"));
}

UpsertFn UpsertGenerator::Function() {
  return GenerateFunction<UpsertFn>(std::string("viya_upsert_do"));
}

}}

