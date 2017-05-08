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
#include "llvm/Transforms/DataInstrumentation/InstrumentationTarget.h"
#include <iostream>
#include "cxxabi.h"

#ifndef DEBUG_TYPE
#define DEBUG_TYPE "data-instrumentation"
#endif

STATISTIC(InstrEntryPts, "Number of instrumented entry points");
STATISTIC(InstrRetPts, "Number of instrumented return instructions");

using namespace llvm;
namespace {

// Attempt to demangle for easier comparison.
std::string demangle(const char* name) {
    int status = -1;
    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(name, NULL, NULL, &status),
        std::free
    };
    return (status == 0) ? res.get() : name;
}

// Insert the function at the beginning of F
void InsertAfterEntry(Function &F, Value *InsFunc) {
  BasicBlock& Entry = F.getEntryBlock();
  auto I = Entry.getFirstInsertionPt();
  CallInst::Create(InsFunc, ConstantInt::get(Type::getInt64Ty(F.getContext()), F.arg_size(), false), "", &*I);
  InstrEntryPts++;
  for(auto &arg : F.args()) {
    DEBUG(errs() << "Arg: " << arg.getName() << '\n');
  }
  DEBUG(errs() << "Instrumenting after instruction" << *I << '\n');
}

// Insert the function before the (only) ret of F
void InsertBeforeReturn(Function &F, Value *InsFunc) {
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
    if(isa<ReturnInst>(*I)) {
      CallInst::Create(InsFunc, "", &*I);
      InstrRetPts++;
      DEBUG(errs() << "Instrumenting before return instruction" << *I << '\n');
    }

}

// actual function pass
class DataInstrumenter : public FunctionPass {
 public:
  static char ID;
  std::vector<InstrumentationTarget> Targets;
  DataInstrumenter() :
    FunctionPass(ID),
    Targets{
      InstrumentationTarget("main"),
      InstrumentationTarget("std::vector<int, std::allocator<int> >::push_back(int const&)")} {}

  bool runOnFunction(Function &F) {
    for (auto Target : Targets) {
      if(Target.FunctionName() == demangle(F.getName().str().c_str())) {
        InsertAfterEntry(F, Target.GetEntryInstrumentation(F));
        InsertBeforeReturn(F, Target.GetExitInstrumentation(F));
      }
    }

    return true;
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
