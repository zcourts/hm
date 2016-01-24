
#include <iostream>

#include <vector>
#include <map>
#include <set>
#include <memory>

#include "variant.hpp"

#include <boost/pending/disjoint_sets.hpp>
#include <boost/spirit/include/qi.hpp>

#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/object/construct.hpp>
#include <boost/phoenix/bind/bind_function.hpp>

using symbol = const char*;

template<class T>
using ref = std::shared_ptr<T>;

template<class T>
using vec = std::vector<T>;

// type-checking
namespace type {

  struct lit;
  struct var;
  struct app;

  using mono = variant< app, var, lit >;
  
  struct lit {
	symbol name;

	bool operator<(const lit& other) const { return name < other.name; }
	bool operator==(const lit& other) const { return name == other.name; }


	friend std::ostream& operator<< (std::ostream& out, const lit& t) {
	  return out << t.name;
	}
  
  };
  
  struct var {
	var(unsigned index = total++) : index(index) { }
	unsigned index;
	
	static unsigned total;

	bool operator<(const var& other) const { return index < other.index; }
	bool operator==(const var& other) const { return index == other.index; }
	bool operator!=(const var& other) const { return index != other.index; }	

	friend std::ostream& operator<< (std::ostream& out, const var& t) {
	  if( ('a' + t.index) < 'z' ) {
		return out << (char)('a' + t.index);
	  } else {
		return out << "type-" << t.index;
	  }
	}
	
  };

  unsigned var::total = 0;
  
  struct app {
	app(ref<mono> from, ref<mono> to) : from(from), to(to) { }
	ref<mono> from, to;

	bool operator<(const app& other) const {
	  return  (*from < *other.from) || ((*from == *other.from) && (*to < *other.to));
	}

	bool operator==(const app& other) const {
	  return  (*from == *other.from) && (*to == *other.to);
	}	
	

	friend std::ostream& operator<< (std::ostream& out, const app& t) {

	  const bool is_from_app = t.from->is< app >();
	  const bool is_to_app = t.to->is< app >();
	  
	  return
		out << (is_from_app ? "(" : "") << *t.from << (is_from_app ? ")" : "")
		  << " -> "
		// TODO maybe we don't have to parentize on the right
		  << (is_to_app ? "(" : "") << *t.to << (is_to_app ? ")" : "")
		;
	}

  };

  // TODO other type constructors, add func

  // TODO univ/exist ?
  struct forall;
  
  using poly = variant< mono, forall >;
  
  struct forall {
	vec<var> args;
	ref<poly> body;


	bool operator==(const forall& other) const {

	  if( args.size() != other.args.size() ) return false;
	  
	  for(unsigned i = 0, n = args.size(); i < n; ++i) {
		if(args[i] != other.args[i]) return false;
	  }

	  return *body == *other.body;
	}
	
	friend std::ostream& operator<< (std::ostream& out, const forall& t) {
	  out << "forall";

	  for( const auto& c : t.args ) {
		out << " " << c;
	  }
	  
	  return out<< ". " << *t.body;
	}
	
  };

  


}



// syntax tree
namespace ast {
  
  template<class T> struct lit;
  struct var;
  struct func;
  struct call;
  struct let;

  using expr = variant< lit<void>, lit<int>, var, func, call, let >;
  
  template<class T>
  struct lit {
	T value;
  };

  template<> struct lit<void> { };
  
  struct var {
	symbol name;
	bool operator<(const var& other) const { return name < other.name; }
  };

  // TODO vec
  struct func {
	var args;
	ref<expr> body;
  };

  struct call {
	ref<expr> func;
	ref<expr> args;
  };

  struct let {
	var id;
	ref<expr> def;
	ref<expr> body;
  };
  
}




using context = std::map< ast::var, type::poly >;

using rank_type =  std::map< type::mono, int >;
using parent_type =  std::map< type::mono, type::mono >;

rank_type rank;
parent_type parent;

template<class T> using pm = boost::associative_property_map<T>;

pm<rank_type> r(rank);
pm<parent_type> p(parent);

boost::disjoint_sets< pm<rank_type>, pm<parent_type> > dsets(r, p);

void debug() {

  std::cout << "rank:";
  for( auto& c : rank ) {
	std::cout << "\t" << c.first << ": " << c.second << std::endl;
  }
  
  std::cout << "parent:";
  for( auto& c : parent ) {
  	std::cout << "\t" << c.first << ": " << c.second <<  std::endl;
  }

 
}

static void unify(type::mono a, type::mono b) {

  std::cout << "unifying: " << a << " with: " << b << std::endl;
  // debug();
  
  using namespace type;
  
  // find representative for each type
  
  // std::cout << "test parent: " << get(p, a) << std::endl;
  
  a = dsets.find_set(a);
  b = dsets.find_set(b);
  
  // std::cout << "classes: " << a << ", " << b << std::endl;
  
  if( a.type() == mono::type< app >() && b.type() == mono::type< app >() ) {
	// TODO generalize to other type constructors, with arity check

	// unify each parameters
	unify( *a.as< app >().from, *b.as< app >().from);
	unify( *a.as< app >().to,   *b.as< app >().to);
	
  } else if( a.type() == mono::type<var>() || b.type() == mono::type<var>() ) {
	// merge a and b classes (representative is right-hand side for link)
	// std::cout << "merging classes" << std::endl;

	if( a.type() == mono::type<var>() ) {
	  dsets.link(a, b);
	} else {
	  dsets.link(b, a);
	}

	// TODO we should make sure the term is the representative and not
	// the variable

  } else {
	throw std::runtime_error("types do not match lol");
  }
};


using substitution = std::map<type::var, type::var>;



static type::mono represent(type::mono t) {
  
  using namespace type;

  mono res = dsets.find_set(t);

  if( res.is<app>() ) {
	auto& self = res.as<app>();
	
	res = app( std::make_shared<mono>( represent(*self.from)),
			   std::make_shared<mono>( represent(*self.to)) );
  }  

  // std::cout << "representing " << t << " by " << res << std::endl;
  
  return res;
}



struct specialize {
  
  // sub-dispatch for monotypes
  struct monotype {

	// replace variables given substitution
	type::mono operator()(const type::var& self, const specialize& parent, substitution&& s) const {
	  auto it = s.find(self);

	  if( it != s.end() ) return it->second;
	  else return self;
	}

	type::mono operator()(const type::app& self, const specialize& parent, substitution&& s) const {
	  // TODO should we make new substitutions ?
	  // TODO should we dsets.find first ?

	  // specialize from/to and get representants
	  type::mono sfrom = self.from->apply<type::mono>(parent, substitution(s) );
	  type::mono sto = self.to->apply<type::mono>(parent, substitution(s) );	  
	  
	  type::app res = {std::make_shared<type::mono>( sfrom ),
					   std::make_shared<type::mono>( sto ) };
	  
	  dsets.make_set(res);
	  return res;
	}

	type::mono operator()(const type::lit& self, const specialize& parent, substitution&& s) const {
	  return self;
	}
	
  };
  

  // monotypes
  type::mono operator()(const type::mono& self, substitution&& s = {}) const {
	return self.apply<type::mono>(monotype(), *this, std::move(s));
  }

  
  // quantified
  type::mono operator()(const type::forall& self, substitution&& s = {}) const {

	// add substitutions
	for(const auto& v : self.args) {
	  type::var fresh;
	  s[v] = fresh;
	  dsets.make_set(fresh);
	};

	// specialize body given new substitutions
	return self.body->apply<type::mono>(*this, std::move(s));
  }
  
};


static type::mono inst(type::poly p, substitution&& s = {}) {
  using namespace type;

  switch(p.type()) {

	// monotypes
  case poly::type<mono>(): {
	// replace variables given substitution
	auto& self = p.as<mono>();
	switch(self.type()) {

	case mono::type<var>(): {
	  auto it = s.find(self.as<var>());

	  if( it != s.end() ) return it->second;
	  else return self;
	} break;
	case mono::type< app >(): {
	  auto& sub = self.as<app>();
	  
	  type::app res = {std::make_shared<mono>( dsets.find_set( inst(*sub.from, substitution(s) ) )),
					   std::make_shared<mono>( dsets.find_set( inst(*sub.to, substitution(s) )) )};
	  
	  dsets.make_set(res);
	  return res;
	  
	} break;
	default:
	  // i dunno lol
	  return self;
	};	
  }
	
	// polytypes
  case poly::type< forall >(): {
	auto& self = p.as< forall >();

	// add substitutions
	for(const auto& v : self.args) {
	  s[v] = var();
	  dsets.make_set(s[v]);
	};

	return inst(*self.body, std::move(s));
  }	break;
  };

  throw std::logic_error("derp");
}




static void get_vars(std::set<type::var>& out, type::mono t) {
  using namespace type;
  switch(t.type()) {

  case mono::type<var>(): out.insert( t.as<var>() ); break;
  case mono::type< app >(): {
	auto& self = t.as< app >();
	get_vars(out, *self.from);
	get_vars(out, *self.to);
  } break;	
	// nothing lol
  };
  
}



static type::poly generalize(const context& ctx, type::mono t) {
  using namespace type;

  // all type variables in monotype t
  std::set<var> all;
  get_vars(all, t);
  
  // FIXME: maintain this set 
  std::set< var > bound;

  // all bound type variables in context
  for(const auto& x : ctx) {
	switch( x.second.type() ) {

	case poly::type< forall >():
	  for(const auto& v : x.second.as< forall >().args ) {
		bound.insert( v );
	  }
	};
  }
  
  
  std::set<var> diff;

  std::set_difference(all.begin(), all.end(),
					  bound.begin(), bound.end(),
					  std::inserter(diff, diff.begin()));

  if( diff.empty() ) {
	// no quantified variables
	return t;
  } else {
	forall res;
	
	std::copy(diff.begin(), diff.end(),
			  std::back_inserter(res.args));
	
	// watch out: we want deep copy with class representatives
	res.body = std::make_shared<poly>(t);
	return res;
  }
}


template<class T> struct traits;

template<> struct traits<int> {

  static type::lit type() {
	return {"int"};
  }
};

template<> struct traits<void> {
  
  static type::lit type() {
	return {"void"};
  }
};


// TODO wrap union-find
struct union_find {
  
};


struct hindley_milner {

  // TODO reference union-find structure
  
  // var
  type::mono operator()(const ast::var& v, context& c) const {
	auto it = c.find(v);

	if(it == c.end()) {
	  // TODO type_error ?
	  std::string msg = "undeclared variable: ";
	  throw std::runtime_error(msg + v.name);
	}

	// specialize polytype for variable
	return it->second.apply<type::mono>(specialize());
	// return inst( it->second );
  }

  
  // app
  type::mono operator()(const ast::call& app, context& c) const {

	// infer types for func/args
	type::mono func = app.func->apply<type::mono>(*this, c);
	type::mono args = app.args->apply<type::mono>(*this, c);

	// get a fresh type for result
	type::var tau_prime;
	dsets.make_set(tau_prime);

	{
	  // form function type
	  type::app app = { std::make_shared<type::mono>(args),
						std::make_shared<type::mono>(tau_prime) };
	  dsets.make_set(app);
	  
	  // try to unify abstract function type with actual function type
	  unify(func, app);
	
	  return tau_prime;
	}
  }


  // abs
  type::mono operator()(const ast::func& func, context& c) const {

	// get a fresh type for args
	type::var from;
	dsets.make_set(from);

	// add assumption to the context
	context sub = c;
	sub[func.args] = type::mono(from);

	// assert(sub[func.args] == type::mono(from) );
	
	// infer type for function body, given our assumption
	type::mono to = func.body->apply<type::mono>(*this, sub);

	// this is our result type, which will be unified during app given
	// actual arguments
	type::app res = {std::make_shared<type::mono>(from),
					 std::make_shared<type::mono>(to)};
	dsets.make_set(res);
	
	return res;

  }


  // let
  type::mono operator()(const ast::let& let, context& c) const {

	// infer type for definition
	type::mono def = let.def->apply<type::mono>(*this, c);

	// add a new assumption to our context, and generalize as much as
	// possible given current context
	context sub = c;

	type::poly gen = generalize(c, def);
	sub[let.id] = gen;

	// infer type for the body given our assumption for the variable
	// type
	type::mono body = let.body->apply<type::mono>(*this, sub);
	
	return body;
  }

  // litterals: constant types
  template<class T>
  type::mono operator()(const ast::lit<T>& lit, context& c) const {
	type::mono res = traits<T>::type();
	dsets.make_set(res);
	return res;
  }
  
};




// parse tree
struct list;

// TODO string, real
using sexpr = variant< list, symbol, int >;

struct list : vec<sexpr> {
  using vec<sexpr>::vec;

  list(const vec<sexpr>& other) : vec<sexpr>(other) { }
  list(vec<sexpr>&& other) : vec<sexpr>(std::move(other)) { }  
  
  friend std::ostream& operator<<(std::ostream& out, const list& x) {
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
};


struct adaptor {
	
  template<class Arg>
  struct result {
	typedef sexpr type;
  };

  template<class T>
  sexpr operator()(const T& data) const {
	std::cout << "adaptor: " << typeid(T).name() << " " << data << std::endl;
	// return data;
	throw std::logic_error("derp");
  }

  sexpr operator()(int x) const {
	return x;
  }

  sexpr operator()(sexpr x) const {
	return x;
  }

  sexpr operator()(const vec<sexpr>& data) const {
	std::cout << "length: " << data.size() << std::endl;
	
	list res = data;
	std::cout << "adaptor: " << res << std::endl;
	return res;
  }

  
};



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
static sexpr parse(Iterator first, Iterator last) {
  using namespace boost::spirit::qi;
  using boost::phoenix::function;
  using boost::phoenix::construct;  
  using boost::phoenix::val;

  auto comment = lexeme[char_('#') >> *(char_ - eol) >> eol];
  auto skip = space | comment;
  
  using skip_type = decltype(skip);

  rule<Iterator, skip_type, sexpr()> rseq, rlist, ratom,  rsymbol, rint, rexpr, rstart;

  rint = int_;

  const std::string exclude = std::string(" ();\"\x01-\x1f\x7f") + '\0';  
  rsymbol = (as_string[ lexeme[+(~char_(exclude))] ])[ _val = bind(&make_symbol, _1) ];
  
  rseq = (*rexpr)[_val = construct<::list>(_1)];

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

  sexpr res;
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

ast::expr transform(sexpr e);

ast::expr transform_let(const list& e) {

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
  res.def = std::make_shared<ast::expr>(transform(e[2]));

  // body
  res.body = std::make_shared<ast::expr>( transform(e[3]) );
  
  return res;
}




ast::expr transform_func(const list& e) {

  if( e.size() != 3 ) {
	throw syntax_error("function definition");
  }

  assert( e[0].is<symbol>() && e[0].as<symbol>() == keyword.func );

  // args check
  if( !e[1].is<list>() ) throw syntax_error("argument list expected");

  const list& args = e[1].as<list>();
  if(args.size() > 1) {
	std::cerr << "warning: multiple arguments not handled yet" << std::endl;
  }
  
  ast::func res;

  for(const auto& a : e[1].as<list>() ) {
	if( !a.is<symbol>() ) throw syntax_error("arguments must be symbols");

	// TODO push_back
	res.args = ast::var{a.as<symbol>() };
  }
  
  // body
  res.body = std::make_shared<ast::expr>(transform(e[2]));
  
  return res;
}



ast::expr transform(sexpr e) { 

  switch(e.type()) {
  case sexpr::type<int>():
	return ast::lit<int>{e.as<int>()};
  case sexpr::type<symbol>():
	return ast::var{e.as<symbol>()};
  case sexpr::type<list>(): {

	auto& self = e.as<list>();

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
	ast::call res = { std::make_shared<ast::expr>(terms[0]),
					  std::make_shared<ast::expr>(terms[1]) };
	return res;
  }
  }
  
  throw std::logic_error("derp");
}




int main() {

  std::string data;
  std::cout << "enter string:" << std::endl;
  std::getline(std::cin, data);

  sexpr prog = parse(data.begin(), data.end());
  assert( prog.is<list>() );
  
  context c;
  for(const auto& s : prog.as<list>() ) {
	ast::expr e = transform(s);

	type::mono t = e.apply<type::mono>( hindley_milner(), c);
	type::mono r = represent(t);
	type::poly p = generalize(c, r);

	std::cout << s << " :: " << p << std::endl;	
  }

  // debug();
  return 0;
}
