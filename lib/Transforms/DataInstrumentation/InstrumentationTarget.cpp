#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/DataInstrumentation/InstrumentationTarget.h"
#include <string>

using namespace llvm;

std::string InstrumentationTarget::FunctionName() {
  return Func;
}

Value* InstrumentationTarget::GetEntryInstrumentation(Function &F) {
  auto &Ctx = F.getContext();
  FunctionType *FT = FunctionType::get(Type::getVoidTy(Ctx), {Type::getInt64Ty(Ctx)}, false);
  return cast<Value>(F.getParent()->getOrInsertFunction("_entry_log", FT));
}

Value* InstrumentationTarget::GetExitInstrumentation(Function &F) {
  FunctionType *FT = FunctionType::get(Type::getVoidTy(F.getContext()), false);
  return cast<Value>(F.getParent()->getOrInsertFunction("_exit_log", FT));
}
