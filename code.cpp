#include "code.hpp"


#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#include <llvm/IR/Mangler.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>


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


  void jit::init() {
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
  }

  jit::jit()
	: target(llvm::EngineBuilder().selectTarget()),
	  layout(target->createDataLayout()), // if this fails, you need to jit::init() once first
	  compile(link, llvm::orc::SimpleCompiler(*target))
  {
	llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
  }


  template<class T>
  static std::vector<T> make_set(T t) {
	std::vector<T> res;
	res.push_back(std::move(t));
	return res;
  }


  jit::symbol jit::find_symbol_mangled(const std::string& name) {

	// search for symbols in reverse
	for (auto h : make_range(modules.rbegin(), modules.rend()))
	  // TODO why is this not const ?
      if (auto sym = compile.findSymbolIn(h, name, true)) {
        return sym;
	  }
	
    // If we can't find the symbol in the JIT, try looking in the host
    // process.
    if (auto addr = RTDyldMemoryManager::getSymbolAddressInProcess(name))
      return {addr, JITSymbolFlags::Exported};

    return nullptr;
  }
 
  
  jit::module_handle jit::add(std::unique_ptr<llvm::Module> module) {
	auto resolver = orc::createLambdaResolver([&](const std::string& name) {
		std::cerr << "jit resolver: " << name << std::endl;
		
		if (auto sym = find_symbol_mangled(name)) {
		  return RuntimeDyld::SymbolInfo(sym.getAddress(), sym.getFlags());
		}
		return RuntimeDyld::SymbolInfo(nullptr);
	  }, [](const std::string &) { return nullptr; });

    auto res = compile.addModuleSet( make_set(std::move(module)),
									make_unique<SectionMemoryManager>(),
									std::move(resolver));

    modules.push_back(res);
    return res;
  }


  void jit::remove(module_handle m) {
    modules.erase(std::find(modules.begin(), modules.end(), m));
    compile.removeModuleSet(m);
  }

  jit::symbol jit::find(const std::string& name) {
    return find_symbol_mangled(mangle(name));
  }

 

  std::string jit::mangle(const std::string& name) const {
    std::string res;

    {
      raw_string_ostream stream(res);
      Mangler::getNameWithPrefix(stream, name, layout);
    }
	
    return res;
  }
}
