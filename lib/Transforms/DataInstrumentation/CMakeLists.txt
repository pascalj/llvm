add_llvm_loadable_module( LLVMDataInstrumentation
  Instrumenter.cpp

  DEPENDS
  intrinsics_gen

  PLUGIN_TOOL
  opt
  )
