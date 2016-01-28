#include "sexpr.hpp"

namespace sexpr {

  struct stream {

	template<class T>
	void operator()(const T& self, std::ostream& out) const {
	  out << self;
	}

	void operator()(const list& self, std::ostream& out) const {
	  out << '(';
	
	  bool first = true;
	  for(const auto& xi : self) {
		if(!first) {
		  out << ' ';
		} else {
		  first = false;
		}
		out << xi;
	  }
	  out << ')';
	}
	
  };


  std::ostream& operator<<(std::ostream& out, const expr& e) {
	e.apply( stream(), out );
	return out;
  }
  
}
