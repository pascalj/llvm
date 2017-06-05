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

#ifndef DEBUG_TYPE
#define DEBUG_TYPE "data-instrumentation"
#endif

STATISTIC(InstrEntryPts, "Number of instrumented entry points");
STATISTIC(InstrRetPts, "Number of instrumented return instructions");

using namespace llvm;

namespace {
enum Mode { m_default = 0, read, write };

// actual function pass
class DataInstrumenter : public FunctionPass {
 public:
  static char ID;
  DataInstrumenter() :
    FunctionPass(ID) {}

  bool runOnFunction(Function &F) {
    bool modified = false;
    if (F.hasFnAttribute("ctr-instrument")) {
      modified = true;
      auto entryInstr = F.getEntryBlock().getFirstNonPHI();
      auto target = F.arg_begin();
      auto mode = getMode(F.getFnAttribute("ctr-instrument"));

      // build params
      std::vector<Value*> params;
      params.push_back(target);
      params.push_back(ConstantInt::get(Type::getInt8Ty(F.getContext()), 1 << mode, false));

      // add accessed element
      if (mode == Mode::read || mode == Mode::write) {
        auto args = F.arg_begin();
        auto accessed = ++args;
        params.push_back(accessed);
      }

      auto &Ctx = entryInstr->getContext();

      // instrument entry point
      FunctionType *FT;
      Value *invoke;
      if (mode == Mode::read) {
       FT = FunctionType::get(Type::getVoidTy(Ctx), {target->getType(), Type::getInt8Ty(Ctx), Type::getInt64Ty(Ctx)}, false);
       invoke = cast<Value>(entryInstr->getModule()->getOrInsertFunction("_access_log", FT));
      } else {
       FT = FunctionType::get(Type::getVoidTy(Ctx), {target->getType(), Type::getInt8Ty(Ctx)}, false);
       invoke = cast<Value>(entryInstr->getModule()->getOrInsertFunction("_entry_log", FT));
      }
      DEBUG(errs() << "Instrumenting before entry instruction" << entryInstr << '\n');
      InstrEntryPts++;
      CallInst::Create(invoke, params, "", entryInstr);

      // log before any return of a flagged method
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        if (isa<ReturnInst>(*I)) {
          auto returnLog = cast<Value>(entryInstr->getModule()->getOrInsertFunction("_exit_log", FT));
          DEBUG(errs() << "Instrumenting before return instruction" << &*I << '\n');
          InstrRetPts++;
          CallInst::Create(returnLog, params, "", &*I);
        }
      }
    }
    return modified;
  }
private:
  Mode getMode(const Attribute attr) {
    auto val = attr.getValueAsString();
    if (val == "read") {
      return Mode::read;
    } else if (val == "write") {
      return Mode::write;
    }
    return Mode::m_default;
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
