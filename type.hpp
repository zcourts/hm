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

    template<class ... Args>
    app operator()(Args&& ... args) const;
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

  // type schemes
  struct scheme {
	vec<var> args;
	mono body;
  };
  
  // polytypes
  using poly = variant< mono, scheme >;
  
  std::ostream& operator<<(std::ostream& out, const poly& p);

  // TODO not quite sure where this belongs
  // constant types for literals
  template<class T> struct traits;

  template<> struct traits<int> {

	static type::mono type() {
	  return lit{"int"};
	}
  };

  template<> struct traits<double> {

	static type::mono type() {
	  return lit{"real"};
	}
	
  };

  template<> struct traits<bool> {

	static type::mono type() {
	  return lit{"bool"};
	}
  };

  template<> struct traits<std::string> {

	static type::mono type() {
	  return lit{"string"};
	}
	
  };

  template<> struct traits<void> {
  
	static type::mono type() {
	  return lit{ "unit" };
	}
	
  };


  // some helpers
  extern const lit integer;
  extern const lit boolean;
  extern const lit unit;

  extern const abs io;
  extern const abs ref;  
  
  // function type
  extern const abs func;
  

  // easy function types (right-associative)
  static inline app operator>>=(const mono& lhs, const mono& rhs) {
	app_type res = { func, {lhs, rhs} };
	return shared<app_type>( std::move(res) );
  }


  template<class ... Args>
  app abs::operator()(Args&& ... args) const {
    vec<type::mono> unpack = {args...};
    auto res = shared<app_type>(*this, unpack);
    return res;
  }
}


#endif
