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

#include "secretflow_serving/ops/tree_utils.h"

#include <cmath>
#include <queue>

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op {

TreePredictSelect::TreePredictSelect(const std::string& str) {
  select.resize(str.size());
  std::memcpy(select.data(), str.data(), str.size());
}

TreePredictSelect::TreePredictSelect(std::string_view str) {
  select.resize(str.size());
  std::memcpy(select.data(), str.data(), str.size());
}

TreePredictSelect::TreePredictSelect(const std::vector<uint64_t>& u64s) {
  SERVING_ENFORCE_GT(u64s.size(), 1U);
  auto size = u64s[0];
  select.reserve(size);
  for (size_t i = 1; i < u64s.size(); ++i) {
    uint64_t combined = u64s[i];
    for (size_t j = 0; j < 8 && select.size() < size; ++j) {
      select.push_back(static_cast<uint8_t>((combined >> (j * 8)) & 0xFF));
    }
  }
}

void TreePredictSelect::SetSelectBuf(const std::string& str) {
  select.resize(str.size());
  std::memcpy(select.data(), str.data(), str.size());
}

void TreePredictSelect::SetSelectBuf(std::string_view str) {
  select.resize(str.size());
  std::memcpy(select.data(), str.data(), str.size());
}

void TreePredictSelect::SetLeafs(size_t leafs, bool all_selected) {
  select.resize(std::ceil(leafs / 8.0) + 1, 0);
  uint8_t pad_bits = ((leafs % 8) != 0U) ? 8 - (leafs % 8) : 0;
  select[0] = pad_bits;
  if (all_selected) {
    for (size_t i = 1; i < select.size(); ++i) {
      select[i] = std::numeric_limits<uint8_t>::max();
    }
  }
}

size_t TreePredictSelect::Leafs() const {
  if (select.empty()) {
    return 0;
  }
  return ((select.size() - 1) * 8) - select[0];
}

void TreePredictSelect::Merge(const TreePredictSelect& s) {
  SERVING_ENFORCE(Leafs(), errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE_EQ(Leafs(), s.Leafs());
  for (size_t i = 1; i < s.select.size(); i++) {
    select[i] &= s.select[i];
  }
}

void TreePredictSelect::SetLeafSelected(uint32_t leaf_idx) {
  SERVING_ENFORCE_LT(leaf_idx, Leafs());
  size_t vec_idx = (leaf_idx / 8) + 1;
  uint8_t bit_mask = 1 << (leaf_idx % 8);
  select[vec_idx] |= bit_mask;
}

int32_t TreePredictSelect::GetLeafIndex() const {
  SERVING_ENFORCE(Leafs(), errors::ErrorCode::LOGIC_ERROR);

  int32_t idx = -1;
  size_t i = 1;

  while (i < select.size()) {
    if (select[i] != 0U) {
      const auto s = select[i];
      // assert only one bit is set.
      SERVING_ENFORCE((s & (s - 1)) == 0, errors::ErrorCode::LOGIC_ERROR,
                      "i {}, s {}", i, s);
      idx = (i - 1) * 8 + std::round(std::log2(s));
      i++;
      break;
    }
    i++;
  }

  while (i < select.size()) {
    // assert only one bit is set.
    SERVING_ENFORCE(select[i++] == 0, errors::ErrorCode::LOGIC_ERROR);
  }

  SERVING_ENFORCE(idx != -1, errors::ErrorCode::LOGIC_ERROR);

  return idx;
}

bool TreePredictSelect::CheckLeafSelected(uint32_t leaf_idx) const {
  SERVING_ENFORCE_LT(leaf_idx, Leafs());
  size_t vec_idx = (leaf_idx / 8) + 1;
  uint8_t bit_mask = 1 << (leaf_idx % 8);

  return (select[vec_idx] & bit_mask) != 0;
}

std::vector<uint64_t> TreePredictSelect::ToU64Vec() const {
  std::vector<uint64_t> u64s;
  u64s.emplace_back(select.size());

  // every eight `uint8_t` elements are merged into one `uint64_t` element
  for (size_t i = 0; i < select.size(); i += 8) {
    uint64_t combined = 0;
    for (size_t j = 0; j < 8 && (i + j) < select.size(); ++j) {
      combined |= static_cast<uint64_t>(select[i + j]) << (j * 8);
    }
    u64s.emplace_back(combined);
  }
  return u64s;
}

size_t TreePredictSelect::GetSelectsU64VecSize(size_t leaf_num) {
  // extra one for select uint8 vec size
  return (leaf_num / 64) + 2;
}

void BuildTreeFromNode(const NodeDef& node_def, const OpDef& op_def,
                       const std::vector<std::string>& feature_list,
                       Tree* tree) {
  // build tree nodes
  // root node id
  tree->root_id = GetNodeAttr<int32_t>(node_def, op_def, "root_node_id");
  auto node_ids = GetNodeAttr<std::vector<int32_t>>(node_def, "node_ids");
  CheckAttrValueDuplicate(node_ids, "node_ids");
  auto lchild_ids = GetNodeAttr<std::vector<int32_t>>(node_def, "lchild_ids");
  CheckAttrValueDuplicate(lchild_ids, "lchild_ids", -1);
  auto rchild_ids = GetNodeAttr<std::vector<int32_t>>(node_def, "rchild_ids");
  CheckAttrValueDuplicate(rchild_ids, "rchild_ids", -1);
  auto leaf_node_ids =
      GetNodeAttr<std::vector<int32_t>>(node_def, "leaf_node_ids");
  auto split_feature_idx_list =
      GetNodeAttr<std::vector<int32_t>>(node_def, "split_feature_idxs");
  auto split_values =
      GetNodeAttr<std::vector<double>>(node_def, "split_values");
  SERVING_ENFORCE(node_ids.size() == lchild_ids.size() &&
                      node_ids.size() == rchild_ids.size() &&
                      node_ids.size() == split_feature_idx_list.size() &&
                      node_ids.size() == split_values.size(),
                  errors::ErrorCode::LOGIC_ERROR,
                  "The length of attr value `node_ids` `lchild_ids` "
                  "`rchild_ids`"
                  "`split_feature_idxs` `split_values` "
                  "should be same.");

  std::for_each(split_feature_idx_list.begin(), split_feature_idx_list.end(),
                [&](const auto& idx) {
                  if (idx >= 0) {
                    SERVING_ENFORCE_LT(static_cast<size_t>(idx),
                                       feature_list.size());
                    tree->used_feature_idx_list.emplace(idx);
                  }
                });

  tree->num_leaf = leaf_node_ids.size();
  std::map<int32_t, int32_t> leaf_idx_map;
  for (size_t idx = 0; idx < leaf_node_ids.size(); ++idx) {
    leaf_idx_map.emplace(leaf_node_ids[idx], idx);
  }

  for (size_t i = 0; i < node_ids.size(); ++i) {
    auto leaf_it = leaf_idx_map.find(node_ids[i]);
    TreeNode node{
        .id = node_ids[i],
        .lchild_id = lchild_ids[i],
        .rchild_id = rchild_ids[i],
        .is_leaf = leaf_it != leaf_idx_map.end(),
        .split_feature_idx = split_feature_idx_list[i],
        .split_value = split_values[i],
        .leaf_index = leaf_it != leaf_idx_map.end() ? leaf_it->second : -1};
    tree->nodes.emplace(node_ids[i], node);
  }
}

std::vector<TreePredictSelect> TreePredict(
    const Tree& tree, const std::shared_ptr<arrow::RecordBatch>& input) {
  SERVING_ENFORCE_GT(tree.nodes.size(), 0U);

  std::map<size_t, std::shared_ptr<arrow::Array>> input_features;
  for (const auto& idx : tree.used_feature_idx_list) {
    const auto& col = input->column(idx);
    input_features.emplace(idx, CastToDoubleArray(input->column(idx)));
  }

  std::vector<TreePredictSelect> selects(input->num_rows());
  arrow::BinaryBuilder builder;
  for (int64_t row = 0; row < input->num_rows(); ++row) {
    auto& pred_select = selects[row];
    pred_select.SetLeafs(tree.num_leaf);
    std::queue<int32_t> node_ids;
    node_ids.push(tree.root_id);

    while (!node_ids.empty()) {
      const auto it = tree.nodes.find(node_ids.front());
      node_ids.pop();
      SERVING_ENFORCE(it != tree.nodes.end(), errors::ErrorCode::LOGIC_ERROR);
      const auto& node = it->second;
      if (node.is_leaf) {
        SERVING_ENFORCE(node.leaf_index != -1, errors::ErrorCode::LOGIC_ERROR);
        pred_select.SetLeafSelected(node.leaf_index);
      } else {
        if (node.split_feature_idx < 0) {
          // split feature not belong to this party, both side could be
          // possible
          node_ids.push(node.lchild_id);
          node_ids.push(node.rchild_id);
        } else {
          auto d_a = std::static_pointer_cast<arrow::DoubleArray>(
              input_features[node.split_feature_idx]);
          SERVING_ENFORCE(d_a, errors::ErrorCode::LOGIC_ERROR);
          if (d_a->Value(row) < node.split_value) {
            node_ids.push(node.lchild_id);
          } else {
            node_ids.push(node.rchild_id);
          }
        }
      }
    }
  }

  return selects;
}

}  // namespace secretflow::serving::op
