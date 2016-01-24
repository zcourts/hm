#include "type.hpp"

#include <ostream>
#include <map>

namespace type {

  
  unsigned var::total = 0;

  // TODO we should protect this with mutexes
  static std::map< std::ostream*, std::map<type::var, unsigned> > context;
  
  std::ostream& operator<< (std::ostream& out, const var& t) {

	auto it = context.find(&out);
	if(it != context.end() ) {

	  auto id = it->second.find(t);

	  if( id != it->second.end() && ('a' + id->second) < 'z' ) {
		return out << '\'' << (char)('a' + id->second);
	  } 
	}
	
	return out << "'type-" << t.index;
	
  }

  bool app::operator<(const app& other) const {
	return  (*from < *other.from) || ((*from == *other.from) && (*to < *other.to));
  }


  bool app::operator==(const app& other) const {
	return  (*from == *other.from) && (*to == *other.to);
  }


  std::ostream& operator<< (std::ostream& out, const app& t) {

	const bool is_from_app = t.from->is< app >();
	const bool is_to_app = t.to->is< app >();
	  
	return
	  out << (is_from_app ? "(" : "") << *t.from << (is_from_app ? ")" : "")
		  << " -> "
	  // TODO maybe we don't have to parentize on the right
		  << (is_to_app ? "(" : "") << *t.to << (is_to_app ? ")" : "")
	  ;
  }

  
  std::ostream& operator<< (std::ostream& out, const forall& t) {

	auto ins = context.insert( {&out, {} } );
	auto& index = ins.first->second;
	
	// add stuff to context
	for(unsigned i = 0, n = t.args.size(); i < n; ++i) {
	  index[t.args[i]] = index.size();
	}
	
	out << *t.body;

	// insertion was successful, we must cleanup
	if( ins.second ) context.erase(ins.first);
	
	return out;
  }

}
