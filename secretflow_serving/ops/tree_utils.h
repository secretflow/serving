#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#include "secretflow_serving/core/exception.h"

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

struct TreePredictSelect {
  // | counts of padding bits in last bit map | 0-7 bit map | 8-15 bit map | ...
  std::vector<uint8_t> select;

  TreePredictSelect() = default;

  explicit TreePredictSelect(const std::string& str) {
    select.resize(str.size());
    std::memcpy(select.data(), str.data(), str.size());
  }

  explicit TreePredictSelect(std::string_view str) {
    select.resize(str.size());
    std::memcpy(select.data(), str.data(), str.size());
  }

  void SetSelectBuf(const std::string& str) {
    select.resize(str.size());
    std::memcpy(select.data(), str.data(), str.size());
  }

  void SetSelectBuf(std::string_view str) {
    select.resize(str.size());
    std::memcpy(select.data(), str.data(), str.size());
  }

  void SetLeafs(size_t leafs, bool all_selected = false) {
    select.resize(std::ceil(leafs / 8.0) + 1, 0);
    uint8_t pad_bits = leafs % 8 ? 8 - leafs % 8 : 0;
    select[0] = pad_bits;
    if (all_selected) {
      for (size_t i = 1; i < select.size(); ++i) {
        select[i] = std::numeric_limits<uint8_t>::max();
      }
    }
  }

  size_t Leafs() const {
    if (select.empty()) {
      return 0;
    }
    return (select.size() - 1) * 8 - select[0];
  }

  void Merge(const TreePredictSelect& s) {
    SERVING_ENFORCE(Leafs(), errors::ErrorCode::LOGIC_ERROR);
    SERVING_ENFORCE_EQ(Leafs(), s.Leafs());
    for (size_t i = 1; i < s.select.size(); i++) {
      select[i] &= s.select[i];
    }
  }

  void SetLeafSelected(uint32_t leaf_idx) {
    SERVING_ENFORCE_LT(leaf_idx, Leafs());
    size_t vec_idx = leaf_idx / 8 + 1;
    uint8_t bit_mask = 1 << (leaf_idx % 8);
    select[vec_idx] |= bit_mask;
  }

  int32_t GetLeafIndex() const {
    SERVING_ENFORCE(Leafs(), errors::ErrorCode::LOGIC_ERROR);

    int32_t idx = -1;
    size_t i = 1;

    while (i < select.size()) {
      if (select[i]) {
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
};

}  // namespace secretflow::serving::op
