
QMAKE_CXX = clang++

QMAKE_CXXFLAGS += -std=c++11

CONFIG -= qt app_bundle
# CONFIG += link_pkgconfig

SOURCES = sexpr.cpp ast.cpp type.cpp syntax.cpp hindley_milner.cpp main.cpp
LIBS += -lreadline

CONFIG = debug
