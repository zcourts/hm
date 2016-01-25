
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

#include <readline/readline.h>
#include <readline/history.h>

const char* getline(const char* prompt) {
  const char* line = readline(prompt);
  if (line && *line) add_history( line );
  return line;
}


int main() {

  const char* line;

  std::cout << std::boolalpha;
  
  context c;  
  while( (line = getline("> ")) ) {
	try {
	  std::stringstream ss(line);

	  sexpr::list prog = parse(ss);
	
	  for(const sexpr::expr& s : prog ) {
		ast::node e = transform( s );

		if( e.is<ast::expr>() ) {
		  type::poly p = hindley_milner(c, e.as<ast::expr>());
		  std::cout << s << " :: " << p << std::endl;
		}
	  
		if( e.is<ast::def>() ) {
		  auto& self = e.as<ast::def>();

		  // we infer type using: (def x y) => (let x y x)
		  ast::let tmp;
		  tmp.id = self.id;
		  tmp.value = self.value;
		  tmp.body = shared<ast::expr>(self.id);

		  type::poly p = hindley_milner(c, tmp);

		  // add to type context
		  c[tmp.id] = p;

		  // TODO eval
		  std::cout << self.id.name << " :: " << p << std::endl;
		}
	  }
	}
	catch( parse_error& e ) {
	  std::cerr << "parse error: " << e.what() << std::endl;
	}	
	catch( syntax_error& e ) {
	  std::cerr << "syntax error: " << e.what() << std::endl;
	}
	catch( type_error& e ) {
	  std::cerr << "type error: " << e.what() << std::endl;
	}

	
  }

  // debug();
  return 0;
}
