#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
namespace {

// Generate a function to insert
Function* FunctionToInsert(Function &F) {
  FunctionType *FT = FunctionType::get(Type::getVoidTy(F.getContext()), false);
  return cast<Function>(F.getParent()->getOrInsertFunction("foobar", FT));
}

// Insert the function at the beginning of F
void InsertAfterEntry(Function &F) {
  BasicBlock& Entry = F.getEntryBlock();
  auto I = Entry.getFirstNonPHI();
  auto foobar = FunctionToInsert(F);
  CallInst::Create(foobar, "", I);
  errs() << *I << "\n";
}

// Insert the function before the (only) ret of F
void InsertBeforeReturn(Function &F) {
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
    if(isa<ReturnInst>(*I))
      errs() << *I << "\n";
}

// actual function pass
class DataInstrumenter : public FunctionPass {
 public:
  static char ID;
  DataInstrumenter() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) {
    errs() << F.getName() << '\n';
    if (F.getName() == "main") {
      InsertBeforeReturn(F);
      InsertAfterEntry(F);
    }

    return false;
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
