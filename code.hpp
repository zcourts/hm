#ifndef HM_LLVM_HPP
#define HM_LLVM_HPP

// #include <llvm/ADT/STLExtras.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/TypeBuilder.h>

// jit
#include <llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>

#include "ast.hpp"

namespace code {

  // some helpers
  using var = llvm::AllocaInst*;
  using constant = llvm::Constant*;
  using func = llvm::Function*;
  using type = llvm::Type*;
  using func_type = llvm::FunctionType*;  
  using value = llvm::Value*;

  
  // constants
  constant make_constant(const double& );
  constant make_constant(const int& );
  constant make_constant(const bool& b);
  constant make_constant(const char* c);
  
  template<class T>
  inline auto make_type() -> decltype(llvm::TypeBuilder<T, false>::get(llvm::getGlobalContext())) {
	return llvm::TypeBuilder<T, false>::get(llvm::getGlobalContext());
  }

  
  
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
	symbol find_symbol_mangled(const std::string& name);
	
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
