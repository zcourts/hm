#include "type.hpp"

#include <ostream>
#include <map>

namespace type {

  
  unsigned var::total = 0;

  bool app::operator<(const app& other) const {
	return  (*from < *other.from) || ((*from == *other.from) && (*to < *other.to));
  }


  bool app::operator==(const app& other) const {
	return  (*from == *other.from) && (*to == *other.to);
  }

  struct stream {

	mutable std::map<type::var, unsigned> context;

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
	  const bool is_from_app = self.from->is< app >();
	  const bool is_to_app = self.to->is< app >();

	  out << (is_from_app ? "(" : "");
	  self.from->apply(*this, out);
	  out << (is_from_app ? ")" : "");

	  out << " -> ";

	  // TODO maybe we don't have to parentize on the right	  
	  out << (is_to_app ? "(" : "");
	  self.to->apply(*this, out);
	  out << (is_to_app ? ")" : "")
		;
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
	p.apply( stream(), out );
	return out;
  }
  
  
  std::ostream& operator<< (std::ostream& out, const poly& p) {
	p.apply( stream(), out );
	return out;
  }


}
