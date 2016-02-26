
#include <iostream>

#include "hindley_milner.hpp"
#include "repl.hpp"
#include "lisp.hpp"
#include "parse.hpp"
#include "syntax.hpp"

#include "code.hpp"

#include <fstream>
#include <boost/program_options.hpp>

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Transforms/Scalar.h"

struct sexpr_parser {
  std::function< void (const sexpr::list& prog ) > handler;

  void operator()( const char* line ) const {
	std::stringstream ss(line);
	(*this)(ss);
  }
  
  void operator()( std::istream& in) const {
	try {
	  const sexpr::list prog = parse(in);
	  handler(prog);
	} catch( parse_error& e ) {
	  std::cerr << "parse error: " << e.what() << std::endl;
	}
  }

};

static std::map<ast::var, code::value> globals;

struct hm_handler {

  std::shared_ptr<llvm::Module> module;
  std::shared_ptr<llvm::IRBuilder<>> builder;

  std::shared_ptr<code::jit> jit;
  
  mutable context ctx;
  mutable union_find<type::mono> types;

  
  

  struct type_check {

    type::poly operator()(const ast::expr& self, union_find<type::mono>& types, const context& ctx) const {
	  type::poly p = hindley_milner(types, ctx, self);
	  std::cout << " :: " << p << std::endl;
      return p;
	}

    type::poly operator()(const ast::def& self, union_find<type::mono>& types, context& ctx) const {
	  // we infer type using: (def x y) => (let x y x)
	  ast::let tmp = {self.id, self.value, shared<ast::expr>(self.id)};
	  type::poly p = hindley_milner(types, ctx, tmp);
	  
	  // add to type context
	  ctx[tmp.id] = p;

	  // TODO eval
	  std::cout << self.id << " :: " << p << std::endl;
      return p;
	}


	// TODO organize this mess ?
    type::poly operator()(const ast::type& self, union_find<type::mono>& types, context& ctx) const {

	  // out new type
	  type::poly p;

	  // our new definitions
	  context delta;
	  
	  if( self.args.empty()) {
		// new literal type
		type::mono t = type::lit{ self.id.name() };
		
		// build constructors

		// TODO need type constants in context somehow
		for(const auto& ctor : self.def) {

		  if(ctor.args.empty() ){

			// define constants
			delta[ctor.id] = t;
			
		  } else {
			throw std::logic_error("type constants not available yet");
		  }

		}
		
	  } else {

		// TODO not quite sure ...
		using type_context = std::map< ast::var, type::mono >;
		type_context sub;
		
		// new type function
		type::abs func(self.id.name(), self.args.size());

		// fresh type variables for parameters		
		vec<type::mono> args;
		for(const auto& v : self.args ) {
		  type::var fresh;
		  sub[v] = fresh;
		  args.push_back( fresh );
		}

		type::mono t = std::make_shared<type::app_type>(func, args);

		// TODO what if constructors take constant types as args ?
		// TODO need to expose type constants
		
		// now build type constructors and their types
		for(const auto& ctor : self.def ) {

		  type::mono app = t;

		  for(auto i = ctor.args.rbegin(), end = ctor.args.rend(); i != end; ++i) {

			auto it = sub.find(*i);
			if(it == sub.end()) {
			  throw type_error("unknown type parameter " + std::string(i->name()));
			}

			app = (it->second >>= app);
		  }

		  type::poly p = generalize(ctx, app);
		  
		  delta[ ctor.id ] = p;
		}
		
		
	  }

	  ctx.insert( delta.begin(), delta.end() );
	  std::cout << "data " << self.id << std::endl;
      return p;
	}
	
  };




  struct codegen {

	llvm::Module& module;
	llvm::IRBuilder<>& builder;
	code::jit& jit;
	// context& ctx;

	
	template<class T>
	void operator()(const T&, const type::poly& p) const {
	  throw std::runtime_error("unimplemented");
	}


	void operator()(const ast::expr& self, const type::poly& p) const {

	  // toplevel expression
	  auto module = make_module("tmp");

	  if( p.is<type::scheme>() ) throw std::runtime_error("need a fixed type");

	  if( p.as<type::mono>() != type::real ) {
		throw std::runtime_error("only reals yo");
	  }

	  anon_expr(module.get(), [&](const llvm::IRBuilder<>& builder) {
		  return self.apply<code::value>(codegen_expr{*module});
		});
	  
	  module->dump();
	}


	struct codegen_expr {
	  llvm::Module& module;
	  // context& ctx;
	  
	  code::value operator()(const ast::lit<double>& self) const {
		return code::constant(self.value);
	  }

	  code::value operator()(const ast::lit<int>& self) const {
		return code::constant(self.value);
	  }

	  code::value operator()(const ast::lit<bool>& self) const {
		return code::constant(self.value);
	  }

	  code::value operator()(const ast::var& self) const {

		// auto sym = jit.find(self.name);
		// if(sym) {
		//   std::cerr << "global variable" << std::endl;
		// }
	  }
	  

	  template<class T>
	  code::value operator()(const T& ) const {
		throw std::runtime_error("unimplemented");
	  }
	};

	struct helper {
	  bool is_void;
	  code::value value;
	  
	  friend helper operator,(code::value val, helper) {
		return helper{false, val};
	  }
	  
	};

	
	template<class F>
	void anon_expr(llvm::Module* module, const F& body,
				   const std::string& name = "__anon_expr") const {

	  code::type ret = nullptr;
	  
	  auto block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry");
	  llvm::IRBuilder<> builder(block);

	  auto h = (body(builder), helper{true, nullptr} );
	  if(h.is_void) {
		builder.CreateRetVoid();
		ret = builder.getVoidTy();
	  } else {
		builder.CreateRet(h.value);
		ret = h.value->getType();
	  }
	  
	  code::func_type ft = llvm::FunctionType::get(ret, {}, false);
	  code::func f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, module);

	  block->insertInto(f);
	  
	  // TODO run fpm on f ?
	  llvm::verifyFunction(*f);
	};
	

	std::unique_ptr<llvm::Module> make_module(const std::string& name) const {
	  std::unique_ptr<llvm::Module> module;
	  module.reset( new llvm::Module(name, llvm::getGlobalContext() ) );
	  module->setDataLayout(jit.layout);

	  // pass stuff
	  auto fpm = llvm::make_unique<llvm::legacy::FunctionPassManager>(module.get());

	  // Do simple "peephole" optimizations and bit-twiddling optzns.
	  fpm->add(llvm::createInstructionCombiningPass());
		
	  // Reassociate expressions.
	  fpm->add(llvm::createReassociatePass());
	  // Eliminate Common SubExpressions.
	  fpm->add(llvm::createGVNPass());
	  // Simplify the control flow graph (deleting unreachable blocks, etc).
	  fpm->add(llvm::createCFGSimplificationPass());

	  fpm->doInitialization();

	  return module;
	}
	
	void operator()(const ast::def& self, const type::poly& p) const {

	  if( !p.is<type::mono>() ) {
		throw std::runtime_error("codegen needs monotypes");
	  }

	  type::mono t = p.as<type::mono>();

	  if(t != type::real) throw std::runtime_error("unimplemented");
	  
	  using namespace llvm;
	  auto type = Type::getDoubleTy(getGlobalContext());

	  bool is_constant = false;
	  
	  // http://stackoverflow.com/questions/19866349/error-when-creating-a-global-variable-in-llvm
	  

	  // variable is defined, we need to set it
	  // std::vector<code::type> args;
	  
	  // module.dump();

	  // TODO look for variable ?
	  
	  {
		auto module = make_module("global");
		
		globals[self.id] = new GlobalVariable(*module,
											  type,
											  is_constant,
											  GlobalValue::ExternalLinkage,
											  ConstantFP::get(getGlobalContext(), APFloat(0.0)),
											  self.id.name());
		module->dump();
		jit.add(std::move(module));
	  }
	  
	  {

		auto module = make_module("tmp");

		auto variable = new GlobalVariable(*module,
										   type,
										   is_constant,
										   GlobalValue::AvailableExternallyLinkage,
										   0,
										   self.id.name());
		
		anon_expr(module.get(), [&](IRBuilder<>& builder) {
			
			auto value = self.value->apply<llvm::Value*>(codegen_expr{*module});
			
			builder.CreateStore(value, variable);
			
		  });
		
	
		module->dump();
		
		auto handle = jit.add(std::move(module));
		auto sym = jit.find("__anon_expr");
		if(!sym) throw std::runtime_error("symbol not found");
		using ptr_type = void (*)();
		
		auto ptr = (ptr_type)(sym.getAddress());
		ptr();
		
		jit.remove(handle);
	  }


	  
	  
	  
	  
	}
	

  };


  
  
  
  void operator()(const sexpr::list& prog) const {
	try {
	  for(const sexpr::expr& s : prog ) {
		const ast::node e = transform( s );
		type::poly p = e.apply<type::poly>(type_check(), types, ctx);

		e.apply( codegen{*module, *builder, *jit}, p);
		
	  }
	}	
	catch( syntax_error& e ) {
	  std::cerr << "syntax error: " << e.what() << std::endl;
	}
	catch( type_error& e ) {
	  std::cerr << "type error: " << e.what() << std::endl;
	}
	catch( std::runtime_error& e) {
	  std::cerr << "runtime error: " << e.what() << std::endl;
	}
	
  }


  hm_handler() {

	{
	  llvm::LLVMContext& context = llvm::getGlobalContext();
	  module = std::make_shared<llvm::Module>("hm", context);
	  builder = std::make_shared<llvm::IRBuilder<>>(context);

	  code::jit::init();
	  jit.reset( new code::jit );
	}

	
	using namespace type;
	ctx[ "+" ] = mono( integer >>= integer >>= integer);
	ctx[ "*" ] = mono( integer >>= integer >>= integer);    
	ctx[ "-" ] = mono( integer >>= integer >>= integer);
	ctx[ "=" ] = mono( integer >>= integer >>= boolean);

    static type::abs list("list", 1);
	using type::ref;
	
    {
      var a;
      ctx["!"] = generalize(ctx, ref(a) >>= a );
    }

    {
      var a;
      ctx["set!"] = generalize(ctx, ref(a) >>= a >>= io(unit) );
    }

    {
      var a;
      ctx["ref"] = generalize(ctx, a >>= ref(a));
    }

    {
      var a;
      ctx["unsafe"] = generalize(ctx, io(a) >>= a);
    }

    
    {
      var a;
      ctx["nil"] = generalize(ctx, list(a));
    }

    {
      var a;
      ctx["cons"] = generalize(ctx, a >>= list(a) >>= list(a) );
    }

    {
      var a;
      ctx["head"] = generalize(ctx, list(a) >>= a );
    }

    {
      var a;
      ctx["tail"] = generalize(ctx, list(a) >>= list(a) );
    }


	{
      var a;
      ctx["print"] = generalize(ctx, a >>= io(unit) );
    }
	    
  }

  
};

  

namespace po = boost::program_options;
po::variables_map parse_options(int ac, const char* av[]) {
  
  po::options_description desc("options");

  desc.add_options()
    ("help,h", "produce help message")
	("input", po::value<std::string>(), "filename")
	;

  po::positional_options_description p;
  p.add("input", 1);
  
  po::variables_map vm;

  try{
	po::parsed_options parsed = po::command_line_parser(ac, av).
	  options(desc).positional(p).run();
	
	po::store(parsed, vm);
  } catch (po::error const& e) {
	std::cerr << e.what() << '\n';
	std::exit(1);
  }

  po::notify(vm);
  if (vm.count("help")) {
	std::cout << desc << "\n";
	std::exit(1);
  }
  
  return vm;
}



int main(int argc, const char* argv[] ) {

  std::cout << std::boolalpha;
  std::cerr << std::boolalpha;  

  vec<std::string> remaining;
  auto vm = parse_options(argc, argv);
  
  sexpr_parser parser{ hm_handler{} };
  // parser.handler = hm_handler{};

  if( vm.count("input") ) {
	std::ifstream file( vm["input"].as<std::string>().c_str() );
	if( !file ) throw std::runtime_error("file not found");
	
	parser( file );
	
  } else {

	repl::loop( parser );
  }
  
  return 0;
}
