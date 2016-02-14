#include "syntax.hpp"

#include <algorithm>
#include <set>

// debug
#include <iostream>


// TODO this should actually move to syntax.hpp/cpp
// syntax_error::syntax_error(const std::string& what)
//   : std::runtime_error("syntax error: " + what) { };


static struct {
  std::string abs = "lambda";
  std::string let = "let";
  std::string def = "def";
  
  std::string type = "type";
  std::string do_ = "do";
  std::string with = "with";
  
  std::set<std::string> all = {abs, let, def, type, do_, with};
  
} keyword;


// core expressions
static ast::expr transform_expr(const sexpr::expr& e);
static ast::expr transform_let(const sexpr::list& e);
static ast::expr transform_abs(const sexpr::list& e);
static ast::var transform_var(const sexpr::expr& e);

// 
static ast::expr transform_do(const sexpr::list& e);


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

      if( s == keyword.do_ ) {
		return transform_do(self);
	  }
      
	  // TODO quote/lists
	}

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
  ast::expr what = transform_expr(e[2]);
  
  // recursive functions by default
  if( what.is<ast::abs>())   {
    static ast::lit<ast::fixpoint> fix;
    
    // (fix (lambda (id) body))
    ast::app app;
    app.func = shared<ast::expr>( fix );
    
    // (lambda (id) body)
    ast::abs wrap;
    wrap.args.push_back(res.id);
    wrap.body = shared<ast::expr>(what);

    app.args.push_back(wrap);
    
    what = app;
  }

  // definition
  res.value = shared<ast::expr>( what );

  // body
  res.body = shared<ast::expr>( transform_expr(e[3]));
  
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

template<class Iterator>
static ast::expr transform_do(Iterator first, Iterator last) {


  if( first + 1 == last ) {
	// base case: just replace expression
	return transform_expr(*first);
	
  } else {
	// bind

	// TODO FIXME optimize symbol lookup + generate unique symbol ? or prevent using "_"
	ast::var id = {"_"};
	ast::expr value; 
	
	if( first->template is<sexpr::list>() ) {

	  
	  auto& self = first->template as<sexpr::list>();

	  if( self.empty() ) throw syntax_error("bad do syntax");

	  // with
	  if( self[0].template is<symbol>() && self[0].template as<symbol>() == keyword.with ) {

		if( self.size() != 2 && self.size() != 3) throw syntax_error("bad with syntax");
		if( !self[1].template is<symbol>() )  throw syntax_error("symbol expected for with-binding");

		// TODO FIXME optimize lookup
		id = {self[1].template as<symbol>().name()};
		value = self.size() == 3 ? transform_expr( self[2] ) : id;
	  }
	}

	// non-with binding: just bind current expr 
	if( !value ) {
	  value = transform_expr(*first);
	}
	
	ast::abs lambda = { {id}, shared<ast::expr>(transform_do(first + 1, last)) };
	ast::app call = { shared<ast::expr>( ast::var("bind") ),
					  {value, lambda} };

	return call;
  }
  
}

static ast::expr transform_do(const sexpr::list& e) {

  ast::expr res = transform_do(e.begin() + 1, e.end());
  
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
