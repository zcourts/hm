#ifndef VLA_HPP
#define VLA_HPP

template<class T>
struct vla {
  T* const data;
  const std::size_t size;
  
  vla(void* data, std::size_t size)
	: data( (T*) data),
	  size(size) {

	// placement new
	for(unsigned i = 0; i < size; ++i) {
	  new ((void*) (this->data + i)) T;
	}
	
  }

  ~vla() {
	for(unsigned i = 0; i < size; ++i) {
	  data[size - 1 - i].~T();
	}
  
  }

  T* begin() const { return data; }
  T* end() const { return data + size; }
  
  
};


#define macro_vla(type, name, size)					\
  char name##_buffer[ (size) * sizeof(type) ];		\
  vla<type> name(name##_buffer, (size) );	\




#endif
