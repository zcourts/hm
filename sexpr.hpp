#ifndef SEXPR_HPP
#define SEXPR_HPP

#include "common.hpp"
#include "variant.hpp"

#include <ostream>

#include <iostream>

// parse tree
namespace sexpr {

  using integer = int;
  using string = std::string;
  using real = double;
  struct expr;

  using list = vec<expr>;
  
  struct expr : variant<bool, integer, real, string, symbol, list > {
	using variant::variant;
  };

  std::ostream& operator<<(std::ostream& out, const expr& e);
}


#endif
