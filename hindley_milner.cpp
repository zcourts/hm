#include "hindley_milner.hpp"

#include "union_find.hpp"

// #include <boost/pending/disjoint_sets.hpp>
#include <set>

// debug
#include <iostream>

static type::poly generalize(const context& ctx, type::mono t);

struct contains {

  bool operator()(const type::app& self, const type::var& var) const {
	return self.from->apply<bool>(*this, var) ||  self.to->apply<bool>(*this, var);
  };


  bool operator()(const type::lit& self, const type::var& var) const {
	return false;
  };

  bool operator()(const type::var& self, const type::var& var) const {
	return self == var;
  };
  
  
};


struct unification_error : type_error {

  static std::string message(const type::mono& expected,
							 const type::mono& got) {
	context ctx;
	std::stringstream ss;
	ss << "expected " << generalize(ctx, expected) << ", got " << generalize(ctx, got);
	return ss.str();
  }
  
  unification_error(const type::mono& expected, const type::mono& got)
	: type_error( message(expected, got) ) {
	
  }
};

static void unify(union_find<type::mono>& types, type::mono a, type::mono b) {

  // std::cout << "unifying: " << a << " with: " << b << std::endl;
  using namespace type;
  
  // find representative for each type
  a = types.find(a);
  b = types.find(b);
  
  if( a.is<app>() && b.is<app>() ) {
	// TODO generalize to other type constructors, with arity check

	// unify each parameters
	unify(types, *a.as< app >().from, *b.as< app >().from);
	unify(types, *a.as< app >().to,   *b.as< app >().to);
	
  } else if( a.is<var>() || b.is<var>() ) {

	auto& self = a.is<var>() ? a.as<var>() : b.as<var>();
	auto& other = a.is<var>() ? b : a;

	// avoid recursive unification (e.g. 'a with 'a -> 'b)
	if(other.is<app>() && other.apply<bool>(contains(), self)) {
	  throw unification_error(other, self);
	};
	
	// note: other becomes the representative
	types.link(self, other);
	
  } else if( a != b ) {
	throw unification_error(a, b);
  }
};


using substitution = std::map<type::var, type::var>;


// get a nice representative for a type
static type::mono represent(union_find<type::mono>& types, const type::mono& t) {
  
  using namespace type;

  mono res = types.find(t);
  
  if( res.is<app>() ) {
	auto& self = res.as<app>();
	
	res = app{ shared( represent(types, *self.from)),
			   shared( represent(types, *self.to)) };
  }  

  return res;
}


// instantiate a polytype with fresh type variables
struct instantiate {

  // sub-dispatch for monotypes
  struct monotype {

	// replace variables given substitution
	type::mono operator()(const type::var& self, substitution&& s) const {
	  auto it = s.find(self);

	  if( it != s.end() ) return it->second;
	  else return self;
	}

	
	type::mono operator()(const type::app& self, substitution&& s) const {
	  // TODO should we make new substitutions ?
	  
	  // instantiate from/to and get representants
	  type::mono sfrom = self.from->apply<type::mono>(*this, substitution(s) );
	  type::mono sto = self.to->apply<type::mono>(*this, substitution(s) );	  
	  
	  type::app res = {shared(sfrom), shared(sto)};

	  return res;
	}
	
	type::mono operator()(const type::lit& self, substitution&& ) const {
	  return self;
	}
	
  };
  

  // monotypes
  type::mono operator()(const type::mono& self, substitution&& s = {}) const {
	return self.apply<type::mono>(monotype(), std::move(s));
  }

  
  // quantified
  type::mono operator()(const type::forall& self, substitution&& s = {}) const {

	// add substitutions
	for(const auto& v : self.args) {
	  type::var fresh;
	  s[v] = fresh;
	};

	// instantiate body given new substitutions
	return self.body->apply<type::mono>(*this, std::move(s));
  }
  
};



// list all variables in monotype
static void get_vars(std::set<type::var>& out, type::mono t) {
  using namespace type;

  if(t.is<var>() ) {
	out.insert( t.as<var>() );
  } else if( t.is<app>() ) {
	auto& self = t.as< app >();
	get_vars(out, *self.from);
	get_vars(out, *self.to);
  }
 
}


// generalize monotype given context (i.e. quantify all unbound variables)
static type::poly generalize(const context& ctx, type::mono t) {
  using namespace type;

  // all type variables in monotype t
  std::set<var> all;
  get_vars(all, t);
  
  // TODO: maintain this set together with context ?
  std::set< var > bound;

  // all bound type variables in context
  for(const auto& x : ctx) {

	if( x.second.is<forall>() ) {
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

	// TODO does this work ?
	// watch out: we want deep copy with class representatives
	res.body = shared<poly>(t);
	return res;
  }
}




// yep
struct algorithm_w {

  union_find<type::mono>& types;
  
  // var
  type::mono operator()(const ast::var& v, context& c) const {
	auto it = c.find(v);

	if(it == c.end()) {
	  std::string msg = "unknown variable: ";
	  throw type_error(msg + v.name());
	}
	
	// instantiate variable polytype stored in context
	return it->second.apply<type::mono>(instantiate());
  }



  // app
  type::mono operator()(const ast::app& self, context& c) const {

	// infer type for function
	type::mono func = self.func->apply<type::mono>(*this, c);

	// fresh result type
	type::var result;

	// function type for application: a0 -> (a1 -> (... -> (an -> result)))
	type::mono app = result;
	
	for(auto it = self.args.rbegin(), end = self.args.rend(); it != end; ++it) {
	  type::mono ai = it->apply<type::mono>(*this, c);
	  app = type::app{ shared(ai), shared(app)};
	};

	// TODO remove special cases by processing ast earlier ?
	// special case for nullary functions
	if( self.args.empty() ) {
	  app = type::app{ shared<type::mono>(type::unit), shared(app) };
	}

	// unify function type with app
	unify(types, func, app);

	// application has the result type
	return result;
  }

  

  // helper
  template<class Iterator>
  type::mono abs(Iterator first, Iterator last, ref<ast::expr> body, context& c) const {

	const unsigned argc = last - first;

	// enriched context with argument type
	context sub = c;
	
	type::mono from, to;

	if( !argc ) {
	  // nullary functions
	  from = type::unit;
	} else {
	  
	  // fresh type variable for argument type
	  from = type::var();
	  
	  // add assumption to the context
	  sub[ *first ] = from;
	}

	if( argc > 1 ) {
	  // n-ary function: a0 -> (a1 -> a2 -> ... -> result)
	  // i.e. to is inferred recursively from unary case
	  to = abs(first + 1, last, body, sub);
	} else {
	  // infer result type from body given our assumption
	  to = body->apply<type::mono>(*this, sub);
	}

	return type::app{ shared(from), shared(to) };
  };
  
  // abs
  type::mono operator()(const ast::abs& self, context& c) const {
	return abs(self.args.begin(), self.args.end(), self.body, c);
  }


  // let
  type::mono operator()(const ast::let& let, context& c) const {

	// infer type for definition
	type::mono def = let.value->apply<type::mono>(*this, c);

	// enriched context with local definition
	context sub = c;

	// generalize as much as possible given current context
	type::poly gen = generalize(c, def);
	sub[let.id] = gen;

	// infer type for the body given our assumption
	type::mono body = let.body->apply<type::mono>(*this, sub);

	return body;
  }

  
  // litterals: constant types
  template<class T>
  type::mono operator()(const ast::lit<T>& , context& ) const {
	type::mono res = type::traits<T>::type();
	return res;
  }
  
};


type::poly hindley_milner(const context& ctx, const ast::expr& e) {

  context c = ctx;
  union_find<type::mono> types;

  // compute monotype in current context
  type::mono t = e.apply<type::mono>( algorithm_w{types}, c);

  // obtain a nice representative
  // TODO merge w/ generalize ?
  type::mono r = represent(types, t);

  // generalize as much as possible given context
  type::poly p = generalize(c, t);

  return p;
}


