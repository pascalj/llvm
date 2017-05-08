#include <string>
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"

class InstrumentationTarget {
public:
  InstrumentationTarget(const std::string Name) : Func(Name) {};
  std::string FunctionName();
  llvm::Value* GetEntryInstrumentation(llvm::Function &F);
  llvm::Value* GetExitInstrumentation(llvm::Function &F);
private:
  const std::string Func;
};
