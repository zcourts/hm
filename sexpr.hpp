#ifndef SEXPR_HPP
#define SEXPR_HPP

#include "common.hpp"
#include "variant.hpp"

namespace sexpr {

  
  // parse tree
  struct list;

  // TODO string, real
  using expr = variant< list, symbol, int >;

  struct list : vec<expr> {
	using vec<expr>::vec;

	list(const vec<expr>& other) : vec<expr>(other) { }
	list(vec<expr>&& other) : vec<expr>(std::move(other)) { }  
	list() {}
	
	friend std::ostream& operator<<(std::ostream& out, const list& x) {
	  out << '(';
	
	  bool first = true;
	  for(const auto& xi : x) {
		if(!first) {
		  out << ' ';
		} else {
		  first = false;
		}
		out << xi;
	  }
	  out << ')';
	
	  return out;
	}
  };


}

#endif
