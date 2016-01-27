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

  // TODO lists
  using expr = variant< lit<void>, lit<int>, lit<double>, lit<bool>, lit<std::string>,
						var, abs, app, let >;
  
  template<class T>
  struct lit {
	T value;
  };

  struct var : symbol {
	using symbol::symbol;
  };

  template<> struct lit<void> { };

  struct abs {
	vec<var> args;
	ref<expr> body;
  };

  // TODO vec for args
  struct app {
	ref<expr> func;
	vec<expr> args;
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


  // complete syntax TODO types ?
  using node = variant<expr, def>;
  
}





#endif
