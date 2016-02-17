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
  struct cond;					// conditionals
  struct expr;					// expression
  
  template<class T>
  struct lit {
	T value;
  };

  // void is not inhabited
  template<> struct lit<void> { };

  
  // fixpoint combinator
  struct fix_type;

  // io monad bind/return
  struct bind_type;
  struct return_type;

  // conditionals
  struct if_type;
  
  template<> struct lit<fix_type> { };
  template<> struct lit<bind_type> { };
  template<> struct lit<return_type> { };
  template<> struct lit<if_type> { };  
  
  
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

	// adapted value for typechecking recursive functions 
	ref<expr> rec() const;
  };


  struct seq {
	
	// haskell's id <- def
	struct with;
	struct ret;
	
	struct term; 
	
	vec<term> terms;
	
	// convert to monadic style
	expr monad() const;
	
  };

  
  struct cond {
	ref<expr> test;
	ref<expr> conseq;
	ref<expr> alt;

	// equivalent application: (if test conseq alt)
	expr app() const;
  };
  
 
  // TODO lists
  struct expr : variant<
	
	// literals
	lit<void>, lit<int>, lit<double>, lit<bool>, lit<std::string>, 

	// helper combinators for type-checking
	lit<fix_type>, lit<bind_type>, lit<return_type>, lit<if_type>,
	
	// core expressions
	var, abs, app, let,
	
	// sequences
	seq,

	// conditions
	cond>

  {
	using variant::variant;
  };


  struct seq::with {
	var id;
	expr def;
	
	with(const var& id, const expr& e = {} );
  };

  
  struct seq::term : variant<expr, with> {
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
