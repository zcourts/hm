#ifndef TYPE_HPP
#define TYPE_HPP

#include "common.hpp"
#include "variant.hpp"

#include <ostream>

// type-checking
namespace type {

  // monotypes
  struct lit : symbol {
	using symbol::symbol;
  };
  
  struct var;
  struct app;

  using mono = variant< app, var, lit >;
  
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

  std::ostream& operator<<(std::ostream& out, const mono& p);  
  
  // TODO other type constructors, add func
  
  // polytypes
  struct forall;
  
  using poly = variant< mono, forall >;

  struct forall {
	vec<var> args;
	ref<poly> body;
  };

  std::ostream& operator<<(std::ostream& out, const poly& p);

  // TODO not quite sure where this belongs
  // constant types for literals
  template<class T> struct traits;

  template<> struct traits<int> {

	static type::lit type() {
	  return {"int"};
	}
  };

  template<> struct traits<double> {

	static type::lit type() {
	  return {"real"};
	}
  };

  template<> struct traits<bool> {

	static type::lit type() {
	  return {"bool"};
	}
  };

  template<> struct traits<std::string> {

	static type::lit type() {
	  return {"string"};
	}
	
  };

  template<> struct traits<void> {
  
	static type::lit type() {
	  return { "unit" };
	}
	
  };


  // some helpers
  static const type::lit integer = traits<int>::type();
  static const type::lit boolean = traits<bool>::type();
  static const type::lit unit = traits<void>::type();


  // easy function types
  static inline type::app operator>>(type::mono lhs, type::mono rhs) {
	return { shared(lhs), shared(rhs) };
  }
  
}


#endif
