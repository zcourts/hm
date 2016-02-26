
SOURCES = toy.cpp


LLVM_CONFIG = llvm-config-3.8
INCLUDEPATH += $$system( $$LLVM_CONFIG --includedir)
LIBS += $$system( $$LLVM_CONFIG --ldflags)
LIBS += $$system( $$LLVM_CONFIG --libs) -ltinfo -lpthread -ldl -lz




