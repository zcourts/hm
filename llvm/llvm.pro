
SOURCES = main.cpp

CONFIG -= qt

# VERSION = 3.6

INCLUDEPATH += $$system(  llvm-config --includedir )

# QMAKE_CXX = clang++
# QMAKE_LINK = clang++

QMAKE_CXXFLAGS = -std=c++11

# QMAKE_CXXFLAGS += $$system( llvm-config-$$VERSION --cxxflags)


CLANG_LIBS = \
    clangFrontend clangSerialization clangDriver \
    clangTooling clangParse clangSema clangAnalysis \
    clangEdit clangAST clangLex clangBasic clangCodeGen  \
    LLVMMC LLVMMCParser LLVMObject LLVMAsmParser LLVMCore  \
	LLVMOption LLVMBitWriter LLVMBitReader LLVMSupport \

LIBS += -L$$system(llvm-config --libdir)

LIBS += -Wl,--start-group
for(CLANG_LIB, CLANG_LIBS) {
   LIBS += -l$${CLANG_LIB}
   }

LIBS +=  -lclang -lLLVM
LIBS += -Wl,--end-group

LIBS += -ldl -lpthread -lz -ltinfo -lstdc++ 

   
CONFIG += debug
CONFIG -= warn_on

