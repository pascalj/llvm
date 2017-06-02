#include <regex>
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"

class InstrumentationTarget {
public:
  InstrumentationTarget(const std::regex Rgx) : Pattern(Rgx) {};
  std::string FunctionName();
  bool IsTargetForFunction(llvm::Function &F);
  virtual llvm::Value* GetEntryInstrumentation(llvm::Instruction &I);
  virtual llvm::Value* GetExitInstrumentation(llvm::Instruction &I);
  ~InstrumentationTarget() {}
private:
  const std::regex Pattern;
};
