#ifndef COMMON_HPP
#define COMMON_HPP

#include <memory>
#include <vector>


using symbol = const char*;
symbol symbolize(const std::string& );

template<class T>
using ref = std::shared_ptr<T>;

template<class T>
static ref<T> shared(const T& x = {}) {
  return std::make_shared<T>(x);
}

template<class T>
using vec = std::vector<T>;


#endif
