#include "code.hpp"






#include <iostream>

namespace code {

  using namespace llvm;

  constant make_constant(const double& self) {
	return ConstantFP::get(getGlobalContext(), APFloat(self));
  }

  constant make_constant(const char* self) {
	return llvm::ConstantDataArray::getString(llvm::getGlobalContext(), self);
  }
  
  // TODO sizeof int ?
  constant make_constant(const int& self) {
	return ConstantInt::get(getGlobalContext(), APInt(64, self, true));
  }
  
  constant make_constant(const bool& self) {
	return ConstantInt::get(getGlobalContext(), APInt(1, self, false));
  }
  
  var local(func f, const std::string& name, type t) {
	IRBuilder<> builder(&f->getEntryBlock(), f->getEntryBlock().begin());
	return builder.CreateAlloca(t, 0, name.c_str());
  }


 


  globals& globals::variable(const std::string& name, code::type type, bool is_constant) {
	table[name] = [name, type, is_constant](llvm::Module* module) -> code::value {
	  // TODO assert type 
	  // try module first
	  code::value res = module->getGlobalVariable(name);
	  if(res) return res;
	  
	  // declare variable
	  return new llvm::GlobalVariable(*module,
									  type, 
									  is_constant,
									  llvm::GlobalValue::AvailableExternallyLinkage,
									  0,
									  name);
	};
	return *this;
  }


  globals& globals::function(const std::string& name, code::func_type type) {

	table[name] = [name, type](llvm::Module* module) -> code::value {
	  // TODO assert type
	  // try module first
	  code::func res = module->getFunction(name);
	  if(res) return res;
	
	  return llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, module);
	};

	return *this;
  }


  // get (declare if needed) global variable from a module
  code::value globals::get(llvm::Module* module, const std::string& name) const {
	auto it = table.find(name);
	
	if( it == table.end() ) {
	  throw std::runtime_error("unknown global: " + name);
	}
	
	return it->second(module);
  }
  
  

  
}
