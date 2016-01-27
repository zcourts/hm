#include "hindley_milner.hpp"

#include "union_find.hpp"

// #include <boost/pending/disjoint_sets.hpp>
#include <set>

// debug
#include <iostream>


static void unify(union_find<type::mono>& types, type::mono a, type::mono b) {

  // std::cout << "unifying: " << a << " with: " << b << std::endl;
  // debug();
  
  using namespace type;
  
  // find representative for each type
  
  // std::cout << "test parent: " << get(p, a) << std::endl;
  
  a = types.find(a);
  b = types.find(b);
  
  // std::cout << "classes: " << a << ", " << b << std::endl;
  
  if( a.is<app>() && b.is<app>() ) {
	// TODO generalize to other type constructors, with arity check

	// unify each parameters
	unify(types, *a.as< app >().from, *b.as< app >().from);
	unify(types, *a.as< app >().to,   *b.as< app >().to);
	
  } else if( a.is<var>() || b.is<var>() ) {
	// merge a and b classes (representative is right-hand side for link)
	// std::cout << "merging classes" << std::endl;

	if( a.is<var>() ) {
	  types.link(a, b);
	} else {
	  types.link(b, a);
	}

	// TODO we should make sure the term is the representative and not
	// the variable

  } else if( a != b ) {
	std::stringstream ss;
	ss << "can't match " << a << " with " << b;
	throw type_error(ss.str());
  }
};


using substitution = std::map<type::var, type::var>;



static type::mono represent(union_find<type::mono>& types, type::mono t) {
  
  using namespace type;

  mono res = types.find(t);

  if( res.is<app>() ) {
	auto& self = res.as<app>();
	
	res = app{ shared( represent(types, *self.from)),
			   shared( represent(types, *self.to)) };
  }  

  return res;
}



struct specialize {

  union_find<type::mono>& types;
  
  // sub-dispatch for monotypes
  struct monotype {

	// replace variables given substitution
	type::mono operator()(const type::var& self, const specialize& , substitution&& s) const {
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
	  
	  type::app res = {shared( sfrom ), shared(sto)};

	  return res;
	}

	type::mono operator()(const type::lit& self, const specialize& , substitution&& ) const {
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
	};

	// specialize body given new substitutions
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


// generalize monotype given context (quantify all unbound variables)
static type::poly generalize(const context& ctx, type::mono t) {
  using namespace type;

  // all type variables in monotype t
  std::set<var> all;
  get_vars(all, t);
  
  // FIXME: maintain this set 
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
	
	// watch out: we want deep copy with class representatives
	res.body = shared<poly>(t);
	return res;
  }
}





struct algorithm_w {

  // TODO reference union-find structure
  union_find<type::mono>& types;
  
  // var
  type::mono operator()(const ast::var& v, context& c) const {
	auto it = c.find(v);

	if(it == c.end()) {
	  // TODO type_error ?
	  std::string msg = "undeclared variable: ";
	  throw std::runtime_error(msg + v);
	}

	// specialize polytype for variable
	return it->second.apply<type::mono>(specialize{types});
  }

  
  // app
  type::mono operator()(const ast::app& app, context& c) const {

	// infer types for func/args
	type::mono func = app.func->apply<type::mono>(*this, c);
	type::mono args = app.args->apply<type::mono>(*this, c);

	// get a fresh type for result
	type::mono fresh = type::var();

	{
	  // form function type
	  type::app app = { shared(args), shared(fresh) };
	  
	  // try to unify abstract function type with actual function type
	  unify(types, func, app);
	
	  return fresh;
	}
  }


  // abs
  type::mono operator()(const ast::abs& abs, context& c) const {

	// get a fresh type for args
	type::mono from = type::var();

	// add assumption to the context
	context sub = c;
	sub[abs.args] = from;

	// assert(sub[func.args] == type::mono(from) );
	
	// infer type for function body, given our assumption
	type::mono to = abs.body->apply<type::mono>(*this, sub);

	// this is our result type, which will be unified during app given
	// actual arguments
	type::app res = {std::make_shared<type::mono>(from),
					 std::make_shared<type::mono>(to)};
	return res;

  }


  // let
  type::mono operator()(const ast::let& let, context& c) const {

	// infer type for definition
	type::mono def = let.value->apply<type::mono>(*this, c);

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
  type::mono operator()(const ast::lit<T>& , context& ) const {
	type::mono res = type::traits<T>::type();
	return res;
  }
  
};


type::poly hindley_milner(const context& ctx, const ast::expr& e) {

  context c = ctx;
  union_find<type::mono> types;
  
  type::mono t = e.apply<type::mono>( algorithm_w{types}, c);
  type::mono r = represent(types, t);
  type::poly p = generalize(c, r);

  // std::cerr << "hm mono: " << t << std::endl;
  // std::cerr << "hm repr: " << r << std::endl;
  // std::cerr << "hm poly: " << p << std::endl;    
  
  return p;
}


