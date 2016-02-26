
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



static type::mono cstring = type::traits<const char*>::type();


static code::type convert(type::mono t) {
  static std::map<type::mono, code::type> table = {
	{type::real,  code::make_type<double>()}, 
	{type::integer,  code::make_type< llvm::types::i<64> >()},
	{type::boolean,  code::make_type< llvm::types::i<2> > ()},
	{cstring, code::make_type<const char*>()} 
  };
  
  auto it = table.find(t);
  if( it != table.end() ) return it->second;

  throw std::runtime_error("type conversion unimplemented");
}

struct variables {
  variables* parent;
  
  llvm::Module* module;
  const context& ctx;
  
  variables(variables* parent)
	: parent(parent),
	  module(parent->module),
	  ctx(parent->ctx){
	
  }

  variables(llvm::Module* module,
			const context& ctx)
	: parent(nullptr),
	  module(module),
	  ctx(ctx) {

  }
  
  std::map<ast::var, code::value> table;

 
  
  code::value find(const ast::var& v) {

	auto it = table.find(v);
	if(it != table.end() ) {
	  return it->second;
	}

	if( parent ) return parent->find(v);

	// unbound variables are implicitly external globals
	type::poly p = ctx.find(v);
	if( p.is<type::scheme>() ) throw std::runtime_error("can't codegen with type schemes");
	
	code::type type = convert( p.as<type::mono>() );

	const bool is_constant = false;
	code::value variable = new llvm::GlobalVariable(*module,
													type, 
													is_constant,
													llvm::GlobalValue::AvailableExternallyLinkage,
													0,
													v.name());
	table[v] = variable;

	return variable;
  }
  
};


static std::vector<code::type> collect(code::type& ret, const type::mono& t, std::vector<code::type>&& args = {}) {
  if(t.is<type::app>() && t.as<type::app>()->func == type::func) {
	// function type
	type::app app = t.as<type::app>();
	
	args.push_back( convert(app->args[0]) );
	return collect(ret, app->args[1], std::move(args));
  } else {
	// this is return type;
	ret = convert(t);
	return args;
  }
}

static code::func declare(llvm::Module* module, const std::string& name, code::func_type ft) {

  // code::func_type ft = llvm::FunctionType::get(ret, args, false);
  return llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, module);
  
}
	

static std::map<std::string, code::func_type> prototype;

static code::func get_or_declare(llvm::Module* module, const std::string& name) {

  // try module first
  code::func res = module->getFunction(name);
  if(res) return res;

  // declare function
  auto it = prototype.find(name);
  if(it != prototype.end() ) {
	return declare(module, name, it->second);
  }

  // derp
  throw std::runtime_error("unknown function");
}


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

	code::jit& jit;
	const context& ctx;
	
	template<class T>
	void operator()(const T&, const type::poly& p) const {
	  throw std::runtime_error("unimplemented");
	}


	// toplevel expression
	void operator()(const ast::expr& self, const type::poly& p) const {

	  auto module = make_module("tmp");

	  if( p.is<type::scheme>() ){
		throw std::runtime_error("need a fixed type");
	  }

	  code::func printf = get_or_declare(module.get(), "printf");

	  variables vars{module.get(), ctx};


	  static std::map<type::mono, const char*> format = {
		{ type::integer, "%d" },
		{ type::real, "%f" },
		{ type::boolean, "%i" }
	  };

	  
	  code::value fmt = nullptr;

	  auto it = format.find(p.as<type::mono>());
	  if(it != format.end()) {
		fmt = new llvm::GlobalVariable(*module,
									   code::make_type<const char*>(),
									   true,
									   llvm::GlobalValue::PrivateLinkage,
									   code::make_constant(it->second),
									   "fmt");
	  }
	  
	  anon_expr(module.get(), [&](llvm::IRBuilder<>& builder) {
		  code::value res = self.apply<code::value>(codegen_expr{builder, vars});
		  assert(res->getType() == convert(p.as<type::mono>()));
		  
		  if( fmt ) builder.CreateCall(printf, {fmt, res});
		  // return res;
		});
	  
	  module->dump();

	  // eval

	  auto handle = jit.add(std::move(module));
	  auto sym = jit.find("__anon_expr");
	  if(!sym) throw std::logic_error("symbol not found");
	  
	  using ptr_type = void (*)();
	  auto ptr = (ptr_type)(sym.getAddress());
	  
	  std::cout << "eval: ";
	  ptr();
	  std::cout << std::endl;
	  
	  jit.remove(handle);
	  
	}


	struct codegen_expr {

	  llvm::IRBuilder<>& builder;
	  variables& vars;
	  
	  code::value operator()(const ast::lit<double>& self) const {
		return code::make_constant(self.value);
	  }

	  code::value operator()(const ast::lit<int>& self) const {
		return code::make_constant(self.value);
	  }

	  code::value operator()(const ast::lit<bool>& self) const {
		return code::make_constant(self.value);
	  }

	  code::value operator()(const ast::var& self) const {
		return builder.CreateLoad( vars.find(self), self.name() );
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

	  using namespace llvm;
	  code::type type = convert(t);

	  bool is_constant = false;
	  
	  // http://stackoverflow.com/questions/19866349/error-when-creating-a-global-variable-in-llvm
	  
	  // TODO look for existing definition and store ?
	  {
		auto module = make_module("global");

		
		auto global = new GlobalVariable(*module,
										 type,
										 is_constant,
										 GlobalValue::ExternalLinkage,
										 ConstantAggregateZero::get(type),
										 self.id.name());
		module->dump();
		jit.add(std::move(module));
	  }
	  
	  {

		auto module = make_module("tmp");

		variables vars{module.get(), ctx};
		anon_expr(module.get(), [&](IRBuilder<>& builder) {
			
			auto value = self.value->apply<llvm::Value*>(codegen_expr{builder, vars});
			
			builder.CreateStore(value, vars.find(self.id) );
			
		  });
		
	
		module->dump();
		
		auto handle = jit.add(std::move(module));
		auto sym = jit.find("__anon_expr");
		if(!sym) throw std::logic_error("symbol not found");
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

		e.apply( codegen{*jit, ctx}, p);
		
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
	catch( std::logic_error& e) {
	  std::cerr << "logic error (!!!): " << e.what() << std::endl;
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

	{
	  // expose c functions
	  auto module = codegen{*jit, ctx}.make_module("libc");

	  prototype["printf"] = code::make_type< void (const char*, ...) >();
	  module->dump();
	  
	  jit->add(std::move(module));
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
