#include "hindley_milner.hpp"

#include <boost/pending/disjoint_sets.hpp>
#include <set>

// debug
#include <iostream>

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
	
	res = app{ shared( represent(*self.from)),
			   shared( represent(*self.to)) };
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
	  
	  type::app res = {shared( sfrom ), shared(sto)};
	  
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

// list all variables in monotype
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
	res.body = shared<poly>(t);
	return res;
  }
}


// constant types for literals
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


struct algorithm_w {

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
  }

  
  // app
  type::mono operator()(const ast::call& app, context& c) const {

	// infer types for func/args
	type::mono func = app.func->apply<type::mono>(*this, c);
	type::mono args = app.args->apply<type::mono>(*this, c);

	// get a fresh type for result
	type::mono fresh = type::var();
	dsets.make_set(fresh);

	{
	  // form function type
	  type::app app = { shared(args), shared(fresh) };
	  dsets.make_set(app);
	  
	  // try to unify abstract function type with actual function type
	  unify(func, app);
	
	  return fresh;
	}
  }


  // abs
  type::mono operator()(const ast::func& func, context& c) const {

	// get a fresh type for args
	type::mono from = type::var();
	dsets.make_set(from);

	// add assumption to the context
	context sub = c;
	sub[func.args] = from;

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


type::poly hindley_milner(const context& ctx, const ast::expr& e) {

  context c = ctx;
  type::mono t = e.apply<type::mono>( algorithm_w(), c);
  type::mono r = represent(t);
  type::poly p = generalize(c, r);
  
  return p;
}


