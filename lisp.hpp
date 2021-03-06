#ifndef LISP_HPP
#define LISP_HPP

#include "variant.hpp"
#include "sexpr.hpp"

#include <map>
#include <unordered_map>
#include <initializer_list>

namespace lisp {

  // values
  struct value;

  using boolean = sexpr::boolean;
  using integer = sexpr::integer;
  using real = sexpr::real;
  
  // wrap these to avoid too large value sizes
  // TODO boost::intrusive_ptr instead of ref<> and make it 2 words  
  using string = ref<sexpr::string>;

  struct cons;
  using list = ref< cons >;

  struct lambda_type;
  using lambda = ref<lambda_type>;
  
  class environment_type;
  using environment = ref<environment_type>;

  using builtin = value (*)(environment& env, value* first, value* last);

  struct closure_type;
  using closure = ref<closure_type>;
  
  struct object_type;
  using object = ref<object_type>;
  
  struct value : variant<boolean, integer, real, symbol, string, list, lambda, environment, builtin, object, closure> {
	using variant::variant;

	friend std::ostream& operator<<(std::ostream& out, const value& );
  };

  // make sure we don't accidentally form too large value type
  static_assert( sizeof(value) <= 3 * sizeof( void* ), "too big yo");

  struct error : std::runtime_error {
	using std::runtime_error::runtime_error;
	std::string details;
  };

  
  class environment_type : public std::enable_shared_from_this<environment_type>,
						   protected std::unordered_map<symbol, value> {
	environment parent;
  public:

	environment_type(environment parent = nullptr) : parent(parent) { }

	template<class SIterator, class VIterator>
	environment augment(SIterator sfirst, SIterator slast,
						VIterator vfirst, VIterator vlast) {
	  assert( slast - sfirst == vlast - vfirst );
	  
	  environment res = std::make_shared<environment_type>( shared_from_this() );
	  
	  for(; sfirst != slast && vfirst != vlast; ++sfirst, ++vfirst) {
		res->insert( {*sfirst, *vfirst} );
	  }

	  return res;
	}

	struct key_error : std::runtime_error {
	  key_error() : std::runtime_error("key not found") { }
	};

	typedef environment_type::unordered_map base;

	// TODO rename 
	template<class Fail>
	inline mapped_type& find(key_type key, Fail&& fail = {} ) {
	  
	  auto it = base::find(key);
	  if( it != end() ) {
		return it->second;
	  }

	  if( !parent ) {
		fail();
		throw key_error();
	  }
	  
	  return parent->find(key, std::forward<Fail>(fail));
	}


	
	using environment_type::base::operator[];
	using environment_type::base::insert;
	
	friend std::ostream& operator<<(std::ostream& out, const environment_type& env);
  };

  
  struct lambda_type {
	lambda_type();
   
	environment env;
	vec<symbol> args;
	ref<symbol> vararg;
	value body;
  };
  

  struct closure_type {
	ref<void> data;
	value (*func)(environment& env, void* data, value* first, value* last) = nullptr;

	closure_type() { };

	template<class F>
	closure_type(F&& f) {

	  data = std::make_shared<F>(std::forward(f));
	  func = [](environment& env, void* data, value* first, value* last) {
		using func_type = typename std::decay<F>::type;
		return reinterpret_cast< func_type* >(data)->operator()(env, first, last);
	  };
	  
	};

  };

  
  
  // testing stuff
  struct object_type : std::unordered_map< symbol, value > {
	symbol type;
	object_type(symbol type = "object", std::initializer_list<value_type> list = {});
  };

  
  // build a value from a pure symbolic expression
  value convert(const sexpr::expr& expr);
  
  // evaluate expression
  value eval(environment& env, const value& expr);

  // apply expr to (evaluated) args
  value apply(environment& env, const value& expr, value* arg, value* end);



  // lists
  struct cons {
	value head;
	list tail = nullptr;

	cons(const value& head, list tail = nullptr) : head(head), tail(tail) { }
	
	// range-based loops
	friend inline list begin(list x) { return x; }
	friend inline list end(list ) { return nullptr; }
	friend inline list& operator++(list& x) { x = x->tail; return x; }
	friend inline value& operator*(list x) { return x->head; };

	
	friend inline unsigned length(list x) {
	  if( !x ) return 0;
	  else return 1 + length(x->tail);
	}
	
	friend inline value& at(list x, unsigned n) {
	  if( !n ) { 
		assert(x);
		return x->head;
	  } else {
		return at(x, n - 1);
	  }
	}
	
  };

  template<class Iterator>
  inline list make_list(Iterator first, Iterator last) {
	if(first == last) {
	  return nullptr;
	} else {
	  const value& head = *first;
	  return std::make_shared<cons>(head, make_list(++first, last));
	}
  }


  static list null = nullptr;
  
}


#endif
