#ifndef DEBUG_HPP
#define DEBUG_HPP

#ifndef NDEBUG
#include <iostream>

static inline std::ostream& debug(int delta = 0) {
  static int indent = 0;

  indent += delta;
  indent = std::max(indent, 0);

  auto& stream = std::cout;
  stream << "[debug] ";

  for(int i = 0; i < indent; ++i) {
	stream << "  ";
  }
  
  return stream;
}

#else

struct sink_stream {
  
  template<class T>
  inline const sink_stream& operator<<(T&& other) const { return *this; }
  inline const sink_stream& operator<<( std::ostream& (*)(std::ostream&) ) const { return *this; }
};

static inline sink_stream debug() { return {}; }

#endif






#endif
