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
#include "oneflow/core/common/data_type.h"
#include "oneflow/core/common/preprocessor.h"
#include "oneflow/core/framework/framework.h"
#include "oneflow/core/framework/dtype.h"
#include "oneflow/user/kernels/distributions/uniform_int_distribution.h"
#include "oneflow/core/ep/include/device.h"
#include "oneflow/core/ep/cuda/cuda_stream.h"

namespace oneflow {

namespace {

__device__ int64_t GenUniformInt(curandState* state, const int64_t low, const int64_t high) {
  auto rand_num = curand_uniform(state);
  // curand_uniform generates (0.0, 1.0], but we want [0.0, 1.0) here
  if (rand_num == 1.0) { rand_num = 0.0; }
  return static_cast<int64_t>(rand_num * (high - low) + low);
}

template<typename T>
__global__ void GenerateGpu(curandState* state, const int64_t elem_cnt, T* dptr, const int64_t low,
                            const int64_t high) {
  const int id = blockIdx.x * blockDim.x + threadIdx.x;
  curandState localState = state[id];
  CUDA_1D_KERNEL_LOOP(i, elem_cnt) {
    dptr[i] = static_cast<T>(GenUniformInt(&localState, low, high));
  }
  state[id] = localState;
}

}  // namespace

template<typename T>
void UniformIntDistribution<DeviceType::kCUDA, T>::operator()(
    ep::Stream* stream, const int64_t elem_cnt, T* dptr,
    const std::shared_ptr<one::Generator>& generator) const {
  CHECK_GE(elem_cnt, 0);
  const auto device_index = stream->device()->device_index();
  auto gen = CHECK_JUST(generator->Get<one::CUDAGeneratorImpl>(device_index));
  int32_t block_num = gen->max_block_num();
  int32_t thread_num = gen->max_thread_num();
  auto* curand_states = gen->curand_states();
  GenerateGpu<T><<<block_num, thread_num, 0, stream->As<ep::CudaStream>()->cuda_stream()>>>(
      curand_states, elem_cnt, dptr, low_, high_);
}

#define INITIATE_CUDA_UNIFORM_INT_DISTRIBUTION(T, typeproto)              \
  template void UniformIntDistribution<DeviceType::kCUDA, T>::operator()( \
      ep::Stream* stream, const int64_t elem_cnt, T* dptr,                \
      const std::shared_ptr<one::Generator>& generator) const;

OF_PP_FOR_EACH_TUPLE(INITIATE_CUDA_UNIFORM_INT_DISTRIBUTION, FLOATING_DATA_TYPE_SEQ)
OF_PP_FOR_EACH_TUPLE(INITIATE_CUDA_UNIFORM_INT_DISTRIBUTION, INT_DATA_TYPE_SEQ)
OF_PP_FOR_EACH_TUPLE(INITIATE_CUDA_UNIFORM_INT_DISTRIBUTION, UNSIGNED_INT_DATA_TYPE_SEQ)

}  // namespace oneflow
