/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#ifndef ONEFLOW_CORE_COMMON_CONTAINER_UTIL_H_
#define ONEFLOW_CORE_COMMON_CONTAINER_UTIL_H_

#include "oneflow/core/common/type_traits.h"
#include "oneflow/core/common/maybe.h"

namespace oneflow {

template<typename Key, typename T, typename Hash = std::hash<Key>>
using HashMap = std::unordered_map<Key, T, Hash>;

template<typename Key, typename Hash = std::hash<Key>>
using HashSet = std::unordered_set<Key, Hash>;

template<typename MapT, typename KeyT>
Maybe<scalar_or_const_ref_t<typename MapT::mapped_type>> MapAt(const MapT& map, const KeyT& key) {
  const auto& iter = map.find(key);
  CHECK_OR_RETURN(iter != map.end());
  return iter->second;
}

template<typename VecT>
Maybe<scalar_or_const_ref_t<typename VecT::value_type>> VectorAt(const VecT& vec, int64_t index) {
  CHECK_GE_OR_RETURN(index, 0);
  CHECK_LT_OR_RETURN(index, vec.size());
  return vec.at(index);
}

}  // namespace oneflow

#endif  // ONEFLOW_CORE_COMMON_CONTAINER_UTIL_H_
