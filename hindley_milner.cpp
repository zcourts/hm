#include "hindley_milner.hpp"

#include "union_find.hpp"

// #include <boost/pending/disjoint_sets.hpp>
#include <set>

// debug
#include <iostream>

#include "debug.hpp"

// TODO where to put this ?
namespace type {

  template<>
  struct traits<ast::fixpoint> {
    static type::poly type() {
      static type::var v;
      static context ctx;
      static type::poly p = generalize(ctx, (v >>= v) >>= v);
      return p;
    }
  };

}


// context
const type::poly& context::find(const ast::var& var) const {

  auto it = table.find(var);

  if( it != table.end() ) return it->second;

  if( parent ) return parent->find(var);

  throw type_error( std::string("unbound variable: ") + var.name() );
}


context& context::set(const ast::var& var, const type::poly& p) {
  auto it = table.insert( last, std::make_pair(var, p) );

  // insertion failed
  if(it->first != var) {

	// TODO are we super sure this is sufficient ?
	if( it->second.is<type::scheme>() ) {
	  
	  for(const auto& v : it->second.as<type::scheme>().args ) {
		vars.erase(v);
	  }
	  
	}
	
	it->second = p;
  }
  
  last = it;
  
  if( it->second.is<type::scheme>() ) {
	
	for(const auto& v : it->second.as<type::scheme>().args ) {
	  vars.insert(v);
	}
	
  }
  
  return *this;
}

context::iterator context::begin() const {
  if( table.empty() ) {
	if( parent ) return parent->begin();
	else return end();
  }
  return {this, table.begin()};
}

context::iterator context::end() const {
  return {nullptr, {}};
}


bool context::iterator::operator!=(const iterator& other) const {
  if(c != other.c) return true;
  if(c && (i != other.i)) return true;
  return false;
}

context::iterator& context::iterator::operator++() {
  if(!c) throw std::out_of_range("iterator");

  if(c->table.empty()) throw std::out_of_range("derp");

  
  if(++i == c->table.end()) {
	c = c->parent;
	
	if(c) {
	  i = c->table.begin();
	}
  }
  
  return *this;
}


void context::insert(iterator first, iterator last) {
  for(auto it = first; it != last; ++it) {
	set( (*it).first, (*it).second);
  }
};

const context::table_type::value_type& context::iterator::operator*() const {
  if(!c) throw std::out_of_range("iterator");
  return *i;
}


bool context::bound(const type::var& v) const {

  auto it = vars.find(v);
  if( it != vars.end() ) return true;

  if( parent ) return parent->bound(v);
  return false;
}






// dangerous variables
static std::set<type::var>&& dangerous(const type::mono& t, std::set<type::var>&& res = {});

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


// get a nice representative for type t
static type::mono represent(union_find<type::mono>& types, const type::mono& t);

// list all variables in monotype
static std::set<type::var>&& variables(const type::mono& t, std::set<type::var>&& out = {});


static void unify(union_find<type::mono>& types, type::mono a, type::mono b) {

  auto& out = debug<2>() << " unifying: " << a << " with: " << b;
  
  using namespace type;
  
  // find representative for each type
  a = types.find(a);
  b = types.find(b);
  
  if( a.is<app>() && b.is<app>() ) {
    out << std::endl;
    
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
    try {
      for(unsigned i = 0, n = ta->args.size(); i < n; ++i) {
        unify(types, ta->args[i], tb->args[i]);
      }
    } catch(unification_error& e) {

      auto ta = represent(types, a);
      auto tb = represent(types, b);

      // don't spit out dangling type variables
      if( variables(ta).empty() && variables(tb).empty() ) {
        std::cerr << "when unifying: " << ta << " with: " << tb << std::endl;
      }

      throw;
    }
	
  } else if( a.is<var>() || b.is<var>() ) {

	auto& self = a.is<var>() ? a.as<var>() : b.as<var>();
	auto& other = a.is<var>() ? b : a;

	// avoid recursive unification (e.g. 'a with 'a -> 'b)
	if(other.is<app>() && other.apply<bool>(contains(), self)) {
	  throw unification_error(other, self);
	};
	
    // other becomes the representative
    types.link(self, other);

    out << "\t=> " << types.find(other);
     
    
  } else if( a != b ) {
	throw unification_error(a, b);
  }
  
  out << std::endl;
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
static std::set<type::var>&& variables(const type::mono& t, std::set<type::var>&& out) {
  using namespace type;
  
  if(t.is<var>() ) {
	out.insert( t.as<var>() );
  } else if( t.is<app>() ) {

	for(const auto& ti : t.as<app>()->args) {
	  variables(ti, std::move(out));
	}	  
  }
  
  return std::move(out);
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

template<class T>
static std::set<T> operator-(const std::set<T>& lhs, const std::set<T>& rhs) {
  std::set<T> res;

  std::set_difference(lhs.begin(), lhs.end(),
					  rhs.begin(), rhs.end(),
					  std::inserter(res, res.begin()));

  return res;
}


// generalize monotype given context (i.e. quantify all unbound
// variables). you might want to 'represent' first.
type::poly generalize(const context& ctx, const type::mono& t, const std::set<type::var>& exclude) {
  using namespace type;

  std::set<var> vars = variables(t) - exclude;

  std::set<var> free;
  
  std::copy_if(vars.begin(), vars.end(), std::inserter(free, free.end()),
			   [&](const var& v) {
				 return !ctx.bound(v);
			   });
  
  if( free.empty() ) {
	// no free variables
	return t;
  } else {
	scheme res;
	
	std::copy(free.begin(), free.end(), std::back_inserter(res.args));

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

  type::mono type;
  
  ~debug_raii() {
	auto& out = debug<log_level>(-1) << "   " << id << ')';
    if( type ) out << "\t::\t" << type;
    out << std::endl;                 
  }
};


// yep
struct algorithm_w {

  union_find<type::mono>& types;

  // var
  type::mono operator()(const ast::var& v, const context& c) const {
	debug_raii debug("var", v);
	
	type::poly p = c.find(v);

	type::mono res;
	if( p.is<type::scheme>() ) {
	  // instantiate type scheme with fresh variables
	  res = p.apply<type::mono>(instantiate());
	} else {

	  // this happens with value restriction
	  res = types.find( p.as<type::mono>() );
	}
	
	debug.type = res;
    return res;
  }



  // app
  type::mono operator()(const ast::app& self, const context& c) const {
	debug_raii debug("app");
	
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
	debug.type = result;
    return result;
  }

  

  // abs helper
  template<class Iterator>
  type::mono abs(Iterator first, Iterator last, ref<ast::expr> body, const context& c) const {
	const unsigned argc = last - first;

	// enriched context with argument type
	context sub(&c);
	
	type::mono from, to;

	if( !argc ) {
	  // nullary functions
	  from = type::unit;
	} else {
	  
	  // fresh type variable for argument type
	  from = type::var();
	  
	  // add assumption to the context
	  sub.set(*first, from);
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
  type::mono operator()(const ast::abs& self, const context& c) const {
	debug_raii debug("abs");
    type::mono res = abs(self.args.begin(), self.args.end(), self.body, c);
    debug.type = res;
    return res;
  }

  

  // let
  type::mono operator()(const ast::let& let, const context& c) const {
	debug_raii debug("let", let.id);
	
	// infer type for definition
	type::mono def = let.value->apply<type::mono>(*this, c);

	// enriched context with local definition
	context sub(&c);

	// generalize
	def = represent(types, def);
	std::set<type::var> exclude = dangerous(def);
	
	type::poly gen = generalize(c, def, exclude);
	
	sub.set(let.id, gen);
	
	// infer type for the body given our assumption
	type::mono body = let.body->apply<type::mono>(*this, sub);

    debug.type = body;
	return body;
  }

  
  // litterals: constant types
  template<class T>
  type::mono operator()(const ast::lit<T>& , const context& ) const {
	debug_raii debug("lit");
    type::poly p = type::traits<T>::type();
    type::mono res = p.apply<type::mono>(instantiate());
    debug.type = res;
    return res;
  }


  // do notation: typecheck desugared expression
  type::mono operator()(const ast::seq& self, const context& e) const {
	return self.monad().apply<type::mono>(*this, e);
  }
  
};





type::poly hindley_milner(union_find<type::mono>& types, const context& ctx, const ast::expr& e) {
  
  // compute monotype in current context
  type::mono t = e.apply<type::mono>( algorithm_w{types}, ctx);

  t = represent(types, t);

  // restrict quantification for let bindings
  if(e.is<ast::let>() ) {
	return generalize(ctx, t, dangerous(t) );
  } else {
	return generalize(ctx, t);
  }
  
}




static std::set<type::var>&& dangerous(const type::mono& t, std::set<type::var>&& res) {
  if( t.is<type::app>() ) {
	auto& self = t.as<type::app>();

	// evil refs !
	if( self->func == type::ref ) {
	  auto sub = variables(self->args[0]);
	  res.insert(sub.begin(), sub.end());
	} else if (self->func == type::func ) {

	  // function has side effects
	  if( self->args[1].is<type::app>() && self->args[1].as<type::app>()->func == type::io ) {

		// variables in contravariant position are dangerous
		auto sub = variables(self->args[0]);
		res.insert(sub.begin(), sub.end());
	  }
	  
	} else  {

	  // TODO systematize covariance/contravariance analysis ?
	  
	  // not quite sure about that but...
	  for(const auto& t : self->args) {
		dangerous(t, std::move(res));
	  }
	}
  }
  
  return std::move(res);
}


