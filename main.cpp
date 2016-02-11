
#include <iostream>

#include "hindley_milner.hpp"
#include "repl.hpp"
#include "lisp.hpp"
#include "parse.hpp"
#include "syntax.hpp"

#include <fstream>
#include <boost/program_options.hpp>

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


struct number {

  inline lisp::real operator()(const lisp::real& x) const { return x; }
  inline lisp::real operator()(const int& x) const { return x; }

  template<class T>
  inline lisp::real operator()(const T& ) const {
	throw lisp::error("number expected");
  }
  
};



namespace lisp {

  struct type {
	
	symbol operator()(const lisp::integer& ) const {
	  static const symbol res = "int";
	  return res;
	}

	
	symbol operator()(const symbol& ) const {
	  static const symbol res = "symbol";
	  return res;
	}

	
	symbol operator()(const lisp::string& ) const {
	  static const symbol res = "string";
	  return res;
	}

	
	symbol operator()(const lisp::real& ) const {
	  static const symbol res = "real";
	  return res;
	}


	symbol operator()(const lisp::boolean& ) const {
	  static const symbol res = "bool";
	  return res;
	}


	symbol operator()(const lisp::list& ) const {
	  static const symbol res = "list";
	  return res;
	}


	symbol operator()(const lisp::object& self) const {
	  return self->type;
	}	


	symbol operator()(const lisp::environment& ) const {
	  static const symbol res = "environment";
	  return res;
	}


	symbol operator()(const lisp::lambda& ) const {
	  static const symbol res = "lambda";
	  return res;
	}

	
	symbol operator()(const lisp::builtin& ) const {
	  static const symbol res = "builtin";
	  return res;
	}
	
	
	template<class T>
	symbol operator()(const T& ) const {
	  throw lisp::error("unimplemented");
	}

	
  };
}

struct lisp_handler {
  mutable lisp::environment env = shared<lisp::environment_type>();

  
  template<class T>
  static T& expect_type(lisp::value& self)  {
	if( !self.is<T>() ) {
	  std::stringstream ss;

	  if(std::is_same<T, lisp::object>::value ) {
		ss << "object";
	  } else {
		T* derp = nullptr;		// #yolo
		ss << lisp::type{}(*derp).name();
	  } 
	  ss << " expected";
	  throw lisp::error( ss.str() );
	}
	
	return self.as<T>();
  }


  static lisp::object& expect_type(const symbol& type, lisp::value& self)  {
	if( !self.is<lisp::object>() || self.as<lisp::object>()->type != type) {
	  throw lisp::error( std::string( type.name() ) + " expected");
	}
	
	return self.as<lisp::object>();
  }

  static void expect_argc(unsigned argc, unsigned n) {
	if(argc != n) throw lisp::error("argc != n");
  }
  
  
  lisp_handler() {
	using namespace lisp;


	(*env)[ "echo" ] = +[](environment&, value* arg, value* end) -> value {
	  for(value* it = arg; it != end; ++it) {
		std::cout << (it == arg ? "" : " ") << *it;
	  }
	  std::cout << std::endl;
	  return lisp::null;
	};


	(*env)[ "error" ] = +[](environment&, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  if( !argc ) throw error("argc == 0");
	  
	  std::stringstream ss;
	  ss << std::boolalpha << *arg;
	  
	  for(value* it = arg + 1; it != end; ++it) {
		ss << "\n   " << *it;
	  }

	  throw error( ss.str() );
	};

	
	(*env)[ "eq?" ] = +[](environment&, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  if(argc < 2) throw error("argc < 2");

	  const value& first = *arg;

	  for(value* it = arg + 1; it != end; ++it) {
		if( first != *it ) return false;
	  }

	  return true;
	};
	
	
	(*env)[ "list?" ] = +[](environment&, value* arg, value* last) -> value {
	  const unsigned argc = last - arg;
	  expect_argc(argc, 1);

	  return arg[0].is<list>();
	};

	(*env)[ "null?" ] = +[](environment&, value* arg, value* last) -> value {
	  const unsigned argc = last - arg;
	  expect_argc(argc, 1);

	  return arg[0] == null;
	};

	(*env)["cons"] = +[](environment&, value* arg, value* last) -> value {
	  const unsigned argc = last - arg;
	  expect_argc(argc, 2);
	  
	  return std::make_shared<cons>(arg[0], expect_type<list>(arg[1]));
	};

	(*env)["car"] = +[]( environment&, value* arg, value* last) -> value {
	  const unsigned argc = last - arg;
	  expect_argc(argc, 1);
	  
	  return expect_type<list>(arg[0])->head;
	};

	(*env)["cdr"] = +[](environment&, value* arg, value* last) -> value {
	  const unsigned argc = last - arg;
	  expect_argc(argc, 1);
	  
	  return expect_type<list>(arg[0])->tail;
	};

	(*env)["apply"] = +[](environment& env, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  expect_argc(argc, 2);

	  // TODO optimize
	  vec<value> args;

	  for(const auto& v : expect_type<list>(arg[1]) ) {
		args.push_back(v);
	  }
	  
	  return apply(env, arg[0], &args.front(), &args.front() + args.size());
	};


	

	
	(*env)[ "type" ] = +[](environment&, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  expect_argc(argc, 1);
	  return arg->apply<value>( lisp::type() );
	};

	
	(*env)[ "object-attr" ] = +[](environment&, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  if( argc != 2 && argc != 3) throw error("bad arg count, expected 2, 3");
	  
	  object& obj = expect_type<object>( arg[0] );
	  symbol& attr = expect_type<symbol>( arg[1]);

	  auto it = obj->find( attr );
	  if( it == obj->end() ) {
		if( argc == 3 ) throw error( *expect_type<string>(arg[2]) );
		throw error(std::string("attribute error: ") + attr.name() );
	  }
	  return it->second;
	};

	

	
	(*env)[ "object-attr!" ] = +[](environment&, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  expect_argc(argc, 3);
	  
	  object& obj = expect_type<object>( arg[0] );
	  symbol& attr = expect_type<symbol>( arg[1]);

	  auto it = obj->find( attr );
	  if( it == obj->end() ) throw error(std::string("attribute error: ") + attr.name() );
	  it->second = arg[2];
	  return null;
	};
	
	(*env)[ "object-make-attr!" ] = +[]( environment&, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  expect_argc(argc, 3);
	  
	  object& obj = expect_type<object>( arg[0] );
	  symbol& attr = expect_type<symbol>( arg[1]);

	  auto it = obj->insert( std::make_pair(attr, arg[2] ));
	  if(!it.second) throw error(std::string("attribute already exists: ") + attr.name() );
	  return null;
	};	
	
	
	(*env) [ "object" ] = +[]( environment&, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  expect_argc(argc, 1);

	  symbol& type = expect_type<symbol>(arg[0]);
	  object res = std::make_shared<object_type>(type);

	  return res;
	};



	(*env)["symbol-append"] = +[]( environment&, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  expect_argc(argc, 2);
	  
	  symbol& lhs = expect_type<symbol>(arg[0]);
	  symbol& rhs = expect_type<symbol>(arg[1]);

	  symbol res = std::string(lhs.name()) + std::string(rhs.name());
	  return res;
	};
	
	
	
	(*env) ["number-add"] = +[]( environment&, value* first, value* last) -> value {
	  const unsigned argc = last - first;
	  if(!argc) throw error("1+ args expected");
		
	  real res = 0;

	  for(value* arg = first; arg != last; ++arg) {
		res += arg->apply<real>( number() );
	  }

	  return res;
	};

	
	(*env)[ "number-mult" ] = +[]( environment&, value* first, value* last) -> value {
	  const unsigned argc = last - first;
	  if(!argc) throw error("1+ args expected");
		
	  real res = 1;

	  for(value* arg = first; arg != last; ++arg) {
		res *= arg->apply<real>( number() );
	  }

	  return res;
	};

	
	(*env)[ "number=?" ] = +[]( environment&, value* first, value* last) -> value {
	  const unsigned argc = last - first;
	  if(argc < 2) throw error("2+ args expected");
	  
	  const real val = first[0].apply<real>(number());
	  
	  for(value* arg = first + 1; arg != last; ++arg) {
		if( val != arg->apply<real>(number()) ) return false;
	  }
		
	  return true;
	};	

	
	(*env)[ "number-sub" ] = +[]( environment&, value* first, value* last ) -> value {
	  const unsigned argc = last - first;
	  if(!argc) throw error("1+ args expected");
		
	  real res = 0;

	  for(value* arg = first; arg != last; ++arg) {

		const real n = arg->apply<real>(number());
		res = (arg == first) ? n : res - n;

	  }

	  return res;
	};


	(*env)[ "string-append" ] = +[]( environment&, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  expect_argc(argc, 2);

	  return shared<std::string>( *expect_type<string>(arg[0]) + *expect_type<string>(arg[1]));
	};

	
	(*env)[ "string=?" ] = +[]( environment&, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  expect_argc(argc, 2);

	  return *expect_type<string>(arg[0]) == *expect_type<string>(arg[1]);
	};


	(*env)[ "to-string" ] = +[]( environment&, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  expect_argc(argc, 1);

	  std::stringstream ss;
	  ss << arg[0];
	  
	  return shared<std::string>(ss.str());
	};
	
	// (*env)[ "make-env" ] = +[](environment, value* first, value* last) -> value {
	//   const unsigned argc = last - first;
	//   if(argc) throw error("unexpected argument");

	//   return shared<environment_type>();
	// };

	
	(*env)[ "env" ] = +[]( environment& env, value* , value* ) -> value {
	  return env;
	};


	(*env)["make-overload"] = +[](environment& , value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  expect_argc(argc, 3);

	  struct capture_type {
		object typeclass;
		symbol typeclass_name;
		symbol func_name;
	  };

	  auto capture = shared<capture_type>();
	  
	  capture->typeclass = expect_type("class", arg[0]);
	  capture->typeclass_name = expect_type<symbol>(arg[1]);	  
	  capture->func_name = expect_type<symbol>(arg[2]);
	  
	  auto res = shared<closure_type>();

	  res->data = capture;
	  res->func = +[](environment& env, void* data, value* arg, value* end) -> value {
		capture_type* capture = reinterpret_cast<capture_type*>(data);
		
		const unsigned argc = end - arg;
		if( argc < 1 ) throw error("can't infer instance without arguments");
		
		auto type = arg[0].apply<symbol>( lisp::type() );

		// TODO try/catch
		try{
		  object& instance = expect_type("instance", capture->typeclass->at(type));

		  try{
			value& overload = instance->at(capture->func_name);

			return apply(env, overload, arg, end);
		  } catch( std::out_of_range& e) {
			std::stringstream ss;
			ss << " instance " << capture->typeclass_name << " " << type << " does not implement " << capture->func_name;
			throw error(ss.str());
		  }

		} catch( std::out_of_range& e ) {
		  std::stringstream ss;
		  ss << type << " is not an instance of " << capture->typeclass_name;
		  throw error(ss.str());
		}

	  };

	  return res;
	};
	
	
	// (*env)[ "@" ] = +[](environment, value* first, value* last) -> value {

	//   const unsigned argc = last - first;

	//   if(argc != 2 ) throw error("expected 2 arguments");
		
	//   if( !first[0].is<environment>() ) throw error("expected environment");
	//   if( !first[1].is<symbol>() ) throw error("expected symbol");

	//   auto& self = first[0].as<environment>();
	//   auto& s = first[1].as<symbol>();

	//   auto& var = self->find(s, [&] {
	// 	  throw error("key not found " + std::string(s.name()) );
	// 	});
		
	//   return var;
	// };

	
	// (*env)[ "@!" ] = +[](environment, value* arg, value* end) -> value {
		
	//   const unsigned argc = end - arg;

	//   if(argc != 3 ) throw error("expected 3 arguments");
		
	//   if( !arg[0].is<environment>() ) throw error("expected environment");
	//   if( !arg[1].is<symbol>() ) throw error("expected symbol");

	//   auto& self = arg[0].as<environment>();
	//   auto& s = arg[1].as<symbol>();

	//   (*self)[s] = arg[2];
	//   return nil();
		
	// };	

  }

  
  void operator()(const sexpr::list& prog) const {
	try {
	  
	  for(const sexpr::expr& s : prog) {
		// std::cout << s << std::endl;
		const lisp::value res = lisp::eval(env, lisp::convert(s) );

		if(res != lisp::null) {
		  std::cout << res << std::endl;
		}
	  }
	  
	} catch( lisp::error& e) {
	  std::cerr << "evaluation error: " << e.what() << std::endl;
	  if(!e.details.empty()) std::cerr << e.details << std::flush;
	}
	
  }
  
};



struct hm_handler {
  mutable context ctx;




  struct type_check {

    type::poly operator()(const ast::expr& self, const context& ctx) const {
	  type::poly p = hindley_milner(ctx, self);
	  std::cout << " :: " << p << std::endl;
      return p;
	}

    type::poly operator()(const ast::def& self, context& ctx) const {
	  // we infer type using: (def x y) => (let x y x)
	  ast::let tmp = {self.id, self.value, shared<ast::expr>(self.id)};
	  type::poly p = hindley_milner(ctx, tmp);
	  
	  // add to type context
	  ctx[tmp.id] = p;

	  // TODO eval
	  std::cout << self.id << " :: " << p << std::endl;
      return p;
	}


	// TODO organize this mess ?
    type::poly operator()(const ast::data& self, context& ctx) const {

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
  
  
  void operator()(const sexpr::list& prog) const {
	try {
	  for(const sexpr::expr& s : prog ) {
		const ast::node e = transform( s );
		e.apply<type::poly>(type_check(), ctx);
		
	  }
	}	
	catch( syntax_error& e ) {
	  std::cerr << "syntax error: " << e.what() << std::endl;
	}
	catch( type_error& e ) {
	  std::cerr << "type error: " << e.what() << std::endl;
	}
	
  }


  hm_handler() {

	ctx[ "+" ] = type::mono( type::integer >>= type::integer >>= type::integer);
	ctx[ "*" ] = type::mono( type::integer >>= type::integer >>= type::integer);    
	ctx[ "-" ] = type::mono( type::integer >>= type::integer >>= type::integer);
	ctx[ "=" ] = type::mono( type::integer >>= type::integer >>= type::boolean);

	// this is just for type inference purpose
	{
	  type::var v;
	  ctx[ "if" ] = generalize(ctx, type::boolean >>= v >>= v >>= v);

	}

    static type::abs IO("IO", 1);
    static type::abs Ref("Ref", 1);
    static type::abs List("List", 1);
    
    {
      type::var a;
      ctx["return"] = generalize(ctx, a >>= IO(a) );
    }


    {
      type::var a, b;
      ctx["bind"] = generalize(ctx, IO(a) >>= (a >>= IO(b)) >>= IO(b) );
    }

    {
      type::var a;
      ctx["get"] = generalize(ctx, Ref(a) >>= a );
    }

    {
      type::var a;
      ctx["set"] = generalize(ctx, Ref(a) >>= a >>= IO(type::unit) );
    }

    {
      type::var a;
      ctx["ref"] = generalize(ctx, a >>= IO(Ref(a)));
    }

    {
      type::var a;
      ctx["[]"] = generalize(ctx, a);
    }

    {
      type::var a;
      ctx["cons"] = generalize(ctx, a >>= List(a) >>= List(a) );
    }
    
    
    // ctx["get"] = 

    
	// TODO n-ary function
	// ctx[ symbolize("=") ] = type::mono( type::integer >> type::integer );		

  }

  
};

  

namespace po = boost::program_options;
po::variables_map parse_options(int ac, const char* av[]) {
  
  po::options_description desc("options");

  desc.add_options()
    ("help,h", "produce help message")
    ("lisp", "lisp evaluation")
    ("hm", "hindley-milner type inference (default)")
	("input", po::value<std::string>(), "filename")
	;

  po::positional_options_description p;
  p.add("input", -1);
  
  po::variables_map vm;
  po::parsed_options parsed = po::command_line_parser(ac, av).
	options(desc).positional(p).run();
  
  po::store(parsed, vm);
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
  
  sexpr_parser parser;

  if(vm.count("lisp")) {
	parser.handler = lisp_handler{};

	std::ifstream init("init.lisp");
	parser( init );
  } else {
	parser.handler = hm_handler{};
  }

  if( vm.count("input") ) {
	std::ifstream file( vm["input"].as<std::string>().c_str() );
	if( !file ) throw std::runtime_error("file not found");
	
	parser( file );
	
  } else {

	repl::loop( parser );
  }
  
  return 0;
}
