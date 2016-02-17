#include "syntax.hpp"

#include <algorithm>
#include <set>

// debug
#include <iostream>


// TODO this should actually move to syntax.hpp/cpp
// syntax_error::syntax_error(const std::string& what)
//   : std::runtime_error("syntax error: " + what) { };


static struct {
  std::string abs = "fn";
  std::string let = "let";
  std::string def = "def";
  
  std::string type = "type";
  std::string seq = "do";
  std::string with = "with";
  std::string ret = "return";	// TODO this one is not exactly a keyword, is it ?
  std::string cond = "if";
  
  std::set<std::string> all = {abs, let, def, type, seq, with, ret, cond};
  
} keyword;


// core expressions
static ast::expr transform_expr(const sexpr::expr& e);


static ast::let transform_let(const sexpr::list& e);
static ast::abs transform_abs(const sexpr::list& e);
static ast::app transform_app(const sexpr::list& e);
static ast::var transform_var(const sexpr::expr& e);


// sequences and conditionals
static ast::seq transform_seq(const sexpr::list& e);
static ast::cond transform_cond(const sexpr::list& e);


struct match_expr {

  ast::expr operator()(const symbol& self) const {
	return transform_var(self);
  }


  ast::expr operator()(const sexpr::integer& self) const {
	return ast::lit<int>{self};
  }

  ast::expr operator()(const sexpr::real& self) const {
	return ast::lit<double>{self};
  }  

  ast::expr operator()(const bool& self) const {
	return ast::lit<bool>{self};
  }

  ast::expr operator()(const std::string& self) const {
	return ast::lit<std::string>{self};
  }

  
  
  template<class T>
  ast::expr operator()(const T& ) const {
	throw std::logic_error("type not implemented: " + std::string( typeid(T).name()) );
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

      if( s == keyword.seq ) {
		return transform_seq(self);
	  }

	  if( s == keyword.cond ) {
		return transform_cond(self);
	  }
	  
	  // TODO quote/lists ?
	}

	// function application
	return transform_app(self);

  }


  
};


static ast::app transform_app(const sexpr::list& self) {

  // function application
  vec<ast::expr> terms;

  // transform each expr
  std::transform(self.begin(), self.end(),
				 std::back_inserter(terms), transform_expr);
	
  // first must be a lambda or a variable
  if( terms[0].is<ast::abs>() || terms[0].is<ast::var>() || terms[0].is<ast::app>() ) {
	// good
  } else {
	throw syntax_error("expected lambda or variable in function application");
  }

  vec<ast::expr> args(terms.begin() + 1, terms.end());
  return ast::app{ shared<ast::expr>(terms[0]), args };
}


static ast::let transform_let(const sexpr::list& e) {

  assert(!e.empty() && e[0].is<symbol>() && e[0].as<symbol>() == keyword.let );
  
  if( e.size() != 4 ) {
	throw syntax_error("local variable definition");
  }
  
  // identifier
  ast::let res;
  res.id = transform_var(e[1]);

  // definition
  res.value = shared<ast::expr>(  transform_expr(e[2]) );

  // body
  res.body = shared<ast::expr>( transform_expr(e[3]));
  
  return res;
}


static ast::abs transform_abs(const sexpr::list& e) {
  assert(!e.empty() && e[0].is<symbol>() && e[0].as<symbol>() == keyword.abs );
  
  if( e.size() != 3 ) {
	throw syntax_error("function expression");
  }
  
  // args check
  if( !e[1].is<sexpr::list>() ) throw syntax_error("argument list expected");

  const sexpr::list& args = e[1].as<sexpr::list>();
  
  ast::abs res;

  res.args.reserve( args.size() );
  
  for(const auto& a : args ) {
	if( !a.is<symbol>() ) throw syntax_error("arguments must be symbols");
	
	res.args.push_back( {a.as<symbol>().name() } );
  }
  
  // body
  res.body = shared<ast::expr>(transform_expr(e[2]));
  
  return res;
}



static ast::cond transform_cond(const sexpr::list& self) {
  assert(!self.empty() && self[0].is<symbol>() && self[0].as<symbol>() == keyword.cond );
  
  if(self.size() != 4) throw syntax_error("bad 'if' syntax"); 

  return { shared<ast::expr>(transform_expr(self[1])),
	  shared<ast::expr>(transform_expr(self[2])),
	  shared<ast::expr>(transform_expr(self[3])) };
}

static ast::seq transform_seq(const sexpr::list& e) {

  ast::seq res;
  
  for(auto it = e.begin() + 1, end = e.end(); it != end; ++it) {

	ast::seq::term term;
	
	if(it->is<sexpr::list>() ) {

	  auto& self = it->as<sexpr::list>();

	  // with 
	  if( !self.empty() && self[0] == keyword.with ) {
		if(term && it + 1 == end) throw syntax_error("'with' cannot end a sequence");
		
		if( self.size() != 2 && self.size() != 3) throw syntax_error("bad 'with' syntax");
		ast::var id = transform_var( self[1] );
		
		ast::seq::with with(id, self.size() == 2 ? id : transform_expr(self[2]));
		term = with;

	  }

	  // return 
	  if( !self.empty() && self[0] == keyword.ret ) {
		if(term && it + 1 != end) throw syntax_error("'return' must end a sequence");
		
		ast::app app = transform_app(self);
		
		static ast::lit<ast::return_type> ret;
		app.func = shared<ast::expr>(ret);
		
		term = ast::expr(app);
	  }
	}
	
	// regular expression
	if(!term) {
	  term = transform_expr(*it);
	}

	res.terms.push_back(term);
  }
  
  return res;
}


static ast::expr transform_expr(const sexpr::expr& e) {
  return e.apply<ast::expr>( match_expr() );
}


static ast::def transform_def(const sexpr::list& list) {
  assert( !list.empty() && list[0].is<symbol>() && list[0].as<symbol>() == keyword.def );
  
  if( list.size() != 3 ) {
	throw syntax_error("toplevel variable definition");
  }

  ast::def res;

  res.id = transform_var( list[1] );
  res.value = shared<ast::expr>( transform_expr( list[2] ) );

  return res;
}


static ast::var transform_var(const sexpr::expr& e) {
  if( !e.is<symbol>() ) {
	throw syntax_error("variable name must be a symbol");
  }

  auto& s = e.as<symbol>();
  
  auto it = keyword.all.find( s.name() );
  if(it != keyword.all.end() ) {
	throw syntax_error( *it + " is a reserved keyword");
  }

  return {s.name()};
}




struct transform_constructor {

  ast::type::constructor operator()(const symbol& self) const {
	return  { self.name(), {} };
  }

  ast::type::constructor operator()(const sexpr::list& self) const {

	if(self.size() < 2 || !self[0].is<symbol>() )  {
	  throw syntax_error("type constructor syntax");
	}

	ast::type::constructor res;
	res.id = self[0].as<symbol>().name();

	for(unsigned i = 1, n = self.size(); i < n; ++i) {
	  if(!self[i].is<symbol>()) {
		throw syntax_error("expected symbol for type constructor argument");
	  }
	  res.args.push_back( self[i].as<symbol>().name() );
	}
	
	return res;
  }

  template<class T>
  ast::type::constructor operator()(const T& ) const {
	throw syntax_error("type constructor syntax");
  }
  
};



// data
static ast::type transform_type(const sexpr::list& list) {
  assert( !list.empty() && list[0].is<symbol>() && list[0].as<symbol>() == keyword.type);

  // TODO split declaration/definition
  if( list.size() < 3 ) {
	throw syntax_error("datatype syntax");
  }
  
  ast::type res;

  if( list[1].is<symbol>() ) {
	res.id = list[1].as<symbol>().name();
  } else if( list[1].is<sexpr::list>() ) {
	auto& self = list[1].as<sexpr::list>();

	if( self.size() < 2 ) {
	  throw syntax_error("parametrized datatype syntax");
	}

	if( !self[0].is<symbol>() ) {
	  throw syntax_error("parametrized datatype declaration syntax");
	}

	// datatype identifier
	res.id = self[0].as<symbol>().name();

	// datatype args
	for(unsigned i = 1, n = self.size(); i < n; ++i) {
	  if(!self[i].is<symbol>()) {
		throw syntax_error("expected symbol for type parameter");
	  }

	  res.args.push_back( self[i].as<symbol>().name() );
	}
	
  } else {
	throw syntax_error("datatype declaration syntax");
  }


  // datatype id/args are ok, now with type constructors
  for(unsigned i = 2; i < list.size(); ++i) {
	res.def.push_back( list[i].apply< ast::type::constructor >( transform_constructor() ) );
  }

  return res;
}


ast::expr ast::cond::app() const {

  static lit<if_type> if_;

  ast::app res;

  res.func = shared<ast::expr>(if_);
  res.args.push_back( *test );
  res.args.push_back( *conseq );
  res.args.push_back( *alt );

  return res;
}



// toplevel
ast::node transform(const sexpr::expr& e) {

  // detect toplevel definitions, types, etc
  if( e.is<sexpr::list>() ) {
	auto& list = e.as<sexpr::list>();
	
	if( !list.empty() && list[0].is<symbol>() ) {
	  auto& s = list[0].as<symbol>();

	  if( s == keyword.def ) {
		return transform_def(list);
	  }

	  if( s == keyword.type ) {
		return transform_type(list);
	  }
	}
	
  }

  // default: expressions
  return transform_expr(e);
}
