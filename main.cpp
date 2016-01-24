
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

#include <boost/pending/disjoint_sets.hpp>
#include <boost/spirit/include/qi.hpp>

#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/object/construct.hpp>
#include <boost/phoenix/bind/bind_function.hpp>


#include "hindley_milner.hpp"




int main() {

  std::string data;
  std::cout << "enter string:" << std::endl;
  std::getline(std::cin, data);

  std::stringstream ss(data);

  sexpr::list prog = sexpr::parse(ss);
  
  context c;
  for(const sexpr::expr& s : prog ) {
	ast::expr e = ast::transform( s );
	type::poly p = hindley_milner(c, e);
	std::cout << s << " :: " << p << std::endl;	
  }

  // debug();
  return 0;
}
