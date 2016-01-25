#ifndef SEXPR_HPP
#define SEXPR_HPP

#include "common.hpp"
#include "variant.hpp"

#include <ostream>

// parse tree
namespace sexpr {

  struct list;
  
  using string = std::string;
  using real = double;
  
  // TODO string, number
  using expr = variant< list, symbol, real, int, bool, string >;
  
  struct list : vec<expr> {
	using vec<expr>::vec;

	list(const vec<expr>& other) : vec<expr>(other) { }
	list(vec<expr>&& other) : vec<expr>(std::move(other)) { }  
	list() {}
	
  };

  std::ostream& operator<<(std::ostream& out, const expr& e);
  
}


#endif
