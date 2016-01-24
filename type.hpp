#ifndef TYPE_HPP
#define TYPE_HPP

#include "common.hpp"
#include "variant.hpp"


// TODO cpp
#include <ostream>

// type-checking
namespace type {

  struct lit;
  struct var;
  struct app;

  using mono = variant< app, var, lit >;
  
  struct lit {
	symbol name;

	inline bool operator<(const lit& other) const { return name < other.name; }
	inline bool operator==(const lit& other) const { return name == other.name; }


	inline friend std::ostream& operator<< (std::ostream& out, const lit& t) {
	  return out << t.name;
	}
  
  };
  
  struct var {
	var(unsigned index = total++) : index(index) { }
	unsigned index;
	
	static unsigned total;

	inline bool operator<(const var& other) const { return index < other.index; }
	inline bool operator==(const var& other) const { return index == other.index; }
	inline bool operator!=(const var& other) const { return index != other.index; }	

	friend std::ostream& operator<< (std::ostream& out, const var& t);
	
  };

  
  struct app {
	app(ref<mono> from, ref<mono> to) : from(from), to(to) { }
	ref<mono> from, to;

	bool operator<(const app& other) const;
	bool operator==(const app& other) const;
	

	friend std::ostream& operator<< (std::ostream& out, const app& t);
  };

  // TODO other type constructors, add func

  // TODO univ/exist ?
  struct forall;
  
  using poly = variant< mono, forall >;

  
  struct forall {
	vec<var> args;
	ref<poly> body;

	
	friend std::ostream& operator<< (std::ostream& out, const forall& t);
	
  };


}


#endif
