#ifndef AST_HPP
#define AST_HPP

#include "common.hpp"
#include "variant.hpp"

// syntax tree
namespace ast {

  // expressions
  template<class T> struct lit;	// literals
  struct var;					// variables
  struct abs;					// lambda-abstractions
  struct app;					// applications
  struct let;					// local defintions
  
  using expr = variant< lit<void>, lit<int>, var, abs, app, let >;
  
  template<class T>
  struct lit {
	T value;
  };

  template<> struct lit<void> { };
  
  struct var {
	symbol name;
	inline bool operator<(const var& other) const {
	  return name < other.name;
	}
  };

  // TODO vec for args
  struct abs {
	var args;
	ref<expr> body;
  };

  // TODO vec for args
  struct app {
	ref<expr> func;
	ref<expr> args;
  };

  struct let {
	var id;
	ref<expr> value;
	ref<expr> body;
  };


  // definitions
  struct def {
	var id;
	ref<expr> value;
  };


  // complete syntax TODO types
  using node = variant<expr, def>;
  
}





#endif
