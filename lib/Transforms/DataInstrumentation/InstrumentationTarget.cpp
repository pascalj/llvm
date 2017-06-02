#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/DataInstrumentation/InstrumentationTarget.h"
#include <string>
#include "cxxabi.h"

using namespace llvm;

std::string demangle(const char* name) {
  int status = -1;
  std::unique_ptr<char, void(*)(void*)> res {
    abi::__cxa_demangle(name, NULL, NULL, &status),
        std::free
  };
  return (status == 0) ? res.get() : name;
}

bool InstrumentationTarget::IsTargetForFunction(Function &F) {
  return std::regex_match(demangle(F.getName().str().c_str()), Pattern);
}

class VectorInstrumentationTarget : public InstrumentationTarget {
public:
  Value* GetEntryInstrumentation(Instruction &I) override {
  }

  Value* GetExitInstrumentation(Instruction &I) override {
  }
  ~VectorInstrumentationTarget() {}
};
