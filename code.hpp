#ifndef HM_LLVM_HPP
#define HM_LLVM_HPP

// #include <llvm/ADT/STLExtras.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/TypeBuilder.h>


#include "ast.hpp"

namespace code {

  // some helpers
  using var = llvm::AllocaInst*;
  using constant = llvm::Constant*;
  using func = llvm::Function*;
  using type = llvm::Type*;
  using func_type = llvm::FunctionType*;
  using struct_type = llvm::StructType*;
  using value = llvm::Value*;

  
  // constants
  constant make_constant(const double& );
  constant make_constant(const int& );
  constant make_constant(const bool& b);
  constant make_constant(const char* c);

  // types
  template<class T>
  inline auto make_type() -> decltype(llvm::TypeBuilder<T, false>::get(llvm::getGlobalContext())) {
	return llvm::TypeBuilder<T, false>::get(llvm::getGlobalContext());
  }
  
  
  // local variables
  var local(func f, const std::string& name, type t);

  
  // elements
  template<int ... I>
  code::value get_element_ptr(llvm::IRBuilder<>& builder, value x) {
	return builder.CreateInBoundsGEP(x, {make_constant(I)...});
  };


  template<int ... I>
  code::value get_element(llvm::IRBuilder<>& builder, value x) {
	return builder.CreateLoad( get_element_ptr<I...>(builder, x));
  };
  
 
  class globals {

	// get or declare a global from/in a module
	typedef std::function< code::value (llvm::Module* ) > declarator_type;

	// id -> declarator
	std::map<std::string, declarator_type> table;

  public:

	// declare global variable
	globals& variable(const std::string& name, code::type type, bool is_constant = false);

	// declare global function
	globals& function(const std::string& name, code::func_type type);


	// get (declare if needed) global variable from a module
	code::value get(llvm::Module* module, const std::string& name) const;
  
  };
  
}




#endif
