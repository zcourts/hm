#ifndef JIT_HPP
#define JIT_HPP

// jit
#include <llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/GlobalMappingLayer.h>

class jit {

  using link_type = llvm::orc::ObjectLinkingLayer<>;
  using compile_type = llvm::orc::IRCompileLayer<link_type>;
  using mapping_type = llvm::orc::GlobalMappingLayer<compile_type>;
	
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

  template<class Ret, class ... Args>
  void def(const std::string& name, Ret (*ptr)(Args...) ) {
	mapping.setGlobalMapping(name, (llvm::orc::TargetAddress)ptr);
  }

  template<class Ret = void, class ... Args>
  Ret exec(const std::string& name, Args... args) {
	auto sym = find(name);
	if(!sym) throw std::runtime_error("symbol not found");

	using ptr_type = Ret (*)(Args...);
	auto ptr = (ptr_type)(sym.getAddress());
	
	return ptr(args...);
  }

  
private:

  std::string mangle(const std::string& name) const;
  symbol find_symbol_mangled(const std::string& name);
	
  std::unique_ptr<llvm::TargetMachine> target;
public:
  const llvm::DataLayout layout;
private:
	
  link_type link;
  compile_type compile;
  mapping_type mapping;
	
  std::vector<module_handle> modules;
};



#endif
