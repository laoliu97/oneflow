#include "oneflow/core/operator/momentum_model_update_op.h"

namespace oneflow {

void MomentumModelUpdateOp::MdUpdtVirtualInitFromOpConf() {
  if (GlobalJobDesc().other_conf().predict_conf().has_tmp_split_fw_bw_train_conf()) {
    EnrollInputBn("momentum", false)->set_is_mutable(true);
  } else {
    UNIMPLEMENTED();
  }
}

void MomentumModelUpdateOp::MdUpdtVirtualInferBlobDescs(
    std::function<BlobDesc*(const std::string&)> GetBlobDesc4BnInOp,
    const ParallelContext* parallel_ctx) const {
  const BlobDesc* model_blob_desc = GetBlobDesc4BnInOp("model");
  CHECK_EQ(model_blob_desc->data_type(), GlobalJobDesc().DefaultDataType());
  CHECK_EQ(model_blob_desc->has_data_id_field(), false);
  if (GlobalJobDesc().other_conf().predict_conf().has_tmp_split_fw_bw_train_conf()) {
    CHECK(*GetBlobDesc4BnInOp("momentum") == *model_blob_desc);
  } else {
    UNIMPLEMENTED();
  }
}

const PbMessage& MomentumModelUpdateOp::GetCustomizedConf() const {
  if (GlobalJobDesc().other_conf().predict_conf().has_tmp_split_fw_bw_train_conf()) {
    return op_conf().momentum_model_update_conf();
  } else {
    UNIMPLEMENTED();
  }
}

const HashSet<std::string> MomentumModelUpdateOp::AlwaysBroadcastParallelBns() const {
  return HashSet<std::string>{};
}

REGISTER_CLASS(NormalModelUpdateOpUserConf::kMomentumConf, NormalModelUpdtOp,
               MomentumModelUpdateOp);

REGISTER_OP(OperatorConf::kMomentumModelUpdateConf, MomentumModelUpdateOp);

}  // namespace oneflow
