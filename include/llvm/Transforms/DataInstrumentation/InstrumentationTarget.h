#include <string>
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"

class InstrumentationTarget {
public:
  InstrumentationTarget(std::string Name) : FunctionName(Name) {};
  std::string GetFunctionName();
  llvm::Value* GetEntryInstrumentation(llvm::Function &F);
  llvm::Value* GetExitInstrumentation(llvm::Function &F);
private:
  std::string FunctionName;
};
