
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

struct sexpr_parser {
  std::function< void (const sexpr::list& prog ) > handler;

  void operator()( const char* line ) const {
	std::stringstream ss(line);
	
	try {
	  sexpr::list prog = parse(ss);
	  handler(prog);
	} catch( parse_error& e ) {
	  std::cerr << "parse error: " << e.what() << std::endl;
	}
	
  }

};

struct lisp_handler {
  mutable lisp::environment env = shared<lisp::environment_type>();

  void operator()(const sexpr::list& prog) const {
	try {
	  
	  for(const auto& s : prog) {
		std::cout << lisp::eval(env, s) << std::endl;
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
		ast::node e = transform( s );

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

  



int main() {

  std::cout << std::boolalpha;
  
  sexpr_parser handler = { lisp_handler{} };
  // sexpr_parser handler = { hm_handler{} };  
  
  repl::loop( handler );
  
  return 0;
}
