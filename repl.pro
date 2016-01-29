
QMAKE_CXX = clang++

QMAKE_CXXFLAGS += -std=c++11 # -I/usr/include/libcxxabi -stdlib=libc++

CONFIG -= qt app_bundle
CONFIG += warn_on

# CONFIG += link_pkgconfig

SOURCES = common.cpp sexpr.cpp ast.cpp type.cpp parse.cpp syntax.cpp hindley_milner.cpp main.cpp lisp.cpp repl.cpp
LIBS += -lreadline -lboost_program_options -lstdc++

CONFIG = debug
# DEFINES += LOG_LEVEL=2

release {
	# QMAKE_CXXFLAGS += -g
	DEFINES += NDEBUG
}
