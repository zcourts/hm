#include "parse.hpp"

#include <sstream>

#include <boost/spirit/include/qi.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/object/construct.hpp>
#include <boost/phoenix/bind/bind_function.hpp>


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

  ss << "on line " << line << " near: ";

  for(curr = line_start; curr != line_end; ++curr) {
	ss << *curr;
  }

  ss << std::endl;
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


// steal yo
static sexpr::list quote_expr(sexpr::expr& expr) {
  return { {symbol("quote"), std::move(expr)} };
}

static sexpr::list unquote_expr(sexpr::expr& expr) {
  return { {symbol("unquote"), std::move(expr)} };
}


static sexpr::list quasiquote_expr(sexpr::expr& expr) {
  return { {symbol("quasiquote"), std::move(expr)} };
}


template<class Iterator>
static sexpr::list parse(Iterator first, Iterator last) {
  using namespace boost::spirit;

  using boost::phoenix::function;
  using boost::phoenix::construct;  
  using boost::phoenix::val;

  using skip_type = qi::rule<Iterator>;
  
  skip_type comment = qi::lexeme[ qi::char_(';') >> *(qi::char_ - qi::eol) >> qi::eol];
  skip_type skip = qi::space | comment;
  
  qi::rule<Iterator, skip_type, sexpr::expr()> atom, symbol, integer, real, string, boolean, t, f, expr;
  qi::rule<Iterator, skip_type, sexpr::list()> list, seq, quote, quasiquote, unquote, start;
  
  integer = qi::int_;

  real = qi::real_parser<sexpr::real, qi::strict_real_policies<sexpr::real>>();
  
  t = qi::string("true") [ _val = val(true) ];
  f = qi::string("false")[ _val = val(false)];
  
  boolean = t | f;
  
  // TODO escape characters
  auto db_quote = qi::char_('"');  
  string = qi::lexeme[db_quote >> qi::as_string[*(~db_quote)][_val = _1] >> db_quote];

  const std::string exclude = std::string(" ();\"\x01-\x1f\x7f") + '\0';  
  symbol = (as_string[ lexeme[+(~qi::char_(exclude))] ])[ _val = construct<::symbol>(_1) ];

  quote = '\'' >> expr[ _val = bind(&quote_expr, _1) ];

  quasiquote = '`' >> expr[ _val = bind(&quasiquote_expr, _1) ];    
  unquote = ',' >> expr[ _val = bind(&unquote_expr, _1) ];  
  
  seq = (*expr);

  list = '(' >> seq > ')';

  atom = real | integer | boolean | string | symbol;
  expr = quote | quasiquote | unquote | list | atom;
  
  start = seq;
  
  std::stringstream ss;

  qi::on_error<qi::fail>
	(list,
	 ss
	 << val("unmatched parenthesis ")
	 << function< reporter >{}( val(first), _2, _3)   // iterators to error-pos, end,
	 );

  sexpr::list res;
  Iterator it = first;

  const bool ret = phrase_parse(it, last, start, skip, res );
  const bool ok = ret && (it == last);

  if( !ok ) throw parse_error(ss.str());

  return res;
  
}


sexpr::list parse(std::istream& in) {
  in >> std::noskipws;
  boost::spirit::istream_iterator first(in), last;
  return parse(first, last);
}
