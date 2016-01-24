#include "sexpr.hpp"

namespace sexpr {

  std::ostream& operator<<(std::ostream& out, const list& x) {
	out << '(';
	
	bool first = true;
	for(const auto& xi : x) {
	  if(!first) {
		out << ' ';
	  } else {
		first = false;
	  }
	  out << xi;
	}
	out << ')';
	
	return out;
  }


}
