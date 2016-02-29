
QMAKE_CXXFLAGS += -std=c++11 # -I/usr/include/libcxxabi -stdlib=libc++


CONFIG -= qt app_bundle
CONFIG += warn_on

# CONFIG += link_pkgconfig
LLVM_CONFIG = llvm-config-3.8

INCLUDEPATH += $$system( $$LLVM_CONFIG --includedir)
LIBS += $$system( $$LLVM_CONFIG --ldflags)
LIBS += $$system( $$LLVM_CONFIG --libs) -ltinfo -lpthread -ldl -lz

SOURCES = common.cpp sexpr.cpp  \
		ast.cpp type.cpp  syntax.cpp hindley_milner.cpp \
		parse.cpp repl.cpp \
		code.cpp \
		jit.cpp \
# 		builtin.cpp \
		lisp.cpp \
		main.cpp

LIBS += -lreadline -lboost_program_options -lstdc++


