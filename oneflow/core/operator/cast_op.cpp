#include "oneflow/core/operator/cast_op.h"
#include "oneflow/core/job/sbp_signature_builder.h"

namespace oneflow {

void CastOp::InitFromOpConf() {
  CHECK(op_conf().has_cast_conf());
  EnrollInputBn("in");
  EnrollOutputBn("out");
}

const PbMessage& CastOp::GetCustomizedConf() const { return op_conf().cast_conf(); }

void CastOp::InferBlobDescs(std::function<BlobDesc*(const std::string&)> GetBlobDesc4BnInOp,
                            const ParallelContext* parallel_ctx) const {
  BlobDesc* out_blob_desc = GetBlobDesc4BnInOp("out");
  BlobDesc* in_blob_desc = GetBlobDesc4BnInOp("in");
  *out_blob_desc = *in_blob_desc;
  out_blob_desc->set_data_type(op_conf().cast_conf().data_type());
}

void CastOp::FixSbpSignature(
    const std::function<const SbpInferHint&(const std::string&)>& SbpInferHint4Ibn,
    SbpSignature* sbp_signature) const {
  auto* bn2sbp = sbp_signature->mutable_bn_in_op2sbp_parallel();
  if (bn2sbp->at("out").has_partial_sum_parallel()  // TODO: data_type is float16
      && GlobalJobDesc().all_reduce_fp16()) {
    bn2sbp->at("in").mutable_broadcast_parallel();
    bn2sbp->at("out").mutable_broadcast_parallel();
  }
}

void CastOp::GetSbpSignatures(
    const std::function<const BlobDesc&(const std::string&)>& LogicalBlobDesc4Ibn,
    SbpSignatureList* sbp_sig_list) const {
  SbpSignatureBuilder()
      .Split(input_bns(), 0)
      .Split(output_bns(), 0)
      .MakeSplitSignatureListBuilder(LogicalBlobDesc4Ibn("in").shape().NumAxes())
      .Build(sbp_sig_list);
  SbpSignatureBuilder()
      .PartialSum(input_bns())
      .PartialSum(output_bns())
      .Build(sbp_sig_list->mutable_sbp_signature()->Add());
}

REGISTER_OP(OperatorConf::kCastConf, CastOp);

}  // namespace oneflow
