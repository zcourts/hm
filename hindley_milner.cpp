#include "hindley_milner.hpp"

#include "union_find.hpp"

// #include <boost/pending/disjoint_sets.hpp>
#include <set>

// debug
#include <iostream>

#include "debug.hpp"


// unify monotypes @a and @b in union_find structure @types
static void unify(union_find<type::mono>& types, type::mono a, type::mono b);


// true if monotype @self contains type variable @var
struct contains {

  bool operator()(const type::app& self, const type::var& var) const {

	for(const auto& t : self->args) {
	  if(t == var) return true;
	}

	return false;
  };


  bool operator()(const type::lit& , const type::var& ) const {
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

  debug<2>() << " unifying: " << a << " with: " << b << std::endl;
  
  using namespace type;
  
  // find representative for each type
  a = types.find(a);
  b = types.find(b);
  
  if( a.is<app>() && b.is<app>() ) {

	auto& ta = a.as<app>();
	auto& tb = b.as<app>();

	// check type func
	if( ta->func != tb->func ) {
	  throw unification_error(a, b);
	}

	assert( ta->args.size() == tb->args.size());
	assert( ta->args.size() == ta->func.arity() );
	assert( tb->args.size() == tb->func.arity() );	

	// unify arg-wise
	for(unsigned i = 0, n = ta->args.size(); i < n; ++i) {
	  unify(types, ta->args[i], tb->args[i]);
	}
	
  } else if( a.is<var>() || b.is<var>() ) {

	auto& self = a.is<var>() ? a.as<var>() : b.as<var>();
	auto& other = a.is<var>() ? b : a;

	// avoid recursive unification (e.g. 'a with 'a -> 'b)
	if(other.is<app>() && other.apply<bool>(contains(), self)) {
	  throw unification_error(other, self);
	};
	
	// note: second arg becomes the representative
	types.link(self, other);
	
  } else if( a != b ) {
	throw unification_error(a, b);
  }
};


using substitution = std::map<type::var, type::var>;





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
	  // TODO reuse substitution ?

	  vec<type::mono> args;
	  args.reserve( self->args.size() );
	  
	  for(const auto& t : self->args) {
		args.push_back( t.apply<type::mono>(*this, substitution(s)) );
	  }
	  
	  return std::make_shared<type::app_type>(self->func, args);
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
  type::mono operator()(const type::scheme& self, substitution&& s = {}) const {

	// add substitutions
	for(const auto& v : self.args) {
	  type::var fresh;
	  s[v] = fresh;
	};

	// instantiate body given new substitutions
	return self.body.apply<type::mono>(*this, std::move(s));
  }
  
};



// list all variables in monotype
static void get_vars(std::set<type::var>& out, const type::mono& t) {
  using namespace type;
  
  if(t.is<var>() ) {
	out.insert( t.as<var>() );
  } else if( t.is<app>() ) {

	for(const auto& ti : t.as<app>()->args) {
	  get_vars(out, ti);
	}	  
  }
 
}


// select the best representative for type t
static type::mono represent(union_find<type::mono>& types,
							const type::mono& t) {
  // debug_raii _("represent", t);
  
  type::mono res = types.find(t);

  if( res.is<type::app>() ) {
	auto& self = res.as<type::app>();
	
	vec<type::mono> args;
	std::transform(self->args.begin(), self->args.end(), std::back_inserter(args),
				   [&](const type::mono& t) {
					 return represent(types, t);
				   });
	
	res = std::make_shared<type::app_type>(self->func, args);
  }

  return res;  
}


// generalize monotype given context (i.e. quantify all unbound
// variables). you might want to 'represent' first.
type::poly generalize(const context& ctx, const type::mono& t) {
  using namespace type;

  
  // all type variables in monotype t
  std::set<var> all;
  get_vars(all, t);

  // TODO: maintain this set together with context ?
  std::set< var > bound;

  // all bound type variables in context
  for(const auto& x : ctx) {

	if( x.second.is<scheme>() ) {
	  for(const auto& v : x.second.as< scheme >().args ) {
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
	scheme res;
	
	std::copy(diff.begin(), diff.end(), std::back_inserter(res.args));

	// TODO does this work ?
	// watch out: we want deep copy with class representatives
	res.body = t;

	return res;
  }
}


struct debug_raii {
  const char* const id;

  static constexpr int log_level = 2;
  
  template<class... Args>
  debug_raii(const char* id, Args&&... args) :id(id) {
	auto& out = debug<log_level>(1) << '(' << id;
	int unpack[] = {(out << " " << args, 0)...}; (void) unpack;
	out << std::endl;
  }

  ~debug_raii() {
	debug<log_level>(-1) << "   " << id << ')' << std::endl;
  }
};


// yep
struct algorithm_w {

  union_find<type::mono>& types;
  
  // var
  type::mono operator()(const ast::var& v, context& c) const {
	debug_raii _("var", v);
	
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
	debug_raii _("app");
	
	// infer type for function 
	type::mono func = self.func->apply<type::mono>(*this, c);

	// fresh result type
	type::var result;

	// function type for application: a0 -> (a1 -> (... -> (an -> result)))
	type::mono app = result;
	
	for(auto it = self.args.rbegin(), end = self.args.rend(); it != end; ++it) {
	  type::mono ai = it->apply<type::mono>(*this, c);
	  app = ai >>= app; 
	};

	// TODO remove special cases by processing ast earlier ?
	// special case for nullary functions
	if( self.args.empty() ) {
	  app = type::unit >>= app; 
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

	// TODO this is horribly inefficient
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

	return from >>= to;
  };
  
  
  // abs
  type::mono operator()(const ast::abs& self, context& c) const {
	debug_raii _("abs");
	return abs(self.args.begin(), self.args.end(), self.body, c);
  }

  // let
  type::mono operator()(const ast::let& let, context& c) const {
	debug_raii _("let", let.id);
	
	// infer type for definition
	type::mono def = let.value->apply<type::mono>(*this, c);

	// enriched context with local definition
	// TODO this is horribly inefficient
	context sub = c;
	
	// generalize as much as possible given current context
	type::poly gen = generalize(c, represent(types, def));
	sub[let.id] = gen;
	
	// infer type for the body given our assumption
	type::mono body = let.body->apply<type::mono>(*this, sub);

	return body;
  }

  
  // litterals: constant types
  template<class T>
  type::mono operator()(const ast::lit<T>& , context& ) const {
	debug_raii _("lit");
	return type::traits<T>::type();
  }
  
};




type::poly hindley_milner(const context& ctx, const ast::expr& e) {

  context c = ctx;
  union_find<type::mono> types;

  // compute monotype in current context
  type::mono t = e.apply<type::mono>( algorithm_w{types}, c);

  // generalize as much as possible given context
  type::poly p = generalize(c, represent(types, t) );

  return p;
}


 
