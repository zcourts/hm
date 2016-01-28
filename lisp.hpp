#ifndef LISP_HPP
#define LISP_HPP

#include "variant.hpp"
#include "sexpr.hpp"

#include <map>

namespace lisp {

  using real = sexpr::real;
  using string = ref<sexpr::string>;

  struct list_type;
  using list = ref<list_type>;
  
  struct lambda_type;
  using lambda = ref<lambda_type>;
  
  class environment_type;
  using environment = ref<environment_type>;

  struct builtin;
  
  struct nil { };

  // TODO boost::intrusive_ptr instead of ref<> and make it 2 words
  using value = variant<nil, bool, int, real, symbol, string, list, lambda, environment, builtin>;

  struct builtin {
	using type = value (*)(environment env, value* first, value* last);
	type ptr;
	
	builtin(type ptr = nullptr) : ptr(ptr) { }
  };
  
  // make sure we don't accidentally get too large value type
  static_assert( sizeof(value) <= 3 * sizeof( void* ), "too big yo");

  struct error : std::runtime_error {
	using std::runtime_error::runtime_error;
  };

  std::ostream& operator<<(std::ostream& out, const value& );

  class environment_type : public std::enable_shared_from_this<environment_type>,
						   protected std::map<symbol, value> {
	environment parent;
  public:

	environment_type(environment parent = nullptr) : parent(parent) { }
	
	template<class SIterator, class VIterator>
	environment augment(SIterator sfirst, SIterator slast,
						VIterator vfirst, VIterator vlast) {
	  if( slast - sfirst != vlast - vfirst ) {
		throw error("bad argument count");
	  }
	  
	  environment res = std::make_shared<environment_type>( shared_from_this() );
	  
	  for(; sfirst != slast && vfirst != vlast; ++sfirst, ++vfirst) {
		res->insert( {*sfirst, *vfirst} );
	  }

	  return res;
	}


	template<class Fail>
	inline mapped_type& find(key_type key, Fail&& fail = {} ) {
	  
	  auto it = environment_type::map::find(key);
	  if( it != end() ) {
		return it->second;
	  }

	  if( !parent ) fail();

	  return parent->find(key, std::forward<Fail>(fail));
	}

	using environment_type::map::operator[];
	
	
  };

  struct lambda_type {
	lambda_type();
   
	environment env;
	vec<symbol> args;
	sexpr::expr body;
  };

  struct list_type : vec<value> {
	using vec<value>::vec;
  };


  // transform expression to canonical form
  sexpr::expr canonicalize(const sexpr::expr& expr);
  
  // evaluates a sexpr in canonical form
  value eval(environment env, const sexpr::expr& expr);

  

}


#endif
