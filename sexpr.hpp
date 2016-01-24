#ifndef SEXPR_HPP
#define SEXPR_HPP

#include "common.hpp"
#include "variant.hpp"

#include <ostream>
#include <istream>

// parse tree
namespace sexpr {

  struct list;

  // TODO string, number
  using expr = variant< list, symbol, int >;

  struct list : vec<expr> {
	using vec<expr>::vec;

	list(const vec<expr>& other) : vec<expr>(other) { }
	list(vec<expr>&& other) : vec<expr>(std::move(other)) { }  
	list() {}
	
	friend std::ostream& operator<<(std::ostream& out, const list& x);
  };


  sexpr::list parse(std::istream& in);
}

#endif
