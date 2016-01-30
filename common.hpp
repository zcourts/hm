#ifndef COMMON_HPP
#define COMMON_HPP

#include <memory>
#include <vector>
#include <functional>

class symbol {
  const char* string = nullptr;
public:

  const char* name() const { return string; }
  
  symbol() { }
  symbol(const std::string& );
  symbol(const char* );

  inline bool operator<(const symbol& other) const { return string < other.string; }
  inline bool operator==(const symbol& other) const { return string == other.string; }
  inline bool operator!=(const symbol& other) const { return string != other.string; }  

  friend std::ostream& operator << (std::ostream& out, const symbol& self);
};

template<class T>
using ref = std::shared_ptr<T>;

template<class T>
static ref<T> shared(const T& x = {}) {
  return std::make_shared<T>(x);
}

template<class T>
using vec = std::vector<T>;

namespace std {

  template<class T>
  struct hash;

  template<>
  struct hash< symbol > {

	inline std::size_t operator()(const symbol& s) const {
	  return std::hash<const char*>{}(s.name());
	}
  };
}

#endif
