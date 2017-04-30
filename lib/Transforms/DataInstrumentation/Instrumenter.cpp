#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
namespace {

void InsertBeforeReturn(Function &F) {
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
    if(isa<ReturnInst>(*I))
      errs() << *I << "\n";
}


Function* FunctionToInsert(Function &F) {
  FunctionType *FT = FunctionType::get(Type::getVoidTy(F.getContext()), false);
  return cast<Function>(F.getParent()->getOrInsertFunction("foobar", FT));
}

void InsertAfterEntry(Function &F) {
  BasicBlock& Entry = F.getEntryBlock();
  auto I = Entry.getFirstNonPHI();
  auto foobar = FunctionToInsert(F);
  CallInst::Create(foobar, "", I);
  errs() << *I << "\n";
}

class DataInstrumenter : public FunctionPass {
 public:
  static char ID;
  DataInstrumenter() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) {
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
