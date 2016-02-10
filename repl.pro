
QMAKE_CXXFLAGS += -std=c++11 # -I/usr/include/libcxxabi -stdlib=libc++

CONFIG -= qt app_bundle
CONFIG += warn_on

# CONFIG += link_pkgconfig

SOURCES = common.cpp sexpr.cpp  \
		ast.cpp type.cpp  syntax.cpp hindley_milner.cpp \
		parse.cpp repl.cpp \
		lisp.cpp \
		main.cpp

LIBS += -lreadline -lboost_program_options -lstdc++


# customization
include(config.pri) {
	# message('reading config.pri')
}
