#include "jit.hpp"

#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/DynamicLibrary.h>

#include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/IR/Mangler.h>

void jit::init() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
}

jit::jit()
  : target(llvm::EngineBuilder().selectTarget()),
	layout(target->createDataLayout()), // if this fails, you need to jit::init() once first
	compile(link, llvm::orc::SimpleCompiler(*target)),
	mapping(compile)
{
	
}


template<class T>
static std::vector<T> make_set(T t) {
  std::vector<T> res;
  res.push_back(std::move(t));
  return res;
}


jit::symbol jit::find_symbol_mangled(const std::string& name) {

  // search for symbols in reverse
  for (auto h : llvm::make_range(modules.rbegin(), modules.rend())) {
	// TODO why is this not const ?
	if (auto sym = mapping.findSymbolIn(h, name, true)) {
	  return sym;
	}
  }

	
  // try global mappings
  if (auto sym = mapping.findSymbol(name, true)) {
	return sym;
  }

	
  // If we can't find the symbol in the JIT, try looking in the host
  // process.
  using namespace llvm;
  if (auto addr = RTDyldMemoryManager::getSymbolAddressInProcess(name))
	return {addr, JITSymbolFlags::Exported};

  return nullptr;
}
 

jit::module_handle jit::add(std::unique_ptr<llvm::Module> module) {
  using namespace llvm;
  auto resolver = orc::createLambdaResolver([&](const std::string& name) {
	  if (auto sym = find_symbol_mangled(name)) {
		return RuntimeDyld::SymbolInfo(sym.getAddress(), sym.getFlags());
	  }
	  return RuntimeDyld::SymbolInfo(nullptr);
	}, [](const std::string &) { return nullptr; });

  auto res = mapping.addModuleSet( make_set(std::move(module)),
								   make_unique<SectionMemoryManager>(),
								   std::move(resolver));

  modules.push_back(res);
  return res;
}


void jit::remove(module_handle m) {
  modules.erase(std::find(modules.begin(), modules.end(), m));
  mapping.removeModuleSet(m);
}

jit::symbol jit::find(const std::string& name) {
  return find_symbol_mangled(mangle(name));
}

 

std::string jit::mangle(const std::string& name) const {
  std::string res;

  {
	using namespace llvm;
	raw_string_ostream stream(res);
	Mangler::getNameWithPrefix(stream, name, layout);
  }
	
  return res;
}
