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
#include "oneflow/core/job/session_global_objects_scope.h"
#include "oneflow/core/job/resource_desc.h"
#include "oneflow/core/job/global_for.h"
#include "oneflow/core/control/ctrl_server.h"
#include "oneflow/core/control/global_process_ctx.h"
#include "oneflow/core/job/available_memory_desc.pb.h"
#include "oneflow/core/job/id_manager.h"
#include "oneflow/core/job/profiler.h"
#include "oneflow/core/job/job_instance.h"
#include "oneflow/core/job/inter_user_job_info.pb.h"
#include "oneflow/core/job/job_desc.h"
#include "oneflow/core/job/critical_section_desc.h"
#include "oneflow/core/job/job_build_and_infer_ctx_mgr.h"
#include "oneflow/core/job/job_set_compile_ctx.h"
#include "oneflow/core/job/runtime_buffer_managers_scope.h"
#include "oneflow/core/framework/load_library.h"
#include "oneflow/core/job/version.h"
#include "oneflow/core/device/node_device_descriptor_manager.h"

#ifdef WITH_CUDA
#include "oneflow/core/device/cuda_device_descriptor.h"
#endif  // WITH_CUDA

#ifdef WITH_ROCM
#include "oneflow/core/device/rocm_device_descriptor.h"
#endif  // WITH_ROCM

namespace oneflow {

namespace {

AvailableMemDescOfMachine GetAvailableMemDescOfMachine(int64_t rank) {
  AvailableMemDescOfMachine machine_mem_desc;
  const auto node_desc =
      Global<device::NodeDeviceDescriptorManager>::Get()->GetNodeDeviceDescriptor(rank);
#ifdef WITH_CUDA
  const auto cuda_device_list =
      node_desc->GetDeviceDescriptorList(device::kCudaDeviceDescriptorClassName);
  CHECK(cuda_device_list);
  FOR_RANGE(int, i, 0, (Global<ResourceDesc, ForSession>::Get()->GpuDeviceNum())) {
    if (i >= cuda_device_list->DeviceCount()) {
      LOG(WARNING) << "Invalid CUDA device ordinal: rank " << rank << " ordinal " << i;
      machine_mem_desc.add_zone_size(0);
    } else {
      const auto cuda_device = std::dynamic_pointer_cast<const device::CudaDeviceDescriptor>(
          cuda_device_list->GetDevice(i));
      CHECK(cuda_device);
      machine_mem_desc.add_zone_size(cuda_device->GlobalMemorySizeBytes());
    }
  }
#endif
#ifdef WITH_ROCM
  const auto rocm_device_list =
      node_desc->GetDeviceDescriptorList(device::kRocmDeviceDescriptorClassName);
  CHECK(rocm_device_list);
  FOR_RANGE(int, i, 0, (Global<ResourceDesc, ForSession>::Get()->GpuDeviceNum())) {
    if (i >= rocm_device_list->DeviceCount()) {
      LOG(WARNING) << "Invalid ROCM device ordinal: rank " << rank << " ordinal " << i;
      machine_mem_desc.add_zone_size(0);
    } else {
      const auto rocm_device = std::dynamic_pointer_cast<const device::RocmDeviceDescriptor>(
          rocm_device_list->GetDevice(i));
      CHECK(rocm_device);
      machine_mem_desc.add_zone_size(rocm_device->GlobalMemorySizeBytes());
    }
  }
#endif
  machine_mem_desc.add_zone_size(node_desc->HostMemorySizeBytes());
  return machine_mem_desc;
}

AvailableMemDesc GetAvailableMemDesc() {
  AvailableMemDesc ret;
  for (int64_t i : Global<ResourceDesc, ForSession>::Get()->process_ranks()) {
    *ret.add_machine_amd() = GetAvailableMemDescOfMachine(i);
  }
  return ret;
}

AvailableMemDesc GetDryRunAvailableMemDesc() {
  AvailableMemDescOfMachine this_machine_mem_desc;
#if defined(WITH_CUDA) || defined(WITH_ROCM)
  FOR_RANGE(int, i, 0, (Global<ResourceDesc, ForSession>::Get()->GpuDeviceNum())) {
    this_machine_mem_desc.add_zone_size(std::numeric_limits<size_t>::max());
  }
#endif
  this_machine_mem_desc.add_zone_size(std::numeric_limits<size_t>::max());

  AvailableMemDesc ret;
  AvailableMemDescOfMachine machine_amd_i;
  for (int64_t i : Global<ResourceDesc, ForSession>::Get()->process_ranks()) {
    *ret.add_machine_amd() = this_machine_mem_desc;
  }
  return ret;
}

}  // namespace

SessionGlobalObjectsScope::SessionGlobalObjectsScope() {}

Maybe<void> SessionGlobalObjectsScope::Init(const ConfigProto& config_proto) {
  session_id_ = config_proto.session_id();
  Global<ResourceDesc, ForSession>::Delete();
  DumpVersionInfo();
  Global<ResourceDesc, ForSession>::New(config_proto.resource(),
                                        GlobalProcessCtx::NumOfProcessPerNode());
  Global<const ProfilerConf>::New(config_proto.profiler_conf());
  Global<IDMgr>::New();
  if (GlobalProcessCtx::IsThisProcessMaster()
      && Global<const ProfilerConf>::Get()->collect_act_event()) {
    Global<Profiler>::New();
  }
  if (GlobalProcessCtx::IsThisProcessMaster()) {
    Global<AvailableMemDesc>::New();
    if (Global<ResourceDesc, ForSession>::Get()->enable_dry_run()) {
      *Global<AvailableMemDesc>::Get() = GetDryRunAvailableMemDesc();
    } else {
      *Global<AvailableMemDesc>::Get() = GetAvailableMemDesc();
    }
    Global<JobName2JobId>::New();
    Global<CriticalSectionDesc>::New();
    Global<InterUserJobInfo>::New();
    Global<LazyJobBuildAndInferCtxMgr>::New();
    Global<JobSetCompileCtx>::New();
    Global<RuntimeBufferManagersScope>::New();
  }
  for (const std::string& lib_path : config_proto.load_lib_path()) { JUST(LoadLibrary(lib_path)); }
  return Maybe<void>::Ok();
}

Maybe<void> SessionGlobalObjectsScope::EagerInit(const ConfigProto& config_proto) {
  session_id_ = config_proto.session_id();
  Global<ResourceDesc, ForSession>::Delete();
  DumpVersionInfo();
  Global<ResourceDesc, ForSession>::New(config_proto.resource());
  Global<const ProfilerConf>::New(config_proto.profiler_conf());
  if (GlobalProcessCtx::IsThisProcessMaster()
      && Global<const ProfilerConf>::Get()->collect_act_event()) {
    Global<Profiler>::New();
  }
  for (const std::string lib_path : config_proto.load_lib_path()) { JUST(LoadLibrary(lib_path)); }
  return Maybe<void>::Ok();
}

SessionGlobalObjectsScope::~SessionGlobalObjectsScope() {
  if (GlobalProcessCtx::IsThisProcessMaster()) {
    Global<RuntimeBufferManagersScope>::Delete();
    Global<JobSetCompileCtx>::Delete();
    Global<LazyJobBuildAndInferCtxMgr>::Delete();
    Global<InterUserJobInfo>::Delete();
    Global<CriticalSectionDesc>::Delete();
    Global<JobName2JobId>::Delete();
    Global<AvailableMemDesc>::Delete();
  }
  if (Global<Profiler>::Get() != nullptr) { Global<Profiler>::Delete(); }
  Global<IDMgr>::Delete();
  Global<const ProfilerConf>::Delete();
  Global<ResourceDesc, ForSession>::Delete();
  Global<ResourceDesc, ForSession>::New(Global<ResourceDesc, ForEnv>::Get()->resource(),
                                        GlobalProcessCtx::NumOfProcessPerNode());
}

}  // namespace oneflow
