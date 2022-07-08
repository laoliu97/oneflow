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
#include "oneflow/ir/oneflow-extension/include/PyAst/AstMlirGen.h"

// declare any scope variables in the front of function block to ensure the enough lifetime.
mlir::LogicalResult BuilderWithSymbolTable::declare(const std::string& var, mlir::Value value) {
  auto iter = symbolTable.find(var);
  if (iter != symbolTable.end()) {
    builder.create<mlir::memref::StoreOp>(loc(), value, iter->second);
    return mlir::failure();
  }

  auto history_block = builder.getInsertionBlock();
  auto history_point = builder.getInsertionPoint();

  builder.setInsertionPointToStart(symbolTableForDeclareBlock);

  auto single_type = mlir::Float32Type::getF32(builder.getContext());
  auto type = mlir::MemRefType::get({}, single_type);
  auto key = builder.create<mlir::memref::AllocOp>(loc(), type);

  builder.setInsertionPoint(history_block, history_point);
  builder.create<mlir::memref::StoreOp>(loc(), value, key);
  symbolTable[var] = key;
  return mlir::success();
}

// look up memref of the special symbol with variable name
mlir::Value BuilderWithSymbolTable::lookup(const std::string& var) {
  if (symbolTable.count(var) == 1) { return symbolTable[var]; }
  theModule->emitError("error: unknown variable '" + var + "'");
  return nullptr;
}

// generate a location of mlir for ops
mlir::Location BuilderWithSymbolTable::loc(const std::string& file_name, int line, int col) {
  return mlir::FileLineColLoc::get(builder.getStringAttr(file_name), line, col);
}

// dump the current whole module up
void BuilderWithSymbolTable::dump() { theModule.dump(); }

// generate a module op for lr jit registry from a ast
mlir::ModuleOp MLIRGenImpl::genModule(pyast::FunctionDef* func) {
  theModule = mlir::ModuleOp::create(loc());

  if (failed(verify(theModule))) {
    theModule.emitError("module verification error");
    return nullptr;
  }

  builder.setInsertionPointToEnd(theModule.getBody());

  auto args = func->get_args()->get_args();
  auto type = mlir::Float32Type::getF32(builder.getContext());
  llvm::SmallVector<mlir::Type> arg_types(args.size(), type);
  llvm::SmallVector<mlir::Type> res_types(1, type);

  auto func_type = builder.getFunctionType(arg_types, res_types);
  auto function = mlir::func::FuncOp::create(loc(), func->get_name(), func_type);

  auto* entry_block = function.addEntryBlock();
  symbolTableForDeclareBlock = entry_block;
  theModule.push_back(function);
  builder.setInsertionPointToStart(entry_block);

  for (const auto nameValue : llvm::zip(args, entry_block->getArguments())) {
    if (failed(declare(std::get<0>(nameValue)->get_arg(), std::get<1>(nameValue)))) {
      return nullptr;
    }
  }

  builder.setInsertionPointToStart(entry_block);
  for (auto& stmt : func->get_body()) { mlirGen(stmt.get()); }

  return theModule;
}

// use llvm rtti to dispatch respective code gen tasks of stmt
void MLIRGenImpl::mlirGen(pyast::stmt* stmt) {
  llvm::TypeSwitch<pyast::stmt*>(stmt)
      .Case<pyast::Return, pyast::Assign, pyast::If>([&](auto* node) { mlirGen(node); })
      .Default([&](auto* node) { theModule->emitError("StmtKind not support yet"); });
}

// use llvm rtti to dispatch respective code gen tasks of expr
mlir::Value MLIRGenImpl::mlirGen(pyast::expr* expr) {
  mlir::Value res;
  llvm::TypeSwitch<pyast::expr*>(expr)
      .Case<pyast::BinOp, pyast::Compare, pyast::Call, pyast::Constant, pyast::Name>(
          [&](auto* node) { res = mlirGen(node); })
      .Default([&](auto* node) { theModule->emitError("ExprKind not support yet"); });
  return res;
}

void MLIRGenImpl::mlirGen(pyast::If* expr) {
  auto test = mlirGen(expr->get_test().get());

  if (test.getType().isF32()) {
    auto eq = mlir::arith::CmpFPredicate::ONE;
    auto zero_attr = builder.getF32FloatAttr(0);
    auto zero = builder.create<mlir::arith::ConstantOp>(loc(), zero_attr);
    test = builder.create<mlir::arith::CmpFOp>(loc(), eq, test, zero);
  }

  mlir::Block* then_block = builder.createBlock(builder.getBlock()->getParent());
  mlir::Block* else_block = builder.createBlock(builder.getBlock()->getParent());
  mlir::Block* after_block = builder.createBlock(builder.getBlock()->getParent());
  builder.setInsertionPointAfterValue(test);
  builder.create<mlir::cf::CondBranchOp>(loc(), test, then_block, llvm::None, else_block,
                                         llvm::None);

  builder.setInsertionPointToStart(then_block);
  for (auto& expr : expr->get_body()) { mlirGen(expr.get()); }
  if (then_block->empty() || !llvm::dyn_cast<mlir::func::ReturnOp>(then_block->back())) {
    builder.create<mlir::cf::BranchOp>(loc(), after_block);
  }

  builder.setInsertionPointToStart(else_block);
  for (auto& expr : expr->get_orelse()) { mlirGen(expr.get()); }
  if (else_block->empty() || !llvm::dyn_cast<mlir::func::ReturnOp>(else_block->back())) {
    builder.create<mlir::cf::BranchOp>(loc(), after_block);
  }

  builder.setInsertionPointToStart(after_block);
}

mlir::Value MLIRGenImpl::mlirGen(pyast::Compare* expr) {
  if (expr->get_comparators().size() != 1 || expr->get_ops().size() != 1) {
    theModule->emitError("compare only support once compare now");
  }

  mlir::arith::CmpFPredicate op = mlir::arith::CmpFPredicate::OEQ;
  switch (expr->get_ops()[0]) {
    case pyast::Compare::kEq: op = mlir::arith::CmpFPredicate::OEQ; break;
    case pyast::Compare::kNotEq: op = mlir::arith::CmpFPredicate::ONE; break;
    case pyast::Compare::kLt: op = mlir::arith::CmpFPredicate::OLT; break;
    case pyast::Compare::kLtE: op = mlir::arith::CmpFPredicate::OLE; break;
    case pyast::Compare::kGt: op = mlir::arith::CmpFPredicate::OGT; break;
    case pyast::Compare::kGtE: op = mlir::arith::CmpFPredicate::OGE; break;
    default: theModule->emitError("compare_ not support op now");
  }

  auto lhs = mlirGen(expr->get_left().get());
  auto rhs = mlirGen(expr->get_comparators()[0].get());
  auto res = builder.create<mlir::arith::CmpFOp>(loc(), op, lhs, rhs);
  return res;
}

mlir::Value MLIRGenImpl::mlirGen(pyast::BinOp* expr) {
  auto lhs = mlirGen(expr->get_left().get());
  auto rhs = mlirGen(expr->get_right().get());
  mlir::Value res;

  switch (expr->get_op()) {
    case pyast::BinOp::kAdd: res = builder.create<mlir::arith::AddFOp>(loc(), lhs, rhs); break;
    case pyast::BinOp::kSub: res = builder.create<mlir::arith::SubFOp>(loc(), lhs, rhs); break;
    case pyast::BinOp::kDiv: res = builder.create<mlir::arith::DivFOp>(loc(), lhs, rhs); break;
    case pyast::BinOp::kMult: res = builder.create<mlir::arith::MulFOp>(loc(), lhs, rhs); break;
    case pyast::BinOp::kPow: res = builder.create<mlir::math::PowFOp>(loc(), lhs, rhs); break;
    default: break;
  }

  return res;
}

mlir::Value MLIRGenImpl::mlirGen(pyast::Call* expr) {
  mlir::Value res;
  if (expr->get_func()->get_kind() == pyast::expr::kAttribute) {
    auto func_ = expr->get_func().get();
    auto func = *dynamic_cast<pyast::Attribute*>(func_);
    auto func_value = func.get_value();

    if (func_value->get_kind() != pyast::expr::kName
        || dynamic_cast<pyast::Name*>(func_value.get())->get_id() != "math") {
      theModule->emitError("only support call func is python math lib");
    }
    if (expr->get_args().size() != 1) {
      theModule->emitError("attribute node only support call func with one param");
    }

    auto value = mlirGen(expr->get_args()[0].get());
    auto attr = func.get_attr();

    if (attr == "floor") {
      res = builder.create<mlir::math::FloorOp>(loc(), value);
    } else if (attr == "cos") {
      res = builder.create<mlir::math::CosOp>(loc(), value);
    } else if (attr == "ceil") {
      res = builder.create<mlir::math::CeilOp>(loc(), value);
    } else {
      theModule->emitError(attr + " not support yet");
    }
  } else if (expr->get_func()->get_kind() == pyast::expr::kName) {
    auto func_ = expr->get_func().get();
    auto func = *dynamic_cast<pyast::Name*>(func_);

    if (expr->get_args().size() != 2) {
      theModule->emitError("name node only support call func with two param");
    }

    auto left = mlirGen(expr->get_args()[0].get());
    auto right = mlirGen(expr->get_args()[1].get());

    auto attr = func.get_id();

    if (attr == "max") {
      res = builder.create<mlir::arith::MaxFOp>(loc(), left, right);
    } else if (attr == "min") {
      res = builder.create<mlir::arith::MinFOp>(loc(), left, right);
    } else {
      theModule->emitError(attr + " not support yet");
    }

  } else {
    theModule->emitError("only support call func is attribute and name node");
  }

  return res;
}

mlir::Value MLIRGenImpl::mlirGen(pyast::Constant* expr) {
  float value = expr->get_value();
  auto constant = builder.create<mlir::arith::ConstantOp>(loc(), builder.getF32FloatAttr(value));
  return constant;
}

mlir::Value MLIRGenImpl::mlirGen(pyast::Name* expr) {
  auto key = lookup(expr->get_id());
  builder.setInsertionPointToEnd(builder.getInsertionBlock());
  auto value = builder.create<mlir::memref::LoadOp>(loc(), key);
  return value;
}

void MLIRGenImpl::mlirGen(pyast::Assign* stmt) {
  auto value = mlirGen(stmt->get_value().get());

  for (auto& target : stmt->get_targets()) {
    if (target->get_kind() != pyast::expr::kName) {
      theModule->emitError("only support assign to name node");
    }
    auto name = dynamic_cast<pyast::Name*>(target.get())->get_id();

    declare(name, value);
  }
}

void MLIRGenImpl::mlirGen(pyast::Return* stmt) {
  auto value = mlirGen(stmt->get_value().get());

  builder.create<mlir::func::ReturnOp>(loc(), mlir::ValueRange({value}));
}
