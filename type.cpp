#include "type.hpp"

#include <ostream>
#include <map>

namespace type {

  
  unsigned var::total = 0;

  // bool app::operator<(const app& other) const {
  // 	return  (*from < *other.from) || ((*from == *other.from) && (*to < *other.to));
  // }


  // bool app::operator==(const app& other) const {
  // 	return  (*from == *other.from) && (*to == *other.to);
  // }

  app_type::app_type(const abs& func,
					 const vec<mono>& args)
	: func(func),
	  args(args) {
	if( func.arity() != args.size() ) {
	  throw std::runtime_error("arity error for " + std::string(func.name()));
	}
  }

  
  struct stream {

	typedef std::map<type::var, unsigned> context_type;

	context_type& bound;
	context_type& free;
	bool parentheses;
	
	
	// type scheme
	void operator()(const scheme& self, std::ostream& out) const {

	  // add stuff to context
	  for(unsigned i = 0, n = self.args.size(); i < n; ++i) {
		const unsigned pos = bound.size();
		bound[self.args[i]] = pos;
	  }

	  self.body.apply(*this, out);
	}

	void operator()(const mono& self, std::ostream& out) const {
	  self.apply(*this, out);
	}

	void operator()(const app& self, std::ostream& out) const {
	  const unsigned size = self->args.size();
	  
	  if( parentheses) out << '(';
	  
	  if( self->func == func ) { 
		self->args[0].apply( stream{bound, free, true}, out);
		out << " -> ";
		self->args[1].apply( stream{bound, free, false}, out);
	  } else {

		out << self->func;

		unsigned i = 0;
		for(const type::mono& t : self->args) {
		  t.apply( stream{bound, free, (++i != size)}, out << " " );
		}
		
	  }

	  if( parentheses) out << ')';
	}


	void operator()(const lit& self, std::ostream& out) const {
	  out << self;
	}


	static void varname(std::ostream& out, unsigned i) {

	  if( 'a' + i < 'z' ) out << (char)('a' + i);
	  else out << i;
	  
	}

	void operator()(const var& self, std::ostream& out) const {
	  auto id = bound.find(self);
	  
	  if( id != bound.end() ) {
		out << "'";
		varname(out, id->second);
	  } else {
		id = free.insert( std::make_pair(self, free.size() ) ).first;
		out << "!";
		varname(out, id->second);
	  }
	  
	}
	
  };


  
  std::ostream& operator<< (std::ostream& out, const poly& p) {
	stream::context_type bound, free;
	p.apply( stream{bound, free, false}, out );
	return out;
  }

  std::ostream& operator<< (std::ostream& out, const mono& t) {
	return out << poly(t);
  }
	
  const lit integer = traits<int>::type().as<lit>();
  const lit real = traits<double>::type().as<lit>();  
  const lit boolean = traits<bool>::type().as<lit>();
  const lit unit = traits<void>::type().as<lit>();

  
  static std::map<abs, unsigned> arity_map;
  
  const abs io("io", 1);
  const abs ref("ref", 1);
  
  const abs func = { "->", 2 };


  
  abs::abs(const char* name, unsigned n) : symbol(name) {
	auto it = arity_map.find(*this);
	if( it != arity_map.end() ) {
	  // TODO or simply check that arity is consistent ?
	  throw std::runtime_error("redefined type constructor lol");
	}
	
	arity_map[*this] = n;
  }

  unsigned abs::arity() const {
	auto it = arity_map.find(*this);

	assert(it != arity_map.end());
	return it->second;
  }
  

  
}
