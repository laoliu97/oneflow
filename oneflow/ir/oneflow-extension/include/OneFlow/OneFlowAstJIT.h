#ifndef ONEFLOW_IR_ONEFLOW_EXTENSION_INCLUDE_ONEFLOW_AST_JIT_H_
#define ONEFLOW_IR_ONEFLOW_EXTENSION_INCLUDE_ONEFLOW_AST_JIT_H_

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <iostream>
#include <string>

#include "oneflow/core/common/just.h"
#include "oneflow/core/common/singleton.h"
#include "oneflow/core/common/util.h"
#include "oneflow/ir/oneflow-extension/include/OneFlow/py_ast.h"

class JIT_Engine;

class LRJITRegistry final {
 public:
  OF_DISALLOW_COPY_AND_MOVE(LRJITRegistry);
  ~LRJITRegistry() = default;

  void Register(const std::string& function_id, pyast::FunctionDef& ast);
  std::shared_ptr<JIT_Engine> LookUp(const std::string& function_id);
  double Invoke(std::shared_ptr<JIT_Engine> engine, double base_lr, double step);

 private:
  friend class oneflow::Singleton<LRJITRegistry>;
  LRJITRegistry() = default;
  std::unordered_map<std::string, std::shared_ptr<JIT_Engine>> function_id2engine_;
};

#endif  // ONEFLOW_IR_ONEFLOW_EXTENSION_INCLUDE_ONEFLOW_AST_JIT_H_
