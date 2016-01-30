#ifndef LISP_HPP
#define LISP_HPP

#include "variant.hpp"
#include "sexpr.hpp"

#include <map>
#include <unordered_map>

namespace lisp {

  // values
  struct value;

  struct nil {
	inline bool operator==(const nil&) const { return true;}
  };
  
  using boolean = sexpr::boolean;
  using integer = sexpr::integer;
  using real = sexpr::real;
  
  // wrap these to avoid too large value sizes
  // TODO boost::intrusive_ptr instead of ref<> and make it 2 words  
  using string = ref<sexpr::string>;

  using list = ref< vec<value> >;
  
  struct lambda_type;
  using lambda = ref<lambda_type>;
  
  class environment_type;
  using environment = ref<environment_type>;

  using builtin = value (*)(environment env, value* first, value* last);

  struct object_type;
  using object = ref<object_type>;
  
  struct value : variant<nil, boolean, integer, real, symbol, string, list, lambda, environment, builtin, object> {
	using variant::variant;

	friend std::ostream& operator<<(std::ostream& out, const value& );
  };

  // make sure we don't accidentally form too large value type
  static_assert( sizeof(value) <= 3 * sizeof( void* ), "too big yo");

  struct error : std::runtime_error {
	using std::runtime_error::runtime_error;
  };

  
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

	struct key_error : std::runtime_error {
	  key_error() : std::runtime_error("key not found") { }
	};

	template<class Fail>
	inline mapped_type& find(key_type key, Fail&& fail = {} ) {
	  
	  auto it = environment_type::map::find(key);
	  if( it != end() ) {
		return it->second;
	  }

	  if( !parent ) {
		fail();
		throw key_error();
	  }
	  
	  return parent->find(key, std::forward<Fail>(fail));
	}

	using environment_type::map::operator[];

	friend std::ostream& operator<<(std::ostream& out, const environment_type& env);
  };

  
  struct lambda_type {
	lambda_type();
   
	environment env;
	vec<symbol> args;
	ref<symbol> vararg;
	value body;
  };
  

  // testing stuff
  struct object_type : std::unordered_map< symbol, value > {
	symbol type;
	object_type(symbol type = "object") : type(type) { }
  };

  
  // build a value from a pure symbolic expression
  value convert(const sexpr::expr& expr);
  
  // evaluate expression
  value eval(environment env, const value& expr);

  // apply expr to (evaluated) args
  value apply(environment env, const value& expr, value* arg, value* end);
  
}


#endif
