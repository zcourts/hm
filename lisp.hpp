#ifndef LISP_HPP
#define LISP_HPP

#include "variant.hpp"
#include "sexpr.hpp"
#include <map>

namespace lisp {

  using real = sexpr::real;
  using string = ref<sexpr::string>;

  struct lambda_type;
  using lambda = ref<lambda_type>;
  
  struct environment_type;
  using environment = ref<environment_type>;

  struct nil { };

  // TODO boost::intrusive_ptr instead of ref<> and make it 2 words
  using value = variant<nil, bool, int, real, symbol, string, lambda, environment>;

  // make sure we don't accidentally get too large value type
  static_assert( sizeof(value) <= 3 * sizeof( void* ), "too big yo");

  struct error : std::runtime_error {
	using std::runtime_error::runtime_error;
  };  

  
  class environment_type : std::map<symbol, value> {
	environment parent;
  public:

	environment_type(environment parent = nullptr) : parent(parent) { }
	
	template<class SIterator, class VIterator>
	environment augment(SIterator sfirst, SIterator slast,
						VIterator vfirst, VIterator vlast) {
	  if( slast - sfirst != vlast - vfirst ) {
		throw error("bad argument count");
	  }
	  
	  environment res = std::make_shared<environment_type>( environment(this) );
	  
	  for(; sfirst != slast && vfirst != vlast; ++sfirst, ++vfirst) {
		res->insert( {*sfirst, *vfirst} );
	  }

	  return res;
	}
	
  };

  struct lambda_type {
	environment env;
	vec<symbol> args;
	sexpr::expr body;
  };

  // eval
  value eval(environment env, sexpr::expr expr);

  

}


#endif
