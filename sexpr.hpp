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
  
  using string = std::string;
  using real = double;
  
  using expr = variant< list, symbol, real, int, bool, string >;
  
  struct list : vec<expr> {
	using vec<expr>::vec;

	list(const vec<expr>& other) : vec<expr>(other) { }
	list(vec<expr>&& other) : vec<expr>(std::move(other)) { }
	list() {}

	list(const list& other) : vec<expr>(other) {
	  // std::cout << "copy called" << std::endl;
	}
	
	list(list&& other) : vec<expr>(std::move(other)) {
	  // std::cout << "move called" << std::endl;
	}


	list& operator=(const list& other) = default;
	list& operator=(list&& other) = default;	
  };

  std::ostream& operator<<(std::ostream& out, const expr& e);
  
}


#endif
