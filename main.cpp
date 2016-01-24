
#include <iostream>

#include <vector>
#include <map>
#include <set>
#include <memory>

#include "variant.hpp"

#include "common.hpp"
#include "variant.hpp"

#include "sexpr.hpp"
#include "ast.hpp"
#include "type.hpp"

#include <boost/pending/disjoint_sets.hpp>
#include <boost/spirit/include/qi.hpp>

#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/object/construct.hpp>
#include <boost/phoenix/bind/bind_function.hpp>


#include "hindley_milner.hpp"



using context = std::map< ast::var, type::poly >;




template<class Iterator>
  static std::string error_report(Iterator first, Iterator last, Iterator err) {

	Iterator line_start = first;
	unsigned line = 1;

	Iterator curr;
	for(curr = first; curr != err; ++curr) {
	  if(*curr == '\n') {
		line_start = curr;
		++line_start;
		++line;
	  }
	}

	for(;curr != last; ++curr) {
	  if( *curr == '\n') break;
	}
	
	Iterator line_end = curr;

	std::stringstream ss;

	ss << "on line " << line << ": ";

	for(curr = line_start; curr != line_end; ++curr) {
	  ss << *curr;
	}

	return ss.str();
  }


struct reporter {
	
  template<class Arg1, class Arg2, class Arg3>
  struct result {
	typedef std::string type;
  };

  template<class Arg1, class Arg2, class Arg3>
  typename result<Arg1, Arg2, Arg3>::type operator()(Arg1 arg1, Arg2 arg2, Arg3 arg3) const {
	return error_report(arg1, arg2, arg3);
  }
};


static std::set<std::string> symbol_table;
static symbol make_symbol(const std::string& s) {
  return symbol_table.insert(s).first->c_str();
}


template<class Iterator>
static sexpr::list parse(Iterator first, Iterator last) {
  using namespace boost::spirit::qi;
  using boost::phoenix::function;
  using boost::phoenix::construct;  
  using boost::phoenix::val;

  auto comment = lexeme[char_('#') >> *(char_ - eol) >> eol];
  auto skip = space | comment;
  
  using skip_type = decltype(skip);

  rule<Iterator, skip_type, sexpr::expr()> ratom,  rsymbol, rint, rexpr;
  rule<Iterator, skip_type, sexpr::list()> rlist, rseq, rstart;
  
  rint = int_;

  const std::string exclude = std::string(" ();\"\x01-\x1f\x7f") + '\0';  
  rsymbol = (as_string[ lexeme[+(~char_(exclude))] ])[ _val = bind(&make_symbol, _1) ];
  
  rseq = (*rexpr)[_val = _1];

  rlist = '(' >> rseq > ')';

  ratom = rint | rsymbol;
  rexpr = ratom | rlist;
  
  rstart = rseq;
  
  std::stringstream ss;

  on_error<fail>
        (rlist,
		 ss
		 << val("unmatched parenthesis ")
		 << function< reporter >{}( val(first), _2, _3)   // iterators to error-pos, end,
		 << std::endl
		 );

  sexpr::list res;
  Iterator it = first;

  const bool ret = phrase_parse(it, last, rstart, skip, res );
  const bool ok = ret && (it == last);

  if( !ok ) {
	std::cout << ss.str() << std::endl;
	throw std::runtime_error("parse error");
  }
  return res;
  
}

static struct {
  std::string func = "func";
  std::string let = "let";
} keyword;


struct syntax_error : std::runtime_error {

  syntax_error(const std::string& what)
	: std::runtime_error("syntax error: " + what) { };
  
};

static ast::expr transform_let(const sexpr::list& e);
static ast::expr transform_func(const sexpr::list& e);

ast::expr transform(const sexpr::expr& e);


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

// ast::expr transform(sexpr::expr e) { 

//   switch(e.type()) {
//   case sexpr::expr::type<int>():
// 	return ast::lit<int>{e.as<int>()};
//   case sexpr::expr::type<symbol>():
// 	return ast::var{e.as<symbol>()};
//   case sexpr::expr::type<sexpr::list>(): {

// 	auto& self = e.as<sexpr::list>();

// 	if( self.empty() ) {
// 	  return ast::lit<void>();
// 	}

// 	// handle special forms
// 	if( self[0].is<symbol>() ) {
// 	  const symbol s = self[0].as<symbol>();
	  
// 	  if( s == keyword.let ) {
// 		return transform_let(self);
// 	  }

// 	  if( s == keyword.func ) {
// 		return transform_func(self);
// 	  }

// 	  // TODO quote/lists
// 	}

// 	// function application
// 	vec<ast::expr> terms;

// 	// transform each expr
// 	std::transform(self.begin(), self.end(),
// 				   std::back_inserter(terms), transform);
	
// 	// first must be a lambda or a variable
// 	switch(terms[0].type()) {
// 	case ast::expr::type<ast::func>():
// 	case ast::expr::type<ast::var>():	  
// 	  break;
// 	default:
// 	  throw std::runtime_error("error in function application");
// 	}

// 	if( terms.size() > 2 ) throw std::runtime_error("multiple function arguments not handled");

// 	// TODO unit type for nullary functions ?
// 	// TODO ref in func/args 
// 	ast::call res = { std::make_shared<ast::expr>(terms[0]),
// 					  std::make_shared<ast::expr>(terms[1]) };
// 	return res;
//   }
//   }
  
//   throw std::logic_error("derp");
// }




int main() {

  std::string data;
  std::cout << "enter string:" << std::endl;
  std::getline(std::cin, data);

  sexpr::list prog = parse(data.begin(), data.end());
  
  context c;
  for(const sexpr::expr& s : prog ) {
	ast::expr e = transform( s );
	type::poly p = hindley_milner(c, e);
	std::cout << s << " :: " << p << std::endl;	
  }

  // debug();
  return 0;
}
