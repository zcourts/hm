#ifndef AST_HPP
#define AST_HPP

#include "common.hpp"
#include "variant.hpp"

#include <string>

// syntax tree
namespace ast {

  // expressions
  template<class T> struct lit;	// literals
  struct var;					// variables
  struct abs;					// lambda-abstractions
  struct app;					// applications
  struct let;					// local defintions
  struct seq;                   // sequence of computations
  struct expr;					// expression
  
  template<class T>
  struct lit {
	T value;
  };

  // fixpoint combinator
  struct fixpoint;

  template<> struct lit<fixpoint> { };
  template<> struct lit<void> { };
  
  struct var : symbol {
	using symbol::symbol;
  };


  struct abs {
	vec<var> args;
	ref<expr> body;
  };


  struct app {
	ref<expr> func;
	vec<expr> args;
  };

  struct let {
	var id;
	ref<expr> value;
	ref<expr> body;
  };

  // do we really want an ast node ? or is a lambda enough ?
  struct seq {
    vec<expr> exprs;
  };
  
  
  // TODO lists
  struct expr : variant< lit<void>, lit<int>, lit<double>, lit<bool>, lit<std::string>,
                         lit<fixpoint>,
						 var, abs, app, let,
                         seq> {
	using variant::variant;
  };
  
  
  
  // variable definitions
  struct def {
	var id;
	ref<expr> value;
  };


  // datatype definitions
  struct type {
    
	var id;
	vec<var> args;

	struct constructor {
	  var id;
	  vec<var> args;
	};
	
	// list of type constructors (???)
	vec< constructor > def;
  };


  // complete syntax
  using node = variant<expr, def, type>;
  
}





#endif
