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
#include "oneflow/core/framework/framework.h"
#include "oneflow/user/image/image_util.h"
#include "oneflow/core/framework/op_generated.h"

namespace oneflow {

/* static */ Maybe<void> ImageResizeToFixedOp::InferLogicalTensorDesc(user_op::InferContext* ctx) {
  const user_op::TensorDesc& in_tensor = ctx->InputTensorDesc("in", 0);
  CHECK_OR_RETURN(in_tensor.shape().NumAxes() == 1 && in_tensor.shape().elem_cnt() > 0);
  int64_t batch_size = in_tensor.shape().elem_cnt();
  int64_t target_width = ctx->Attr<int64_t>("target_width");
  int64_t target_height = ctx->Attr<int64_t>("target_height");
  int64_t channels = ctx->Attr<int64_t>("channels");

  user_op::TensorDesc* out_tensor = ctx->OutputTensorDesc("out", 0);
  *out_tensor->mut_shape() = Shape({batch_size, target_height, target_width, channels});
  out_tensor->set_is_dynamic(in_tensor.is_dynamic());

  user_op::TensorDesc* scale_tensor = ctx->OutputTensorDesc("scale", 0);
  *scale_tensor->mut_shape() = Shape({batch_size, 2});
  scale_tensor->set_is_dynamic(in_tensor.is_dynamic());

  return Maybe<void>::Ok();
}

/*static*/ Maybe<void> ImageResizeToFixedOp::InferPhysicalTensorDesc(user_op::InferContext* ctx) {
  return InferLogicalTensorDesc(ctx);
}

/* static */ Maybe<void> ImageResizeToFixedOp::GetSbp(user_op::SbpContext* ctx) {
  ctx->NewBuilder().Split(ctx->inputs(), 0).Split(ctx->outputs(), 0).Build();
  return Maybe<void>::Ok();
}

/* static */ Maybe<void> ImageResizeToFixedOp::CheckAttr(const user_op::UserOpDefWrapper& def,
                                                         const user_op::UserOpConfWrapper& conf) {
  bool check_failed = false;
  std::ostringstream err;
  err << "Illegal attr value for " << conf.op_type_name() << " op, op_name: " << conf.op_name();
  int64_t target_width = conf.attr<int64_t>("target_width");
  int64_t target_height = conf.attr<int64_t>("target_height");
  if (target_width <= 0 || target_height <= 0) {
    err << ", target_width: " << target_width << ", target_height: " << target_height;
    check_failed = true;
  }
  int64_t channels = conf.attr<int64_t>("channels");
  if (channels != 1 && channels != 3) {
    err << ", channels: " << channels << " (channels can only be 1 or 3)";
    check_failed = true;
  }
  DataType data_type = conf.attr<DataType>("data_type");
  if (data_type != DataType::kUInt8 && data_type != DataType::kFloat) {
    err << ", data_type: " << data_type << " (only support kUInt8 and kFloat for now)";
    check_failed = true;
  }
  const std::string& interp_type = conf.attr<std::string>("interpolation_type");
  if (!CheckInterpolationValid(interp_type, err)) { check_failed = true; }
  if (check_failed) { return oneflow::Error::CheckFailedError() << err.str(); }
  return Maybe<void>::Ok();
}

/* static */ Maybe<void> ImageResizeToFixedOp::InferDataType(user_op::InferContext* ctx) {
  const user_op::TensorDesc& in_tensor = ctx->InputTensorDesc("in", 0);
  CHECK_OR_RETURN(in_tensor.data_type() == DataType::kTensorBuffer);
  user_op::TensorDesc* out_tensor = ctx->OutputTensorDesc("out", 0);
  *out_tensor->mut_data_type() = ctx->Attr<DataType>("data_type");
  user_op::TensorDesc* scale_tensor = ctx->OutputTensorDesc("scale", 0);
  *scale_tensor->mut_data_type() = DataType::kFloat;
  return Maybe<void>::Ok();
}

/* static */ Maybe<void> ImageResizeKeepAspectRatioOp::InferLogicalTensorDesc(
    user_op::InferContext* ctx) {
  const user_op::TensorDesc& in_desc = ctx->InputTensorDesc("in", 0);
  CHECK_OR_RETURN(in_desc.shape().NumAxes() == 1 && in_desc.shape().At(0) > 0);
  user_op::TensorDesc* out_desc = ctx->OutputTensorDesc("out", 0);
  *out_desc->mut_shape() = in_desc.shape();
  user_op::TensorDesc* size_desc = ctx->OutputTensorDesc("size", 0);
  *size_desc->mut_shape() = in_desc.shape();
  user_op::TensorDesc* scale_desc = ctx->OutputTensorDesc("scale", 0);
  *scale_desc->mut_shape() = in_desc.shape();
  return Maybe<void>::Ok();
}

/*static*/ Maybe<void> ImageResizeKeepAspectRatioOp::InferPhysicalTensorDesc(
    user_op::InferContext* ctx) {
  return InferLogicalTensorDesc(ctx);
}

/* static */ Maybe<void> ImageResizeKeepAspectRatioOp::GetSbp(user_op::SbpContext* ctx) {
  ctx->NewBuilder().Split(ctx->inputs(), 0).Split(ctx->outputs(), 0).Build();
  return Maybe<void>::Ok();
}

/* static */ Maybe<void> ImageResizeKeepAspectRatioOp::CheckAttr(
    const user_op::UserOpDefWrapper& def, const user_op::UserOpConfWrapper& conf) {
  bool check_failed = false;
  std::ostringstream err;
  err << "Illegal attr value for " << conf.op_type_name() << " op, op_name: " << conf.op_name();
  const int32_t target_size = conf.attr<int32_t>("target_size");
  const int32_t max_size = conf.attr<int32_t>("max_size");
  if (target_size <= 0) {
    err << ", target_size: " << target_size << " (target_size must be greater than 0)";
    check_failed = true;
  }
  if (max_size < target_size && max_size > 0) {
    err << ", max_size: " << max_size
        << " (max_size must be greater than target_size or equal to 0)";
    check_failed = true;
  }
  const std::string& interp_type = conf.attr<std::string>("interpolation_type");
  if (!CheckInterpolationValid(interp_type, err)) { check_failed = true; }
  if (check_failed) { return oneflow::Error::CheckFailedError() << err.str(); }
  return Maybe<void>::Ok();
}

/* static */ Maybe<void> ImageResizeKeepAspectRatioOp::InferDataType(user_op::InferContext* ctx) {
  const user_op::TensorDesc& in_desc = ctx->InputTensorDesc("in", 0);
  CHECK_OR_RETURN(in_desc.data_type() == DataType::kTensorBuffer);
  user_op::TensorDesc* out_desc = ctx->OutputTensorDesc("out", 0);
  *out_desc->mut_data_type() = DataType::kTensorBuffer;
  user_op::TensorDesc* size_desc = ctx->OutputTensorDesc("size", 0);
  *size_desc->mut_data_type() = DataType::kTensorBuffer;
  user_op::TensorDesc* scale_desc = ctx->OutputTensorDesc("scale", 0);
  *scale_desc->mut_data_type() = DataType::kTensorBuffer;
  return Maybe<void>::Ok();
}

}  // namespace oneflow
