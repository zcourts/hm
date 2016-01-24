#include "sexpr.hpp"

#include <sstream>

#include <boost/spirit/include/qi.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/object/construct.hpp>
#include <boost/phoenix/bind/bind_function.hpp>

// TODO symbol.hpp
#include <set>

namespace sexpr {

  std::ostream& operator<<(std::ostream& out, const list& x) {
	out << '(';
	
	bool first = true;
	for(const auto& xi : x) {
	  if(!first) {
		out << ' ';
	  } else {
		first = false;
	  }
	  out << xi;
	}
	out << ')';
	
	return out;
  }


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


  sexpr::list parse(std::istream& in) {
	in >> std::noskipws;
	boost::spirit::istream_iterator first(in), last;
	return parse(first, last);
  }

  
}
