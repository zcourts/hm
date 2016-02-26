#ifndef HM_LLVM_HPP
#define HM_LLVM_HPP

// #include <llvm/ADT/STLExtras.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>

// jit
#include <llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>

#include "ast.hpp"

namespace code {

  // some helpers
  using var = llvm::AllocaInst*;
  using func = llvm::Function*;
  using type = llvm::Type*;
  using func_type = llvm::FunctionType*;  
  using value = llvm::Value*;

  
  // constants
  value constant(const double& );
  value constant(const int& );
  value constant(const bool& b);

  
  // local variables
  var local(func f, const std::string& name, type t);

  // jit
  class jit {

	using link_type = llvm::orc::ObjectLinkingLayer<>;
	using compile_type = llvm::orc::IRCompileLayer<link_type>;
	
  public:
	jit();
	
	using module_handle = compile_type::ModuleSetHandleT;

	using symbol = llvm::orc::JITSymbol;
	
	module_handle add(std::unique_ptr<llvm::Module> module);
	void remove(module_handle m);
	
	symbol find(const std::string& name);

	llvm::DataLayout data_layout() const {
	  return target->createDataLayout();
	}

	static void init();
	
  private:

	std::string mangle(const std::string& name) const;
	symbol find_mangled(const std::string& name);
	
	std::unique_ptr<llvm::TargetMachine> target;
  public:
	const llvm::DataLayout layout;
  private:
	
	link_type link;
	compile_type compile;

	std::vector<module_handle> modules;
  };
  
}




#endif
