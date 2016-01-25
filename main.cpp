
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
	
	(*env)[ symbolize("+") ] = builtin( [](environment, vec<value>&& args) -> value {
		if(args.empty()) throw error("1+ args expected");
		
		real res = 0;

		for(const auto& v : args) {
		  res += v.apply<real>( number() );
		}

		return res;
	  });

	(*env)[ symbolize("=") ] = builtin( [](environment, vec<value>&& args) -> value {
		if(args.size() < 2) throw error("2+ args expected");

		const real value = args[0].apply<real>(number());

		for(unsigned i = 1, n = args.size(); i < n; ++i) {
		  if( value != args[i].apply<real>(number()) ) return false;
		}
		  
		return true;
	  });	

	(*env)[ symbolize("-") ] = builtin( [](environment, vec<value>&& args) -> value {
		if(args.empty()) throw error("1+ args expected");
		
		real res = 0;

		unsigned i = 0;
		for(const auto& v : args) {

		  const real n = v.apply<real>(number());
		  res = i ? res - n : n;

		  ++i;
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
		  std::cout << self.id.name << " :: " << p << std::endl;
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

  



int main(int argc, const char* argv[] ) {
  
  std::cout << std::boolalpha;
  
  sexpr_parser handler = { lisp_handler{} };
  // sexpr_parser handler = { hm_handler{} };  

  if( argc > 1) {
	std::ifstream file(argv[1]);
	if( !file ) throw std::runtime_error("file not found");

	handler( file );
	
  } else {
	repl::loop( handler );
  }
  
  return 0;
}
