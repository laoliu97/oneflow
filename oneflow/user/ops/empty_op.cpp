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
#include "oneflow/core/common/maybe.h"
#include "oneflow/core/framework/framework.h"
#include "oneflow/core/framework/op_generated.h"
#include "oneflow/core/job/nd_sbp_util.h"
#include "oneflow/core/framework/device.h"
#include "oneflow/core/framework/stream.h"

namespace oneflow {

namespace {

Maybe<Symbol<Stream>> MakeEmptyStream(const Symbol<Device>& out_device, const bool pin_memory) {
  if (pin_memory) {
    CHECK_OR_RETURN(out_device->type() == "cpu")
        << "empty op only support pin_memory in cpu device but got " << out_device->type();
    // TODO:(zhaoluyang) Parsing pin-memory-device from python
    auto pin_device = JUST(Device::New("cuda"));
    return Stream::New(pin_device, StreamRole::kPinnedCompute);
  }
  return Stream::New(out_device, StreamRole::kCompute);
}

}  // namespace

/* static */ Maybe<void> EmptyOp::InferLogicalTensorDesc(user_op::InferContext* ctx) {
  *ctx->OutputShape("out", 0) = Shape(ctx->Attr<Shape>("shape").dim_vec());
  *ctx->OutputStride("out", 0) = Stride(Shape(ctx->Attr<Shape>("shape").dim_vec()));
  return Maybe<void>::Ok();
}

/* static */ Maybe<void> EmptyOp::InferPhysicalTensorDesc(user_op::InferContext* ctx) {
  const Shape& parallel_hierarchy = *ctx->parallel_desc().hierarchy();
  const NdSbp& nd_sbp = ctx->NdSbp4ArgNameAndIndex("out", 0);
  const Shape& logical_shape = ctx->Attr<Shape>("shape");
  const int64_t parallel_id = ctx->parallel_ctx().parallel_id();
  const auto tensor_slice_view =
      GetTensorSliceView4ParallelId(parallel_hierarchy, nd_sbp, logical_shape, parallel_id);
  const Shape& physical_shape = tensor_slice_view.shape();

  *ctx->OutputShape("out", 0) = physical_shape;
  *ctx->OutputStride("out", 0) = Stride(physical_shape);
  return Maybe<void>::Ok();
}

/* static */ Maybe<void> EmptyOp::GetSbp(user_op::SbpContext* ctx) { return Maybe<void>::Ok(); }

/* static */ Maybe<void> EmptyOp::InferNdSbp(user_op::InferNdSbpFnContext* ctx) {
  SbpParallel default_sbp;
  default_sbp.mutable_broadcast_parallel();
  return user_op::InferNdSbp4SrcOp(ctx, default_sbp);
}

/* static */ Maybe<void> EmptyOp::InferDataType(user_op::InferContext* ctx) {
  *ctx->OutputDType("out", 0) = ctx->Attr<DataType>("dtype");
  return Maybe<void>::Ok();
}

/* static */ Maybe<Symbol<Stream>> EmptyOp::InferDeviceAndStream(
    user_op::DeviceAndStreamInferContext* ctx) {
  Symbol<Device> out_device =
      JUST(Device::New(ctx->Attr<std::string>("device_type"), ctx->Attr<int64_t>("device_id")));
  *ctx->OutputTensorDevice4ArgNameAndIndex("out", 0) = out_device;
  const bool pin_memory = ctx->Attr<bool>("pin_memory");
  return MakeEmptyStream(out_device, pin_memory);
}

}  // namespace oneflow
