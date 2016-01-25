#ifndef TYPE_HPP
#define TYPE_HPP

#include "common.hpp"
#include "variant.hpp"

#include <ostream>

// type-checking
namespace type {

  // monotypes
  struct lit;
  struct var;
  struct app;

  using mono = variant< app, var, lit >;
  
  struct lit {
	symbol name;

	inline bool operator<(const lit& other) const { return name < other.name; }
	inline bool operator==(const lit& other) const { return name == other.name; }

  };

  
  struct var {
	var(unsigned index = total++) : index(index) { }
	unsigned index;
	
	static unsigned total;

	inline bool operator<(const var& other) const { return index < other.index; }
	inline bool operator==(const var& other) const { return index == other.index; }
	inline bool operator!=(const var& other) const { return index != other.index; }	
	
  };

  
  struct app {
	ref<mono> from, to;
	
	bool operator<(const app& other) const;
	bool operator==(const app& other) const;
	
  };

  
  // TODO other type constructors, add func
  
  // polytypes
  struct forall;
  
  using poly = variant< mono, forall >;

  struct forall {
	vec<var> args;
	ref<poly> body;
  };

  std::ostream& operator<<(std::ostream& out, const poly& p);
}


#endif
