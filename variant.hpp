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


template<class X, class ... In> struct index_of {
  static_assert(sizeof...(In), "type does not belong to variant");
};

template<class X, class ... Rest> struct index_of<X, X, Rest...> : unsigned_type<0> { };

template<class X, class H, class ... Rest> struct index_of<X, H, Rest...>
  : unsigned_type<1 + index_of<X, Rest...>::value > { };


template<unsigned I, class ... H> struct type_at;

template<class T, class ... H > struct type_at<0, T, H...> {
  typedef T type;
};

template<unsigned I, class T, class ... H >
struct type_at<I, T, H...> : type_at<I - 1, H...> { };



template<class ... Types>
class variant {

  // TODO this should be the size of Types... and checks >=
  // uninitialized
  static const unsigned uninitialized = -1;
  
  unsigned id = uninitialized;
  
  static constexpr unsigned data_size = maximum< sizeof(Types)...>::value;
  char data[data_size];
  
  template<class T>
  inline void destruct() {
	((T*)data)->~T();
  }
  
  template<class T>
  inline void construct(const T& from) {
	new((void*)data) T(from);
  }

  template<class T>
  inline void copy(const variant& from) {
	construct<T>(from.as<T>());
  };

  template<class T>
  inline bool compare(const variant& to) const {
	return as<T>() < to.as<T>();
  };


  template<class T>
  inline bool equals(const variant& to) const {
	return as<T>() == to.as<T>();
  };
  


  template<class T>
  inline void copy_assign(const variant& to) {
	as<T>() = to.as<T>();
  };

  template<class Out, class T>
  inline void ostream(Out& out) const {
	out << as<T>();
  }

  template<class T, class Ret, class F, class ...Args>
  inline Ret apply_thunk(const F& f, Args&& ... args) const {
	return f( as<T>(), std::forward<Args>(args)... );
  }
  
  using destruct_type = void ( variant::*) ();  
  using copy_type = void ( variant::*) (const variant& );
  using copy_assign_type = void ( variant::*) (const variant& );
  using compare_type = bool ( variant::*) (const variant& ) const;  

  template<class Out>
  using ostream_type = void (variant::*)(Out& ) const;

  template<class In>
  using istream_type = void (variant::*)(In& );

  template<class Ret, class F, class ... Args>
  using apply_type = Ret (variant::*)(const F&, Args&& ... ) const;
  
public:
  
  template<class T>
  variant(const T& other) : id( index_of<T, Types...>::value ) {
	construct<T>(other);
  }

  variant(const variant& other) : id(other.id) {

	if( id != uninitialized ) { 
	  static copy_type cpy[] = { &variant::copy<Types>... };
	  (this->*cpy[id])(other);
	}
  }
  
  ~variant() {
	if( id == uninitialized ) return;
	
	static destruct_type dtor[] = { &variant::destruct<Types>... };
	(this->*dtor[id])();
  }

  variant() : id(uninitialized) { }

  struct error : std::runtime_error {
	error() : std::runtime_error("cast error")  { }
  };


  variant& operator=(const variant& other) {
	// assert(other.id != uninitialized );
	
  	if( id == uninitialized ) {
  	  id = other.id;

	  // contruct
  	  if( other.id != uninitialized ) {
  		static copy_type cpy[] = { &variant::copy<Types>... };
  		(this->*cpy[id])(other);
  	  }
  	} else {

	  // same type: assign
  	  if( other.id == id ) {
  		static copy_assign_type cpy[] = { &variant::copy_assign<Types>... };
  		(this->*cpy[id])(other);
  	  } else {
  		// type change: destruct then construct
		static destruct_type dtor[] = { &variant::destruct<Types>... };
  		(this->*dtor[id])();

		id = other.id;

		if( other.id != uninitialized ) {
		  static copy_type cpy[] = { &variant::copy<Types>... };
		  (this->*cpy[id])(other);
		}
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

  // TODO unsafe casts

  
  unsigned type() const {
	assert( id != uninitialized );	
	return id;
  }

  template<class T>
  static constexpr unsigned type() {
	return index_of<T, Types...>::value;
  }

  bool operator<(const variant& other) const {
	if( id < other.id ) return true;
	if( id != other.id ) return false;

	if( id == uninitialized ) return false;
	static compare_type cmp[] = { &variant::compare<Types>...};
	return (this->*cmp[id])(other);
  }

  bool operator!=(const variant& other) const {
	return ! (*this == other);
  }
  
  bool operator==(const variant& other) const {
	if( id != other.id ) return false;

	// TODO ?
	if( id == uninitialized ) return true;
	
	static compare_type cmp[] = { &variant::equals<Types>...};
	return (this->*cmp[id])(other);
  }


  template<class Out>
  friend Out& operator<<(Out& out, const variant& v) {

	if( v.id != uninitialized ) {
	  static ostream_type<Out> str[] = { &variant::ostream<Out, Types>...};
	  (v.*str[v.id])(out);
	} else {
	  out << "*uninitialized*";
	}
	return out;
  };


  template<class Ret = void, class F, class ... Args>
  Ret apply(const F& f, Args&& ... args ) const {
	if( id == uninitialized ) throw error();

	static apply_type<Ret, F, Args...> app[] = { &variant::apply_thunk<Types>... };
	return (this->*app[id])(f, std::forward<Args>(args)...);
  }
  
 
  
};


#endif
