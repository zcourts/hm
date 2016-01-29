
#include <iostream>

#include <vector>
#include <map>
#include <set>
#include <memory>

#include "variant.hpp"

#include "common.hpp"
#include "variant.hpp"

#include "sexpr.hpp"
#include "ast.hpp"
#include "type.hpp"

#include "parse.hpp"
#include "syntax.hpp"
#include "hindley_milner.hpp"

#include <sstream>

#include "repl.hpp"
#include "lisp.hpp"

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



struct lisp_handler {
  mutable lisp::environment env = shared<lisp::environment_type>();

  lisp_handler() {
	using namespace lisp;

	(*env)[ "echo" ] = +[](environment, value* arg, value* last) -> value {
	  for(value* it = arg; it != last; ++it) {
		std::cout << *it << " ";
	  }
	  std::cout << std::endl;
	  return nil();
	};

	(*env)[ "list?" ] = +[](environment, value* arg, value* last) -> value {
	  const unsigned argc = last - arg;
	  if( argc != 1) throw error("argc != 1");

	  return arg->is<list>();
	};

	
	(*env)[ "length" ] = +[](environment, value* arg, value* last) -> value {
	  const unsigned argc = last - arg;
	  if( argc != 1) throw error("argc != 1");
	  if( !arg->is<list>() ) throw error("list expected");
	  return (int) arg->as<list>()->size();
	};
	
	(*env)[ "eq?" ] = +[](environment, value* arg, value* end) -> value {
	  const unsigned argc = end - arg;
	  if(argc < 2) throw error("argc < 2");

	  const value& first = *arg;

	  for(value* it = arg + 1; it != end; ++it) {
		if( first != *it ) return false;
	  }

	  return true;
	};

	
	(*env)[ "nth" ] = +[](environment, value* arg, value* last) -> value {
	  const unsigned argc = last - arg;
	  if( argc != 2) throw error("argc != 2");
	  if( !arg->is<list>() ) throw error("list expected");
	  if( !(arg + 1)->is<integer>() ) throw error("integer expected");		
	  return (*arg->as<list>())[ (arg + 1)->as<integer>() ];
	};

	(*env)["cons"] = +[](environment, value* arg, value* last) -> value {
	  const unsigned argc = last - arg;
	  if( argc != 2) throw error("argc != 2");

	  const value& x = *arg;
	  ++arg;
	  
	  if( !arg->is<list>() ) throw error("list expected");
	  const list& y = arg->as<list>();
	  
	  list res = shared< vec<value> >();
	  res->push_back(x);
	  res->insert(res->end(), y->begin(), y->end());
	  
	  return res;
	};

	
	(*env)[ "map" ] = +[](environment env, value* arg, value* last) -> value {
	  const unsigned argc = last - arg;
	  if( argc != 2) throw error("argc != 2");

	  if( !arg->is<list>() ) throw error("list expected");

	  auto res = std::make_shared< vec<value> >();
	  res->reserve(arg->as<list>()->size());

	  const value& func = *(arg + 1);

	  std::transform(arg->as<list>()->begin(), arg->as<list>()->end(),
					 std::back_inserter(*res), [&](value& x) {
					   return lisp::apply(env, func, &x, &x + 1);
					 });
		
	  return res;
	};	

	
	(*env)[ "list" ] = +[](environment, value* arg, value* last) -> value {
	  return std::make_shared<vec<value>>(arg, last);
	};
	
	
	(*env)[ "+" ] = +[](environment, value* first, value* last) -> value {
	  const unsigned argc = last - first;
	  if(!argc) throw error("1+ args expected");
		
	  real res = 0;

	  for(value* arg = first; arg != last; ++arg) {
		res += arg->apply<real>( number() );
	  }

	  return res;
	};

	
	(*env)[ "*" ] = +[](environment, value* first, value* last) -> value {
	  const unsigned argc = last - first;
	  if(!argc) throw error("1+ args expected");
		
	  real res = 1;

	  for(value* arg = first; arg != last; ++arg) {
		res *= arg->apply<real>( number() );
	  }

	  return res;
	};

	
	(*env)[ "=" ] = +[](environment, value* first, value* last) -> value {
	  const unsigned argc = last - first;
	  if(argc < 2) throw error("2+ args expected");

	  const real val = first[0].apply<real>(number());

	  for(value* arg = first + 1; arg != last; ++arg) {
		if( val != arg->apply<real>(number()) ) return false;
	  }
		
	  return true;
	};	

	
	(*env)[ "-" ] = +[](environment, value* first, value* last ) -> value {
	  const unsigned argc = last - first;
	  if(!argc) throw error("1+ args expected");
		
	  real res = 0;

	  for(value* arg = first; arg != last; ++arg) {

		const real n = arg->apply<real>(number());
		res = (arg == first) ? n : res - n;

	  }

	  return res;
	};


	(*env)[ "make-env" ] = +[](environment, value* first, value* last) -> value {
	  const unsigned argc = last - first;
	  if(argc) throw error("unexpected argument");

	  return shared<environment_type>();
	};

	
	(*env)[ "env" ] = +[](environment env, value* , value* ) -> value {
	  return env;
	};

	
	(*env)[ "@" ] = +[](environment, value* first, value* last) -> value {

	  const unsigned argc = last - first;

	  if(argc != 2 ) throw error("expected 2 arguments");
		
	  if( !first[0].is<environment>() ) throw error("expected environment");
	  if( !first[1].is<symbol>() ) throw error("expected symbol");

	  auto& self = first[0].as<environment>();
	  auto& s = first[1].as<symbol>();

	  auto& var = self->find(s, [&] {
		  throw error("key not found " + std::string(s.name()) );
		});
		
	  return var;
	};

	
	(*env)[ "@!" ] = +[](environment, value* arg, value* end) -> value {
		
	  const unsigned argc = end - arg;

	  if(argc != 3 ) throw error("expected 3 arguments");
		
	  if( !arg[0].is<environment>() ) throw error("expected environment");
	  if( !arg[1].is<symbol>() ) throw error("expected symbol");

	  auto& self = arg[0].as<environment>();
	  auto& s = arg[1].as<symbol>();

	  (*self)[s] = arg[2];
	  return nil();
		
	};	
	
  }

  
  void operator()(const sexpr::list& prog) const {
	try {
	  
	  for(const sexpr::expr& s : prog) {
		// std::cout << s << std::endl;
		const lisp::value res = lisp::eval(env, lisp::convert(s) );

		if(!res.is<lisp::nil>()) {
		  std::cout << res << std::endl;
		}
	  }
	  
	} catch( lisp::error& e) {
	  std::cerr << "eval error: " << e.what() << std::endl;
	}
	
  }
  
};



struct hm_handler {
  mutable context ctx;

  hm_handler() {

	ctx[ "+" ] = type::mono( type::integer >>= type::integer >>= type::integer);
	ctx[ "-" ] = type::mono( type::integer >>= type::integer >>= type::integer);
	ctx[ "=" ] = type::mono( type::integer >>= type::integer >>= type::boolean);

	// this is just for type inference purpose
	{
	  type::var v;
	  ctx[ ast::var("if") ] = generalize(ctx, type::boolean >>= v >>= v >>= v);	
	}
	
	// TODO n-ary function
	// ctx[ symbolize("=") ] = type::mono( type::integer >> type::integer );		

  }



  struct type_check {

	void operator()(const ast::expr& self, const context& ctx) const {
	  type::poly p = hindley_milner(ctx, self);
	  std::cout << " :: " << p << std::endl;
	}

	void operator()(const ast::def& self, context& ctx) const {
	  // we infer type using: (def x y) => (let x y x)
	  ast::let tmp = {self.id, self.value, shared<ast::expr>(self.id)};
	  type::poly p = hindley_milner(ctx, tmp);
	  
	  // add to type context
	  ctx[tmp.id] = p;

	  // TODO eval
	  std::cout << self.id << " :: " << p << std::endl;
	}


	// TODO organize this mess
	void operator()(const ast::data& self, context& ctx) const {

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
	}
	
  };
  
  
  void operator()(const sexpr::list& prog) const {
	try {
	  for(const sexpr::expr& s : prog ) {
		const ast::node e = transform( s );
		e.apply(type_check(), ctx);
		
	  }
	}	
	catch( syntax_error& e ) {
	  std::cerr << "syntax error: " << e.what() << std::endl;
	}
	catch( type_error& e ) {
	  std::cerr << "type error: " << e.what() << std::endl;
	}
	
  }
  
};

  

namespace po = boost::program_options;
po::variables_map parse_options(int argc, const char* argv[], std::vector<std::string>& remaining) {
  
  po::options_description desc("options");

  desc.add_options()
    ("help,h", "produce help message")
    ("lisp", "lisp evaluation")
    ("hm", "hindley-milner type inference (default)")
	;
   
  po::variables_map vm;
  po::parsed_options parsed =
    po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
  po::store(parsed, vm);
  po::notify(vm);    

  if (vm.count("help")) {
	std::cout << desc << "\n";
	std::exit(1);
  }
  
  remaining = po::collect_unrecognized(parsed.options, po::include_positional);
  
  return vm;
}



int main(int argc, const char* argv[] ) {

  std::cout << std::boolalpha;

  vec<std::string> remaining;
  auto vm = parse_options(argc, argv, remaining);
  
  sexpr_parser parser;

  if(vm.count("lisp")) {
	parser.handler = lisp_handler{};

	std::ifstream init("init.lisp");
	parser( init );
  } else {
	parser.handler = hm_handler{};
  }

  if( !remaining.empty() ) {
	std::ifstream file( remaining[0].c_str() );
	if( !file ) throw std::runtime_error("file not found");

	parser( file );
	
  } else {
	repl::loop( parser );
  }
  
  return 0;
}
