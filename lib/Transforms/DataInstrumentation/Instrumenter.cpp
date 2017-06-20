#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
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
#include <set>

#ifndef DEBUG_TYPE
#define DEBUG_TYPE "data-instrumentation"
#endif

STATISTIC(InstrEntryPts, "Number of instrumented entry points");
STATISTIC(InstrRetPts, "Number of instrumented return instructions");

using namespace llvm;

namespace {
enum Mode { m_default = 0, reference, ctor, dtor };

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

      // instrument entry point
      switch (mode) {
        case Mode::reference: {
          // add accessed element
          auto args = F.arg_begin();
          auto accessed = ++args;
          params.push_back(accessed);
          CallInst::Create(referenceInstrumentation(F, target, entryInstr, accessed), params, "", entryInstr);
          break;
        }
        case Mode::ctor: {
          CallInst::Create(ctorInstrumentation(F, target, entryInstr), params, "", entryInstr);
          break;
        }
        case Mode::dtor: {
          CallInst::Create(dtorInstrumentation(F, target, entryInstr), params, "", entryInstr);
          break;
        }
        default: {
          CallInst::Create(defaultInstrumentation(F, target, entryInstr), params, "", entryInstr);
          break;
        }
      }
      DEBUG(errs() << "Instrumenting before instruction" << entryInstr << '\n');
      InstrEntryPts++;
    }
    return modified;
  }
private:
  Mode getMode(const Attribute attr) {
    auto val = attr.getValueAsString();
    if (val == "reference") {
      return Mode::reference;
    } else if (val == "ctor") {
        return Mode::ctor;
    } else if (val == "dtor") {
        return Mode::dtor;
    }
    return Mode::m_default;
  }
  
  Value* defaultInstrumentation(Function& f, Value *target, Instruction* i) {
    auto &ctx = i->getContext();
    auto *ft = FunctionType::get(Type::getVoidTy(ctx), {target->getType()}, false);
    return cast<Value>(i->getModule()->getOrInsertFunction("_entry_log", ft));
  }

  Value* ctorInstrumentation(Function& f, Value *target, Instruction* i) {
    auto &ctx = i->getContext();
    auto *ft = FunctionType::get(Type::getVoidTy(ctx), {target->getType()}, false);
    return cast<Value>(i->getModule()->getOrInsertFunction("_ctor_log", ft));
  }

  Value* dtorInstrumentation(Function& f, Value *target, Instruction* i) {
    auto &ctx = i->getContext();
    auto *ft = FunctionType::get(Type::getVoidTy(ctx), {target->getType()}, false);
    return i->getModule()->getOrInsertFunction("_dtor_log", ft);
  }

  Value* writeInstrumentation(Function& f, Value *target, Instruction* i, Value *accessed) {
    auto &ctx = i->getContext();
    auto *ft = FunctionType::get(Type::getVoidTy(ctx), {target->getType()}, false);
    return i->getModule()->getOrInsertFunction("_write_log", ft);
  }

  
  /**
   Instrument a "reference" instruction
   
   This instruction represents a function call that returns a reference.

   @param f Current function scope
   @param target target instruction
   @param i The actual reference instruction
   @param accessed The index of the accessed element
   @return Instrumentation call
   */
  Value* referenceInstrumentation(Function& f, Value *target, Instruction* i, Value *accessed) {
    // get invokations
    for (auto user: f.users()) {
      auto invoke = dyn_cast<InvokeInst>(user);
      if (!invoke) continue;
      // get accessed element
      auto vector = invoke->getArgOperand(0);
      auto element = invoke->getArgOperand(invoke->getNumArgOperands() - 1);
      // get results of invokations
      for(auto user: findWriteInstructions(invoke)) {
        user->dump();
        if (auto useInstr = dyn_cast<Instruction>(user)) {
          auto &ctx = user->getContext();
          auto params = {vector, element};
          auto *ft = FunctionType::get(Type::getVoidTy(ctx), {vector->getType(), element->getType()}, false);
          auto refLog = i->getModule()->getOrInsertFunction("_write_log", ft);
          CallInst::Create(refLog, params, "", useInstr);
        }
      }
    }
    auto &ctx = i->getContext();
    auto *ft = FunctionType::get(Type::getVoidTy(ctx), {target->getType(), Type::getInt64Ty(ctx)}, false);
    return cast<Value>(i->getModule()->getOrInsertFunction("_reference_log", ft));
  }
  
  
  /**
   Find write instructions for a given value.
   
   When we have a function that returns a reference to an object (e.g. operator[]/at) we
   want to find all write instructions that write to that value's space.

   @param source The source value
   @param depth Depth limiter - how many steps should be checked
   @return The set of values (Instructions really) that are considered to be 'write' instructions
   */
  std::set<Value*> findWriteInstructions(Value *source, int depth = 4) {
    if (--depth == 0) return {};
    std::set<Value*> result;
    if (isa<StoreInst>(source)) result.insert(source);
    else if (auto call = dyn_cast<CallInst>(source)) {
      if (isa<MemCpyInst>(call)) {
        result.insert(source);
        return result;
      }
      for (auto user: source->users()) {
        auto userInstrs = findWriteInstructions(user, depth);
        result.insert(userInstrs.begin(), userInstrs.end());
      }
    } else if (isa<InvokeInst>(source)) {
      for (auto user: source->users()) {
        auto userInstrs = findWriteInstructions(user, depth);
        result.insert(userInstrs.begin(), userInstrs.end());
      }
    } else if (auto bitcast = dyn_cast<BitCastInst>(source)) {
      for (auto user: bitcast->users()) {
        auto userInstrs = findWriteInstructions(user, depth);
        result.insert(userInstrs.begin(), userInstrs.end());
      }
    } else {
      errs() << "Unexpected instruction: \n";
      source->dump();
    }

    return result;
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
