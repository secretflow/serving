// Copyright 2024 Ant Group Co., Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "arrow/api.h"

#include "secretflow_serving/protos/graph.pb.h"
#include "secretflow_serving/protos/op.pb.h"

namespace secretflow::serving::op {

struct TreeNode {
  int32_t id;
  // [common] left child. `-1` means not valid
  int32_t lchild_id;
  // [common] right child. `-1` means not valid
  int32_t rchild_id;
  // [common]
  bool is_leaf;
  // [internal] split feature index, in party's dataset space. `-1` means
  // feature not belong to self or not valid.
  int32_t split_feature_idx;
  // [internal] split value, goes left when less than it. valid when `is_leaf ==
  // false`
  double split_value;
  // only for leaf node;
  int32_t leaf_index = -1;
};

struct Tree {
  int32_t root_id;

  int32_t num_leaf;

  std::set<int32_t> used_feature_idx_list;

  std::map<int32_t, TreeNode> nodes;
};

struct TreePredictSelect {
  // | counts of padding bits in last bit map | 0-7 bit map | 8-15 bit map | ...
  std::vector<uint8_t> select;

  TreePredictSelect() = default;
  explicit TreePredictSelect(const std::string& str);
  explicit TreePredictSelect(std::string_view str);
  explicit TreePredictSelect(const std::vector<uint64_t>& u64s);

  void SetSelectBuf(const std::string& str);

  void SetSelectBuf(std::string_view str);

  void SetLeafs(size_t leafs, bool all_selected = false);

  [[nodiscard]] size_t Leafs() const;

  void Merge(const TreePredictSelect& s);

  void SetLeafSelected(uint32_t leaf_idx);

  [[nodiscard]] int32_t GetLeafIndex() const;

  [[nodiscard]] bool CheckLeafSelected(uint32_t leaf_idx) const;

  [[nodiscard]] std::vector<uint64_t> ToU64Vec() const;

  static size_t GetSelectsU64VecSize(size_t leaf_num);
};

void BuildTreeFromNode(const NodeDef& node_def, const OpDef& op_def,
                       const std::vector<std::string>& feature_list,
                       Tree* tree);

std::vector<TreePredictSelect> TreePredict(
    const Tree& tree, const std::shared_ptr<arrow::RecordBatch>& input);

}  // namespace secretflow::serving::op
