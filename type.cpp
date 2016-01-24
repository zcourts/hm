#include "type.hpp"


namespace type {

  
  unsigned var::total = 0;


  std::ostream& operator<< (std::ostream& out, const var& t) {
	if( ('a' + t.index) < 'z' ) {
	  return out << (char)('a' + t.index);
	} else {
	  return out << "type-" << t.index;
	}
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
	out << "forall";

	for( const auto& c : t.args ) {
	  out << " " << c;
	}
	  
	return out<< ". " << *t.body;
  }

}
