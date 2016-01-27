
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

	(*env)[ symbol("+") ] = builtin( [](environment, value* first, value* last) -> value {
		  const unsigned argc = last - first;
		  if(!argc) throw error("1+ args expected");
		
		  real res = 0;

		  for(value* arg = first; arg != last; ++arg) {
			res += arg->apply<real>( number() );
		  }

		  return res;
		});
	
	(*env)[ symbol("=") ] = builtin( [](environment, value* first, value* last) -> value {
		const unsigned argc = last - first;
		if(argc < 2) throw error("2+ args expected");

		const real val = first[0].apply<real>(number());

		for(value* arg = first + 1; arg != last; ++arg) {
		  if( val != arg->apply<real>(number()) ) return false;
		}
		
		return true;
	  });	

	(*env)[ symbol("-") ] = builtin( [](environment, value* first, value* last ) -> value {
		const unsigned argc = last - first;
		if(!argc) throw error("1+ args expected");
		
		real res = 0;

		for(value* arg = first; arg != last; ++arg) {

		  const real n = arg->apply<real>(number());
		  res = (arg == first) ? n : res - n;

		}

		return res;
	  });

  }
  
  void operator()(const sexpr::list& prog) const {
	try {
	  
	  for(const auto& s : prog) {
		std::cout << s << std::endl;
		const lisp::value res = lisp::eval(env, s);

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

	ctx[ ast::var("+") ] = type::mono( type::integer >>= type::integer >>= type::integer);
	ctx[ ast::var("-") ] = type::mono( type::integer >>= type::integer >>= type::integer);
	ctx[ ast::var("=") ] = type::mono( type::integer >>= type::integer >>= type::boolean);

	// this is just for type inference purpose
	{
	  type::var v;
	  ctx[ ast::var("if") ] = generalize(ctx, type::boolean >>= v >>= v >>= v);	
	}
	
	// TODO n-ary function
	// ctx[ symbolize("=") ] = type::mono( type::integer >> type::integer );		

  }

  
  void operator()(const sexpr::list& prog) const {
	try {
	  for(const sexpr::expr& s : prog ) {
		const ast::node e = transform( s );

		if( e.is<ast::expr>() ) {
		  type::poly p = hindley_milner(ctx, e.as<ast::expr>());
		  std::cout << s << " :: " << p << std::endl;
		}
	  
		if( e.is<ast::def>() ) {
		  auto& self = e.as<ast::def>();

		  // we infer type using: (def x y) => (let x y x)
		  ast::let tmp;
		  tmp.id = self.id;
		  tmp.value = self.value;
		  tmp.body = shared<ast::expr>(self.id);

		  type::poly p = hindley_milner(ctx, tmp);

		  // add to type context
		  ctx[tmp.id] = p;

		  // TODO eval
		  std::cout << self.id << " :: " << p << std::endl;
		}
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

  int x;
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
