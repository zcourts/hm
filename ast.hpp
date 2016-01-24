#ifndef AST_HPP
#define AST_HPP

#include "common.hpp"
#include "variant.hpp"

// syntax tree
namespace ast {
  
  template<class T> struct lit;
  struct var;
  struct func;
  struct call;
  struct let;

  using expr = variant< lit<void>, lit<int>, var, func, call, let >;
  
  template<class T>
  struct lit {
	T value;
  };

  template<> struct lit<void> { };
  
  struct var {
	symbol name;
	bool operator<(const var& other) const {
	  return name < other.name;
	}
  };

  // TODO vec for args
  struct func {
	var args;
	ref<expr> body;
  };

  // TODO vec for args
  struct call {
	ref<expr> func;
	ref<expr> args;
  };

  struct let {
	var id;
	ref<expr> def;
	ref<expr> body;
  };
  
}





#endif
