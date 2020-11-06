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
#ifndef ONEFLOW_CORE_VM_H_
#define ONEFLOW_CORE_VM_H_

#include "oneflow/core/common/maybe.h"
#include "oneflow/core/object_msg/object_msg.h"

namespace oneflow {
namespace vm {
namespace cfg {
class InstructionListProto;
}

class InstructionMsg;

ObjectMsgPtr<InstructionMsg> NewInstruction(const std::string& instr_type_name);

Maybe<void> Run(const std::string& instruction_list_proto_str);

Maybe<void> Run(const cfg::InstructionListProto& cfg_instruction_list_proto);

}  // namespace vm
}  // namespace oneflow

#endif  // ONEFLOW_CORE_VM_H_
