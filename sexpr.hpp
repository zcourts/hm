#ifndef SEXPR_HPP
#define SEXPR_HPP

#include "common.hpp"
#include "variant.hpp"

#include <ostream>

#include <iostream>

// parse tree
namespace sexpr {

  // TODO make list a ref<list_type> so that boost spirit does not
  // copy lists recursively during parsing (no move semantics in
  // spirit2 :-/)
  struct list;

  using integer = int;
  using string = std::string;
  using real = double;
  
  using expr = variant< list, symbol, real, integer, bool, string >;
  
  struct list : vec<expr> {

	// this makes gcc choke :-/
	// using vec<expr>::vec;

	list(const vec<expr>& other) : vec<expr>(other) { }
	list(vec<expr>&& other) : vec<expr>(std::move(other)) { }
	list() {}

  };

  std::ostream& operator<<(std::ostream& out, const expr& e);
  
}


#endif
