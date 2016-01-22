
#include <iostream>

#include <vector>
#include <map>
#include <set>
#include <memory>

#include "variant.hpp"

#include <boost/pending/disjoint_sets.hpp>


using symbol = const char*;

template<class T>
using ref = std::shared_ptr<T>;

template<class T>
using vec = std::vector<T>;



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

	friend std::ostream& operator<< (std::ostream& out, const var& t) {
	  return out << '\'' << (char)('a' + t.index);
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

	friend std::ostream& operator<< (std::ostream& out, const forall& t) {
	  out << "forall ";

	  for( const auto& c : t.args ) {
		out << c << " ";
	  }
	  
	  return out<< ". " << *t.body;
	}
	
  };

  


}




namespace term {
  
  template<class T> struct lit;
  struct var;
  struct func;
  struct call;
  struct let;

  // TODO refcount?
  using expr = variant< lit<int>, var, func*, call*, let* >;
  
  template<class T>
  struct lit {
	T value;
  };

  struct var {
	symbol name;
	bool operator<(const var& other) const { return name < other.name; }
  };

  // TODO vec
  struct func {
	var args;
	expr body;
  };

  struct call {
	expr func;
	expr args;
  };

  struct let {
	var id;
	expr def;
	expr body;
  };
  
}




using context = std::map< term::var, type::poly >;


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

  std::cout << "unifying " << a << " with " << b << std::endl;
  debug();
  
  using namespace type;
  
  // find representative for each type
  
  // std::cout << "test parent: " << get(p, a) << std::endl;
  
  a = dsets.find_set(a);
  b = dsets.find_set(b);
  
  std::cout << "classes: " << a << ", " << b << std::endl;
  
  if( a.type() == mono::type< app >() && b.type() == mono::type< app >() ) {
	// TODO generalize to other type constructors, with arity check

	// unify each parameters
	unify( *a.as< app >().from, *b.as< app >().from);
	unify( *a.as< app >().to,   *b.as< app >().to);
	
  } else if( a.type() == mono::type<var>() || b.type() == mono::type<var>() ) {
	// merge a and b classes (representative is right-hand side for link)
	std::cout << "merging classes" << std::endl;

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
	  
	  return app(std::make_shared<mono>( dsets.find_set( inst(*sub.from, substitution(s) ) )),
				 std::make_shared<mono>( dsets.find_set(inst(*sub.to, substitution(s) )) ) );
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


static type::poly gen(const context& ctx, type::mono t) {
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

static type::mono infer(context& c, const term::expr& e) {
  using namespace term;
  
  switch(e.type()) {

  case expr::type< lit<int> >(): {


	type::mono res = traits<int>::type();
	dsets.make_set(res);
	return res;
  } break;
	
	// var
  case expr::type< var >(): {
	auto& v = e.as<var>();
	auto it = c.find(v);

	if(it == c.end()) throw std::logic_error("undeclared variable");

	return inst( it->second );
  } break;

	// app
  case expr::type< call* >(): {
	auto& self = e.as<call*>();

	type::mono func = infer(c, self->func );
	
	type::mono args = infer(c, self->args );
	
	type::var tau_prime;
	dsets.make_set(tau_prime);
	
	type::app app( std::make_shared<type::mono>(args),
				   std::make_shared<type::mono>(tau_prime));
	dsets.make_set(app);	
	
	unify(func, app);
	
	return tau_prime;

  } break;	

	// abs
  case expr::type< func* >(): {

	auto& self = e.as<func*>();

	type::var from;
	dsets.make_set(from);
	
	context sub = c;
	sub[self->args] = type::mono(from);

	type::mono to = infer(sub, self->body);
	to.type();
	
	type::mono res = type::app(std::make_shared<type::mono>(from),
							   std::make_shared<type::mono>(to));

	dsets.make_set(res);
	return res;

  } break;

	// let
  case expr::type< let* >(): {
	auto& self = e.as<let*>();

	type::mono def = infer(c, self->def);
	def.type();
	
	context sub = c;
	sub[self->id] = gen(c, def);

	type::mono body = infer(sub, self->body);
	body.type();
	
	return body;
  } break;		

	
  default:
	throw std::runtime_error("unknown expression type");
  };

};




int main() {

  
  using namespace term;
  
  lit<int> b = {true};

  var v1 = {"x"};
  var v2 = {"y"};


  func f1 = { v2, v1 };
  func f2 = { v1, &f1 };

  expr e = b;
  call a = { &f2, b};

  expr x[] = { &a };

  for(const auto& e : x) {

	context c;
	type::mono t = infer(c, e);
	std::cout << represent(t) << std::endl;
  }

  debug();
  return 0;
}
