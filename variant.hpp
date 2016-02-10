#ifndef VARIANT_HPP
#define VARIANT_HPP

#include <new>
#include <stdexcept>
#include <cassert>

template<class ... Types> class variant;

// some helpers
namespace impl {
  
 
  // type at a given index in a parameter pack
  template<unsigned I, class ... H> struct type_at;

  template<class T, class ... H > struct type_at<0, T, H...> {
	typedef T type;
  };

  template<unsigned I, class T, class ... H >
  struct type_at<I, T, H...> : type_at<I - 1, H...> { };


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

	template<class U> struct error : std::false_type { };
  
	friend constexpr unsigned get( index_of ) {
	  static_assert( error<T>::value, "type does not belong to variant");
	  return sizeof(T);
	}

  };

}



template<class ... Types>
class variant {

  typedef long id_type;
  static const id_type uninitialized = -1;
  
  id_type id = uninitialized;
  
  typedef typename std::aligned_union<0, Types...>::type data_type;
  data_type data;


  struct destruct {

	template<class T>
	void operator()(T& self) const {
	  self.~T();
	}
	
  };


  struct construct {

	template<class T>
	void operator()(T& self, const T& other) const {
	  new((void*)&self) T(other);
	}

	template<class T>
	void operator()(T& self, T&& other) const {
	  new((void*)&self) T( std::move(other));
	}

	template<class T>
	void operator()(T& self, const variant& other) const {
	  assert(other.id == type_index<T>());
	  (*this)(self, other.as<T>() );
	}

	template<class T>
	void operator()(T& self, variant&& other) const {
	  assert(other.id == type_index<T>());
	  (*this)(self, std::move(other.as<T>()) );
	}
	
	
	template<class T, class U>
	void operator()(T&, U&& ) const {
	  throw std::logic_error("should never be called");
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

 
  

  struct assign {

	template<class T>
	void operator()(T& self, const variant& other) const {
	  self = other.as<T>();
	}

	template<class T>
	void operator()(T& self, variant&& other) const {
	  self = std::move( other.as<T>() );
	}


  };

  
  template<class T, class Self, class Ret, class F, class ...Args>
  static inline Ret thunk(Self&& self, const F& f, Args&& ... args) {
	return f( std::forward<Self>(self).template as<T>(), std::forward<Args>(args)... );
  }
  
  
 
  template<class Ret, class Self, class F, class ... Args>
  using thunk_type = Ret (*)(Self&& , const F&, Args&& ... );
  

  template<class T>
  static constexpr id_type type_index() {
	return get( impl::index_of<T, variant>{} );
  }

  
  template<class Variant>
  inline variant& assign_impl(Variant&& other) {
	if( id != other.id ) {
	  // change types: destruct/construct
	  
	  this->~variant();
	  new (this) variant(std::forward<Variant>(other));

	} else if( *this ) {
	  // assign 
	  apply( assign(), std::forward<Variant>(other) );
	}

  	return *this;
  }

  template<class Ret = void, class Self, class F, class ... Args>
  inline static Ret apply_impl(Self&& self, const F& f, Args&& ... args ) {
	assert( self );
	
	static thunk_type<Ret, Self, F, Args...> table[sizeof...(Types)] = { &variant::thunk<Types>... };
	return table[self.id](std::forward<Self>(self), f, std::forward<Args>(args)...);
  }


  
public:
  
  template<class T>
  variant(const T& other) : id( type_index<T>() ) {
	construct().template operator()(as<T>(), other);
  }
  
  
  variant(const variant& other) : id(other.id) {
	if( !*this ) return;

	apply( construct(), other);
  }

  variant(variant&& other) : id(other.id) {
	if( !*this ) return;

	apply( construct(), std::move(other));
  }
  
  ~variant() {
	if( !*this ) return;
	apply( destruct() );
  }
  
  variant() : id(uninitialized) { }

  struct error : std::runtime_error {
	error() : std::runtime_error("cast error")  { }
  };

  
  variant& operator=(const variant& other) {
	return assign_impl(other);
  }


  variant& operator=(variant&& other) {
	return assign_impl(std::move(other));
  }
  
  
  // query
  template<class T>
  bool is() const {
	static constexpr id_type type = type_index<T>();
	return id == type;
  };
  
  // cast
  template<class T>
  T& as() {
	assert( is<T>() && "cast error" );
	return reinterpret_cast<T&>(data);
  }

  template<class T>
  const T& as() const {
	assert( is<T>() && "cast error" );	
	return reinterpret_cast<const T&>(data);
  }

  
  bool operator!=(const variant& other) const {
  	return !(*this == other);
  }


  template<class Ret = void, class F, class ... Args>
  inline Ret apply(const F& f, Args&& ... args ) const {
	return apply_impl<Ret>(*this, f, std::forward<Args>(args)...);
  }


  template<class Ret = void, class F, class ... Args>
  inline Ret apply(const F& f, Args&& ... args )  {
	return apply_impl<Ret>(*this, f, std::forward<Args>(args)...);
  }
  
  
  bool operator==(const variant& other) const {

	// TODO do we want to compare uninitalized values ?
	if( id != other.id ) return false;
	if( !*this ) return true;

	return apply<bool>(equals(), other);
  }

  bool operator<(const variant& other) const {
    if( id < other.id ) return true;
    if( id == other.id && apply<bool>(compare(), other)) return true;
    return false;
  }


  explicit operator bool() const { return id != uninitialized; }

};


#endif


