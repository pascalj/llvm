#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/ADT/Statistic.h"
#include <iostream>
#include <regex>

#ifndef DEBUG_TYPE
#define DEBUG_TYPE "data-instrumentation"
#endif

STATISTIC(InstrEntryPts, "Number of instrumented entry points");
STATISTIC(InstrRetPts, "Number of instrumented return instructions");

using namespace llvm;

namespace {
// actual function pass
class DataInstrumenter : public FunctionPass {
 public:
  static char ID;
  DataInstrumenter() :
    FunctionPass(ID) {}

  bool runOnFunction(Function &F) {
    bool modified = false;
    if (F.hasFnAttribute("function-instrument")) {
      modified = true;
      auto &entry = F.getEntryBlock();
      auto entryInstr = entry.getFirstNonPHI();
      auto vector = F.arg_begin();
      std::vector<Value*> params;
      params.push_back(vector);
      params.push_back(ConstantInt::get(Type::getInt64Ty(F.getContext()), F.arg_size(), false));
      auto &Ctx = entryInstr->getContext();
      FunctionType *FT = FunctionType::get(Type::getVoidTy(Ctx), {vector->getType(), Type::getInt64Ty(Ctx)}, false);
      auto invoke = cast<Value>(entryInstr->getModule()->getOrInsertFunction("_entry_log", FT));
      DEBUG(errs() << "Instrumenting before instruction" << entryInstr << '\n');
      CallInst::Create(invoke, params, "", entryInstr);
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        if (isa<ReturnInst>(*I)) {
          auto returnLog = cast<Value>(entryInstr->getModule()->getOrInsertFunction("_exit_log", FT));
          DEBUG(errs() << "Instrumenting before instruction" << &*I << '\n');
          CallInst::Create(returnLog, params, "", &*I);
        }
      }
    }
    return modified;
  }
};

}

char DataInstrumenter::ID = 0;
static RegisterPass<DataInstrumenter> X("DataInstrumenter", "Instrument data structures", false, false);

// Use the legacy pass manager to register this as a default pass
static void loadPass(const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
  PM.add(new DataInstrumenter());
}
static RegisterStandardPasses dataInstrumenter_Ox(PassManagerBuilder::EP_OptimizerLast, loadPass);
// also run it on -O0
static RegisterStandardPasses dataInstrumenter_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, loadPass);
