#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <iostream>

#ifndef LOG_LEVEL
#define LOG_LEVEL 0
#endif


namespace impl {

  struct sink_stream {
  
	template<class T>
	inline const sink_stream& operator<<(T&& other) const { return *this; }
	inline const sink_stream& operator<<( std::ostream& (*)(std::ostream&) ) const { return *this; }
  };

  template<int LogLevel, bool = LOG_LEVEL >= LogLevel> struct debug_traits;

  template<int Level> struct debug_traits<Level, true> {
	using type = std::ostream&;



	static inline std::ostream& debug(int delta) {
	  auto& stream = std::cout;
	  static int indent = 0;
	  
	  indent += delta;
	  indent = std::max(indent, 0);
  
	  stream << "[debug] ";
	
	  for(int i = 0; i < indent; ++i) {
		stream << "  ";
	  }
  
	  return stream;
	}

	
  };

  template<int Level> struct debug_traits<Level, false> {
	using type = sink_stream;

	static inline sink_stream debug(int) { return {}; }
  };
  
}


template<int Level = 0>
static inline typename impl::debug_traits<Level>::type debug(int indent = 0) {
  return impl::debug_traits<Level>::debug(indent);
}




#endif
