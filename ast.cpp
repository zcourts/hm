#include "ast.hpp"

#include <string>
#include <algorithm>

// debug
#include <iostream>

namespace ast {


  // TODO this should actually move to syntax.hpp/cpp
  syntax_error::syntax_error(const std::string& what)
	: std::runtime_error("syntax error: " + what) { };


  static struct {
	std::string func = "func";
	std::string let = "let";
  } keyword;


  static ast::expr transform_let(const sexpr::list& e);
  static ast::expr transform_func(const sexpr::list& e);
  
  struct transform_expr {

	ast::expr operator()(const int& self) const {
	  return ast::lit<int>{self};
	}

	ast::expr operator()(const symbol& self) const {
	  return ast::var{self};
	}

	ast::expr operator()(const sexpr::list& self) const {

	  if( self.empty() ) {
		return ast::lit<void>();
	  }

	  // handle special forms
	  if( self[0].is<symbol>() ) {
		const symbol s = self[0].as<symbol>();
	  
		if( s == keyword.let ) {
		  return transform_let(self);
		}

		if( s == keyword.func ) {
		  return transform_func(self);
		}

		// TODO quote/lists
	  }

	  // function application
	  vec<ast::expr> terms;

	  // transform each expr
	  std::transform(self.begin(), self.end(),
					 std::back_inserter(terms), transform);
	
	  // first must be a lambda or a variable
	  switch(terms[0].type()) {
	  case ast::expr::type<ast::func>():
	  case ast::expr::type<ast::var>():	  
		break;
	  default:
		throw std::runtime_error("error in function application");
	  }

	  if( terms.size() > 2 ) throw std::runtime_error("multiple function arguments not handled");

	  // TODO unit type for nullary functions ?
	  // TODO ref in func/args 
	  return ast::call{ shared(terms[0]), shared(terms[1]) };
	}
  
  };


  static ast::expr transform_let(const sexpr::list& e) {

	if( e.size() != 4 ) {
	  throw syntax_error("variable definition");
	}

	assert( e[0].is<symbol>() && e[0].as<symbol>() == keyword.let );

	// identifier
	// TODO we should also exclude keywords
	if( !e[1].is<symbol>() ) {
	  throw syntax_error("variable name must be a symbol");
	}

	ast::let res;
	res.id = ast::var{e[1].as<symbol>()};

	// definition
	res.def = shared( transform(e[2]));

	// body
	res.body = shared( transform(e[3]));
  
	return res;
  }


  static ast::expr transform_func(const sexpr::list& e) {

	if( e.size() != 3 ) {
	  throw syntax_error("function definition");
	}

	assert( e[0].is<symbol>() && e[0].as<symbol>() == keyword.func );

	// args check
	if( !e[1].is<sexpr::list>() ) throw syntax_error("argument list expected");

	const sexpr::list& args = e[1].as<sexpr::list>();
	if(args.size() > 1) {
	  std::cerr << "warning: multiple arguments not handled yet" << std::endl;
	}
  
	ast::func res;

	for(const auto& a : e[1].as<sexpr::list>() ) {
	  if( !a.is<symbol>() ) throw syntax_error("arguments must be symbols");

	  // TODO push_back
	  res.args = ast::var{a.as<symbol>() };
	}
  
	// body
	res.body = shared(transform(e[2]));
  
	return res;
  }

  ast::expr transform(const sexpr::expr& e) {
	return e.apply<ast::expr>( transform_expr() );
  }


}
