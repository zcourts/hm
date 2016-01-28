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

  struct app_type;
  using app = ref<app_type>;
  
  using mono = variant< app, var, lit >;
  
  struct var {
	var(unsigned index = total++) : index(index) { }
	unsigned index;
	
	static unsigned total;

	inline bool operator<(const var& other) const { return index < other.index; }
	inline bool operator==(const var& other) const { return index == other.index; }
	inline bool operator!=(const var& other) const { return index != other.index; }	
	
  };


  // TODO name -> type func should be associated externally in a map ?
  // TODO merge lit with nullary type functions ?

  // type function
  struct abs : symbol {
	abs(const char* name, unsigned arity);
	unsigned arity() const;
  };

  
  // type application
  struct app_type {

	// arity check TODO move args
	app_type(const abs& func,
			 const vec<mono>& args);
	
	abs func;
	vec<mono> args;
	
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
  extern const lit integer;
  extern const lit boolean;
  extern const lit unit;

  // function type
  extern const abs func;
  

  // easy function types (right-associative)
  static inline app operator>>=(const mono& lhs, const mono& rhs) {
	app_type res = { func, {lhs, rhs} };
	return shared(res);
  }
  
}


#endif
