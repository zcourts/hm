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

  app_type::app_type(const constructor& ctor,
					 const vec<mono>& args)
	: ctor(ctor),
	  args(args) {
	if( ctor.arity() != args.size() ) {
	  throw std::runtime_error("arity error for " + std::string(ctor.name()));
	}
  }
  
  struct stream {

	typedef std::map<type::var, unsigned> context_type;
	context_type& context;

	stream(context_type& context, bool parentheses = false)
	  : context( context ),
		parentheses( parentheses ) {
	}
	
	bool parentheses;
	
	// quantified
	void operator()(const forall& self, std::ostream& out) const {
	 
	  // add stuff to context
	  for(unsigned i = 0, n = self.args.size(); i < n; ++i) {
		context[self.args[i]] = context.size();
	  }

	  self.body->apply(*this, out);
	}

	void operator()(const mono& self, std::ostream& out) const {
	  self.apply(*this, out);
	}

	void operator()(const app& self, std::ostream& out) const {

	  if( parentheses ) out << '(';
	  
	  if( self->ctor == func ) { 
		self->args[0].apply( stream{context, true}, out);
		out << " -> ";
		self->args[1].apply( stream{context, false}, out);
	  } else {

		out << self->ctor;
		
		for(const type::mono& t : self->args) {
		  t.apply( stream{context, true}, out );
		}
		
	  }

	  if( parentheses ) out << ')';
	}


	void operator()(const lit& self, std::ostream& out) const {
	  out << self;
	}


	void operator()(const var& self, std::ostream& out) const {
	  auto id = context.find(self);

	  if( id != context.end() && ('a' + id->second) < 'z' ) {
		out << '\'' << (char)('a' + id->second);
	  } else { 
		out << "'type-" << self.index;
	  }
	}
	
  };


   

  std::ostream& operator<< (std::ostream& out, const mono& p) {
	stream::context_type context;
	p.apply( stream(context), out );
	return out;
  }
  
  
  std::ostream& operator<< (std::ostream& out, const poly& p) {
	stream::context_type context;
	p.apply( stream(context), out );
	return out;
  }

  const lit integer = traits<int>::type();
  const lit boolean = traits<bool>::type();
  const lit unit = traits<void>::type();

  static std::map<constructor, unsigned> arity_map;
  const constructor func = { "->", 2 };


  
  constructor::constructor(const char* name, unsigned n) : symbol(name) {
	auto it = arity_map.find(*this);
	if( it != arity_map.end() ) {
	  // TODO or simply check that arity is consistent ?
	  throw std::runtime_error("redefined type constructor lol");
	}
	
	arity_map[*this] = n;
  }

  unsigned constructor::arity() const {
	auto it = arity_map.find(*this);

	assert(it != arity_map.end());
	return it->second;
  }
  

  
}
