#include "syntax.hpp"

#include <algorithm>
#include <set>

// debug
#include <iostream>


// TODO this should actually move to syntax.hpp/cpp
// syntax_error::syntax_error(const std::string& what)
//   : std::runtime_error("syntax error: " + what) { };


static struct {
  std::string abs = "func";
  std::string let = "let";
  std::string def = "def";

  std::set<std::string> all = {abs, let, def};
  
} keyword;


static ast::expr transform_expr(const sexpr::expr& e);
static ast::expr transform_let(const sexpr::list& e);
static ast::expr transform_abs(const sexpr::list& e);
static ast::var transform_var(const sexpr::expr& e);


struct match_expr {

  ast::expr operator()(const int& self) const {
	return ast::lit<int>{self};
  }

  ast::expr operator()(const symbol& self) const {
	return transform_var(self);
  }

  ast::expr operator()(const sexpr::list& self) const {

	if( self.empty() ) {
	  return ast::lit<void>();
	}

	// handle special forms
	if( self[0].is<symbol>() ) {
	  auto& s = self[0].as<symbol>();
	  
	  if( s == keyword.let ) {
		return transform_let(self);
	  }

	  if( s == keyword.abs ) {
		return transform_abs(self);
	  }

	  // TODO quote/lists
	}

	// function application
	vec<ast::expr> terms;

	// transform each expr
	std::transform(self.begin(), self.end(),
				   std::back_inserter(terms), transform_expr);
	
	// first must be a lambda or a variable
	if( terms[0].is<ast::abs>() && terms[0].is<ast::var>() ) {
	  // good
	} else {
	  throw std::runtime_error("error in function application");
	}

	if( terms.size() > 2 ) throw std::runtime_error("multiple function arguments not handled");

	// TODO unit type for nullary functions ?
	// TODO ref in func/args 
	return ast::app{ shared(terms[0]), shared(terms[1]) };
  }
  
};


static ast::expr transform_let(const sexpr::list& e) {

  assert(!e.empty() && e[0].is<symbol>() && e[0].as<symbol>() == keyword.let );
  
  if( e.size() != 4 ) {
	throw syntax_error("local variable definition");
  }
  
  // identifier
  ast::let res;
  res.id = transform_var(e[1]);

  // definition
  res.value = shared( transform_expr(e[2]));

  // body
  res.body = shared( transform_expr(e[3]));
  
  return res;
}


static ast::expr transform_abs(const sexpr::list& e) {
  assert( !e.empty() && e[0].is<symbol>() && e[0].as<symbol>() == keyword.abs );
  
  if( e.size() != 3 ) {
	throw syntax_error("function expression");
  }
  
  // args check
  if( !e[1].is<sexpr::list>() ) throw syntax_error("argument list expected");

  const sexpr::list& args = e[1].as<sexpr::list>();
  if(args.size() > 1) {
	std::cerr << "warning: multiple arguments not handled yet" << std::endl;
  }
  
  ast::abs res;

  for(const auto& a : e[1].as<sexpr::list>() ) {
	if( !a.is<symbol>() ) throw syntax_error("arguments must be symbols");

	// TODO push_back
	res.args = ast::var{a.as<symbol>() };
  }
  
  // body
  res.body = shared(transform_expr(e[2]));
  
  return res;
}

ast::expr transform_expr(const sexpr::expr& e) {
  return e.apply<ast::expr>( match_expr() );
}


ast::def transform_def(const sexpr::list& list) {
  assert( !list.empty() && list[0].is<symbol>() && list[0].as<symbol>() == keyword.def );
  
  if( list.size() != 3 ) {
	throw syntax_error("toplevel variable definition");
  }

  ast::def res;

  res.id = transform_var( list[1] );
  res.value = shared( transform_expr( list[2] ) );

  return res;
}


ast::var transform_var(const sexpr::expr& e) {
  if( !e.is<symbol>() ) {
	throw syntax_error("variable name must be a symbol");
  }

  auto& s = e.as<symbol>();
  
  auto it = keyword.all.find( s );
  if(it != keyword.all.end() ) {
	throw syntax_error( *it + " is a reserved keyword");
  }

  return {s};
}


// toplevel
ast::node transform(const sexpr::expr& e) {

  // detect definitions, types, etc
  if( e.is<sexpr::list>() ) {
	auto& list = e.as<sexpr::list>();
	
	if( !list.empty() && list[0].is<symbol>() ) {
	  auto& s = list[0].as<symbol>();

	  if( s == keyword.def ) {
		return transform_def(list);
	  }
	}
	
  }

  // default: expressions
  return transform_expr(e);
}
