add_llvm_loadable_module( LLVMDataInstrumentation
  InstrumentationTarget.cpp
  Instrumenter.cpp

  PLUGIN_TOOL
  opt
  )
