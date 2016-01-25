#ifndef VARIANT_HPP
#define VARIANT_HPP

#include <new>
#include <stdexcept>
#include <cassert>

template<class T, T x> struct value_type {
  static const T value = x;
};

template<unsigned x>
using unsigned_type = value_type<unsigned, x>;

template<unsigned ... I> struct maximum;

template<unsigned I> struct maximum<I> : unsigned_type<I> { };

template<unsigned I, unsigned ... Rest> struct maximum<I, Rest...> {
  static constexpr unsigned sub = maximum<Rest...>::value;
  static constexpr unsigned value = I > sub ? I : sub;
};


template<unsigned I, class ... H> struct type_at;

template<class T, class ... H > struct type_at<0, T, H...> {
  typedef T type;
};

template<unsigned I, class T, class ... H >
struct type_at<I, T, H...> : type_at<I - 1, H...> { };

template<class ... Types> class variant;

// index of type in a variant through constexpr function (ADL):
// unsigned index = get( index_of<variant_type, type>{} );

// note: ADL is used to avoid horrible recursive error messages:
// functions are first built in a recursive way, but only the actual
// function call will trigger the error, if any.
template<class T, class Variant, int I = 0> struct index_of;

template<class T, class ... Tail, int I >
struct index_of< T, variant<T, Tail...>, I > {
  friend constexpr unsigned get(index_of) { return I; }
};

template<class ... Tail, class H, class T, int I>
struct index_of< T, variant<H, Tail...>, I > : index_of< T, variant<Tail...>, I + 1> { };

template<class T, int I>
struct index_of< T, variant<>, I > {

  template<class U>  struct error {
	static const bool value = false;
  };
  
  friend constexpr unsigned get(index_of) {
	static_assert( error<T>::value, "type does not belong to variant");
	return -1;
  }

};



template<class ... Types>
class variant {

  // TODO this should be the size of Types... and checks >=
  // uninitialized
  static const unsigned uninitialized = -1;
  
  unsigned id = uninitialized;
  
  static constexpr unsigned data_size = maximum< sizeof(Types)...>::value;
  char data[data_size];


  struct destruct {

	template<class T>
	void operator()(T& self) const {
	  self.~T();
	}
	
  };


  template<class From>
  struct construct {

	// TODO deal with alignment
	void operator()(From& self, const From& other) const {
	  new((void*)&self) From(other);
	}

	template<class T>
	void operator()(T&, const From& other) const {
	  throw std::logic_error("should never be called");
	}
	
  };
  

  struct copy_construct {

	template<class T>
	void operator()(T& self, const variant& other) const {
	  construct<T>()(self, other.as<T>());
	}
	
  };

  struct equals {

	template<class T>
	bool operator()(const T& self, const variant& other) const {
	  return self == other.as<T>();
	}
	
  };

  struct compare {

	template<class T>
	bool operator()(const T& self, const variant& other) const {
	  return self < other.as<T>();
	}
	
  };

  template<class Out>
  struct ostream {

	template<class T>
	Out& operator()(const T& self, Out& out) const {
	  return out << self;
	}
	
  };
  

  struct copy_assign {

	template<class T>
	void operator()(T& self, const variant& other) const {
	  self = other.as<T>();
	}
	
  };

  
  template<class T, class Ret, class F, class ...Args>
  inline Ret apply_const_thunk(const F& f, Args&& ... args) const {
	return f( as<T>(), std::forward<Args>(args)... );
  }

  template<class T, class Ret, class F, class ...Args>
  inline Ret apply_thunk(const F& f, Args&& ... args) {
	return f( as<T>(), std::forward<Args>(args)... );
  }

  
  template<class Ret, class F, class ... Args>
  using apply_const_type = Ret (variant::*)(const F&, Args&& ... ) const;

  template<class Ret, class F, class ... Args>
  using apply_type = Ret (variant::*)(const F&, Args&& ... );
  
public:
  
  template<class T>
  variant(const T& other) : id( get( index_of<T, variant>{} ) ) {
	apply( construct<T>(), other );
  }

  variant(const variant& other) : id(other.id) {
	if( id == uninitialized ) return;

	apply( copy_construct(), other);
  }
  
  ~variant() {
	if( id == uninitialized ) return;
	apply( destruct() );
  }

  variant() : id(uninitialized) { }

  struct error : std::runtime_error {
	error() : std::runtime_error("cast error")  { }
  };


  // TODO move assignement/constructors
  
  variant& operator=(const variant& other) {

	// different types ?
	if( id != other.id ) {

	  // destroy if needed
	  if( id != uninitialized ) {
		apply( destruct() );
	  }

	  // destructed, now change type
	  id = other.id;

	  // construct if needed
	  if( other.id != uninitialized ) {
		apply( copy_construct(), other );
	  }
	  
	} else {

	  // copy assign if needed
	  if( id != uninitialized ) {
		apply( copy_assign(), other);
	  }

	}

  	return *this;
  }
  
  
  // query
  template<class T>
  bool is() const {
	return type() == variant::type<T>();
  };

  // cast
  template<class T>
  T& as() {
	if( !is<T>() ) throw error();
	return *reinterpret_cast<T*>(data);
  }

  template<class T>
  const T& as() const {
	if( !is<T>() ) throw error();
	return *reinterpret_cast<const T*>(data);
  }

  // TODO unsafe casts ?

  
  unsigned type() const {
	assert( id != uninitialized );	
	return id;
  }

  template<class T>
  static constexpr unsigned type() {
	return get( index_of<T, variant>{} );
  }

  bool operator<(const variant& other) const {
	// TODO do we want to compare uninitalized values ?
	
	if( id < other.id ) return true;
	if( id != other.id ) return false;
	if( id == uninitialized ) return false;

	return apply<bool>(compare(), other);
  }

  inline bool operator!=(const variant& other) const {
	return ! (*this == other);
  }
  
  inline bool operator==(const variant& other) const {

	// TODO do we want to compare uninitalized values ?
	if( id != other.id ) return false;
	if( id == uninitialized ) return true;

	return apply<bool>(equals(), other);
  }


  template<class Out>
  friend Out& operator<<(Out& out, const variant& v) {

	if( v.id == uninitialized ) {
	  return out << "*uninitialized*";
	} else {
	  return v.apply<Out&>( ostream<Out>(), out );
	}
	
  };


  // static type switch from a function object f: (const T&, Args&&...) -> Ret
  template<class Ret = void, class F, class ... Args>
  inline Ret apply(const F& f, Args&& ... args ) const {
	if( id == uninitialized ) throw error();

	static apply_const_type<Ret, F, Args...> app[] = { &variant::apply_const_thunk<Types>... };
	return (this->*app[id])(f, std::forward<Args>(args)...);
  }


  // static type switch from a function object f: (T&, Args&&...) -> Ret
  template<class Ret = void, class F, class ... Args>
  inline Ret apply(const F& f, Args&& ... args ) {
	if( id == uninitialized ) throw error();

	static apply_type<Ret, F, Args...> app[] = { &variant::apply_thunk<Types>... };
	return (this->*app[id])(f, std::forward<Args>(args)...);
  }
  
 
  
};


#endif
