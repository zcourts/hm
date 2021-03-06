
#include <iostream>

#include "hindley_milner.hpp"
#include "repl.hpp"
#include "parse.hpp"
#include "syntax.hpp"

#include "code.hpp"

#include <fstream>

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Transforms/Scalar.h"

#include "jit.hpp"


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

static code::type convert(type::mono t);
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


// TODO expose table so that we can create new types ?
static code::type convert(type::mono t) {
  static std::map<type::mono, code::type> table = {
	{type::unit, code::make_type< void >() },
	{type::real,  code::make_type<sexpr::real>()},
	{type::integer,  code::make_type< sexpr::integer >()}, // 	TODO auto bitlength ?
	{type::boolean,  code::make_type< llvm::types::i<2> > ()},
	{cstring, code::make_type<const char*>()} 
  };
  
  auto it = table.find(t);
  if( it != table.end() ) return it->second;

  // TODO this can not be used for closures :-/

  if(t.is<type::app>()) {
	auto& self = t.as<type::app>();

	// ->
	if(self->func == type::func ) {
	  code::type ret;
	  auto args = collect(ret, t);

	  return llvm::FunctionType::get(ret, args, false);
	}

	// io
	if(self->func == type::io ) {
	  return convert(self->args[0]);
	}
	
  }
  throw std::runtime_error("type conversion unimplemented");
}


static code::globals global;

// TODO this is a value store
struct variables {
  variables* parent;
  
  llvm::Module* module;
  
  variables(variables* parent)
	: parent(parent),
	  module(parent->module){
	
  }

  variables(llvm::Module* module)
	: parent(nullptr),
	  module(module) {

  }
  
  std::map<ast::var, code::value> table;
  
  code::value find(const ast::var& v) {

	{
	  auto it = table.find(v);
	  if(it != table.end() ) {
		return it->second;
	  }
	}

	if( parent ) return parent->find(v);

	// unbound variables are implicitly globals
	{
	  code::value res = global.get(module, v.name());
	  table[v] = res;
	  
	  return res;
	}
  }
  
};



static code::func declare(llvm::Module* module, const std::string& name, code::func_type ft) {

  // code::func_type ft = llvm::FunctionType::get(ret, args, false);
  return llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, module);
  
}
	

static std::map<std::string, code::func_type> prototype;


struct hm_handler {

  std::shared_ptr<::jit> jit;
  
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
	  ctx.set(tmp.id, p);

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

	::jit& jit;
	
	template<class T>
	void operator()(const T&, const type::poly& p) const {
	  throw std::runtime_error("unimplemented");
	}


	// toplevel expression
	void operator()(const ast::expr& self, const type::poly& p) const {
	  std::runtime_error derp("derp");
	  
	  auto module = make_module("tmp");

	  if( p.is<type::scheme>() ){
		throw std::runtime_error("need a fixed type");
	  }

	  variables vars{module.get()};

	  static std::map<type::mono, const char*> format = {
		{ type::integer, "print_int" },
		{ type::real, "print_real" },
		{ type::boolean, "print_bool" }
	  };
	  
	  code::value print = nullptr;
	  
	  auto it = format.find(p.as<type::mono>());
	  if(it != format.end()) {
		print = vars.find(it->second);
	  }
	  
	  anon_expr(module.get(), [&](llvm::IRBuilder<>& builder) {
		  // compute expression
		  code::value res = self.apply<code::value>(codegen_expr{builder, vars});
		  // assert(res->getType() == convert(p.as<type::mono>()));
		  
		  if( print ) builder.CreateCall(print, {res});
		  // return res;
		});
	  
	  module->dump();

	  // eval

	  {
		auto handle = jit.add(std::move(module));
		
		std::cout << "eval: ";
		jit.exec("__anon_expr");
		std::cout << std::endl;
		// jit.remove(handle);
	  }	  
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
		code::value v = vars.find(self);

		if( v->getType()->isPointerTy() && !v->getType()->getPointerElementType()->isFirstClassType() ) {
		  return v;
		} else {
		  return builder.CreateLoad(v, self.name() );
		}
	  }
	  

	  code::value operator()(const ast::app& self) const {
		
		code::value func = self.func->apply<code::value>(*this);

		std::vector<code::value> args;

		// TODO reserve
		std::transform(self.args.begin(), self.args.end(),
					   std::back_inserter(args), [&](const ast::expr& e) {
						 return e.apply<code::value>(*this);
					   });

		code::type ft = func->getType();
		
		// regular functions are pointer to function type
		if( ft->isPointerTy() && !ft->getPointerElementType()->isFirstClassType()) {
		  std::cout << "function" << std::endl;
		  return builder.CreateCall(func, args);
		} else {
		  std::cout << "closure "  << ft->isPointerTy() << std::endl;
		  
		  // this is a closure
		  using namespace llvm;

		  code::value tmp = builder.CreateAlloca(func->getType());
		  builder.CreateStore(func, tmp);

		  code::value proc = code::get_element<0, 0>(builder, tmp);
		  code::value data = code::get_element<0, 1>(builder, tmp);

		  args.insert(args.begin(), data);
		  return builder.CreateCall(proc, args);
		}

	  }


	  struct free_variables {
		typedef std::set<ast::var> set;
		set& free;
		const set& bound;
		
		void operator()(const ast::var& self) const {
		  if(bound.find(self) != bound.end() ) {
			free.insert(self);
		  }
		};
		
		void operator()(const ast::app& self) const {
		  self.func->apply(*this);

		  for(const auto& e : self.args) {
			e.apply(*this);
		  }
		};

		void operator()(const ast::abs& self) const {
		  set sub_free;
		  set sub_bound(self.args.begin(), self.args.end());

		  self.body->apply( free_variables{sub_free, sub_bound} );

		  // free += sub_free - bound
		  std::set_difference(sub_free.begin(), sub_free.end(),
							  bound.begin(), bound.end(),
							  std::inserter(free, free.end()));
		};

		void operator()(const ast::cond& self) const {
		  self.app().apply(*this);
		};

		void operator()(const ast::seq& self) const {
		  self.monad().apply(*this);
		};
		
		
		template<class T>
		void operator()(const ast::lit<T>&) const { }

		void operator()(const ast::let& self) const {
		  throw std::logic_error("unimplemented");
		}
		
	  };
	  

	  // lambda TODO need type lol
	  code::value operator()(const ast::abs& self) const {

		// 1 find free variables
		free_variables::set free, bound;
		self.body->apply( free_variables{free, bound} );
		
		// 2 create data structure, fill w/ free variables

		// so normally here types have been inferred for all unbound
		// variables so we can ask the context for types
		// context ctx;

		// TODO need to rework architecture
		
		// 3 update vars so that free variables end up accessing
		// closure data
		

		
		// 4 generate code with updated vars
		
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
		
		auto var = new GlobalVariable(*module,
									  type,
									  is_constant,
									  GlobalValue::ExternalLinkage,
									  ConstantAggregateZero::get(type),
									  self.id.name());

		global.variable(self.id.name(), type, is_constant);
		
		module->dump();
		jit.add(std::move(module));
	  }
	  
	  {

		auto module = make_module("tmp");

		variables vars{module.get()};
		anon_expr(module.get(), [&](IRBuilder<>& builder) {
			
			auto value = self.value->apply<llvm::Value*>(codegen_expr{builder, vars});
			
			builder.CreateStore(value, vars.find(self.id) );
			
		  });
		
	
		module->dump();

		{
		  auto handle = jit.add(std::move(module));

		  jit.exec("__anon_expr");
		  
		  // FIXME: causes segfault everytime an exception is
		  // thrown :-/
		  // jit.remove(handle);
		}
	  }


	  
	  
	  
	  
	}
	


  };


  
  
  
  void operator()(const sexpr::list& prog) const {
	try {
	  for(const sexpr::expr& s : prog ) {
		const ast::node e = transform( s );
		type::poly p = e.apply<type::poly>(type_check(), types, ctx);

		{
		  e.apply( codegen{*jit}, p);
		}
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
	  ::jit::init();
	  jit.reset( new ::jit );

	  jit->add( codegen{*jit}.make_module("global") );
	}

	
	// 
	global.function("printf", code::make_type< void (const char*, ...) >());

	// return;
	
	using namespace type;

	auto def_extern = [&](const std::string& name, type::mono t) {
	  ctx[name] = t;
	  global.function( name, static_cast<code::func_type>(convert(t)) );
	};

	def_extern("print_real", real >>= io(unit) );
	def_extern("print_int", integer >>= io(unit) );
	def_extern("print_bool", boolean >>= io(unit) );
	def_extern("print_endl", unit >>= io(unit) );

	def_extern("add_int", integer >>= integer >>= integer );
	def_extern("sub_int", integer >>= integer >>= integer );	
	
	jit->def("print_real", +[](sexpr::real x) { std::cout << x; } );
	jit->def("print_int", +[](sexpr::integer x) { std::cout << x; } );
	jit->def("print_bool", +[](sexpr::boolean x) { std::cout << x; } );
	jit->def("print_endl", +[]() { std::cout << std::endl; } );

	jit->def("add_int", +[](sexpr::integer x, sexpr::integer y) { return x + y; } );
	jit->def("sub_int", +[](sexpr::integer x, sexpr::integer y) { return x + y; } );		


	{

	  code::struct_type data = llvm::StructType::create(llvm::getGlobalContext(), "data");

	  auto int_t = convert( integer );
	  data->setBody(  { int_t }, false);
	  
	  code::struct_type closure = llvm::StructType::create(llvm::getGlobalContext(), "closure");
	  
	  code::func_type code = llvm::FunctionType::get(int_t, {data, int_t}, false);
	  closure->setBody( { llvm::PointerType::get(code, 0), data}, false);

	  {
		auto module = codegen{*jit}.make_module("tmp");
		variables vars{module.get()};
		
		code::func func = declare(module.get(), "closure_code", code);
		using namespace llvm;
		
		{
		  auto block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry");
		  llvm::IRBuilder<> builder(block);
		  ConstantInt* const_int32_10 = ConstantInt::get(getGlobalContext(), APInt(32, StringRef("0"), 10));

		  std::vector<code::value> args;
		  
		  for(auto& a : func->args() ) {
			args.push_back(&a);
		  }

		  code::value tmp = builder.CreateAlloca(args[0]->getType());
		  builder.CreateStore(args[0], tmp);

		  code::value captured = code::get_element<0, 0>(builder, tmp);
		  code::value sum = builder.CreateCall( vars.find("add_int"), { captured, args[1]} );
		  
		  builder.CreateRet( sum );
		  block->insertInto(func);
		  
		}
		
		using namespace llvm;
		auto var = new GlobalVariable(*module,
									  closure,
									  false,
									  GlobalValue::ExternalLinkage,
									  ConstantStruct::get(closure, {func,  ConstantStruct::get(data, { code::make_constant(2) })}),
									  "add2");

		module->dump();
		jit->add( std::move(module) );
	  }
	  
	  global.variable("add2", closure);
	  ctx["add2"] = mono( integer >>= integer );
	}
	

	
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


	// {
    //   var a;
    //   ctx["print"] = generalize(ctx, a >>= io(unit) );
    // }
	    
  }

  
};

  


int main(int argc, const char* argv[] ) {

  std::cout << std::boolalpha;
  std::cerr << std::boolalpha;  

  sexpr_parser parser{ hm_handler{} };
  // parser.handler = hm_handler{};

  if( argc > 1 ) {
	std::ifstream file(  argv[1] );
	if( !file ) throw std::runtime_error("file not found");
	
	parser( file );
	
  } else {

	repl::loop( parser );
  }
  
  return 0;
}
