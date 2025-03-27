PYTHON_RELEASE = python$(shell python3 -c "import sys;sys.stdout.write('{}.{}'.format(sys.version_info[0],sys.version_info[1]))")

NAME          = sch
DEPEND        = interpret_phy phy boolean

SRCDIR        = $(NAME)
TESTDIR       = tests
GTEST        := ../../googletest
GTEST_I      := -I$(GTEST)/googletest/include -I.
GTEST_L      := -L$(GTEST)/build/lib -L.

INCLUDE_PATHS = $(DEPEND:%=-I../%) -I../gdstk/build/include $(shell python3-config --includes) -I.
LIBRARY_PATHS = $(DEPEND:%=-L../%) -L$(shell python3-config --prefix)/lib -L.
LIBRARIES     = $(DEPEND:%=-l%) -l$(PYTHON_RELEASE)
LIBFILES      = $(foreach dep,$(DEPEND),../$(dep)/lib$(dep).a)
CXXFLAGS      = -std=c++17 -O2 -g -Wall -fmessage-length=0 -D CL_HPP_MINIMUM_OPENCL_VERSION=120 -D CL_HPP_TARGET_OPENCL_VERSION=120 -D CL_HPP_ENABLE_EXCEPTIONS $(DEPEND:%=-I../%) -I../gdstk/include -I.
LDFLAGS       =  

SOURCES	     := $(shell mkdir -p $(SRCDIR); find $(SRCDIR) -name '*.cpp')
OBJECTS	     := $(SOURCES:%.cpp=build/%.o)
DEPS         := $(shell mkdir -p build/$(SRCDIR); find build/$(SRCDIR) -name '*.d')
TARGET        = lib$(NAME).a

TESTS        := $(shell mkdir -p $(TESTDIR); find $(TESTDIR) -name '*.cpp')
TEST_OBJECTS := $(TESTS:%.cpp=build/%.o) build/$(TESTDIR)/gtest_main.o
TEST_DEPS    := $(shell mkdir -p build/$(TESTDIR); find build/$(TESTDIR) -name '*.d')
TEST_TARGET   = test

ifeq ($(OS),Windows_NT)
    CXXFLAGS += -D WIN32
    LIBRARIES += -lOpenCL
    ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
        CXXFLAGS += -D AMD64
    else
        ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
            CXXFLAGS += -D AMD64
        endif
        ifeq ($(PROCESSOR_ARCHITECTURE),x86)
            CXXFLAGS += -D IA32
        endif
    endif
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        CXXFLAGS += -D LINUX
        LIBRARIES += -lOpenCL
    endif
    ifeq ($(UNAME_S),Darwin)
        CXXFLAGS += -D OSX -mmacos-version-min=12.0 -Wno-varargs
	INCLUDE_PATHS += -I$(shell brew --prefix opencl-headers)/include -I$(shell brew --prefix opencl-clhpp-headers)/include
        LIBRARIES += -framework OpenCL
    endif
    UNAME_P := $(shell uname -p)
    ifeq ($(UNAME_P),x86_64)
        CXXFLAGS += -D AMD64
    endif
    ifneq ($(filter %86,$(UNAME_P)),)
        CXXFLAGS += -D IA32
    endif
    ifneq ($(filter arm%,$(UNAME_P)),)
        CXXFLAGS += -D ARM
    endif
endif


all: lib

lib: $(TARGET)

tests: lib $(TEST_TARGET)

$(TARGET): $(OBJECTS)
	ar rvs $(TARGET) $(OBJECTS)

build/$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(INCLUDE_PATHS) $(CXXFLAGS) $(LDFLAGS) -MM -MF $(patsubst %.o,%.d,$@) -MT $@ -c $<
	$(CXX) $(INCLUDE_PATHS) $(CXXFLAGS) $(LDFLAGS) -c -o $@ $<

sch/Placer.cpp: sch/Kernel.h

sch/Kernel.h: cl/placer.cpp
	printf "#pragma once\n\nnamespace sch {\n\nconst string placer_cpp_string = " > sch/Kernel.h
	cat cl/placer.cpp | sed 's/\\/\\\\/g;s/"/\\"/g;s/^/"/g;s/$$/\\n"/g' >> sch/Kernel.h
	printf ";\n}\n" >> sch/Kernel.h

$(TEST_TARGET): $(TEST_OBJECTS) $(TARGET) $(LIBFILES)
	$(CXX) $(LIBRARY_PATHS) $(GTEST_L) $(CXXFLAGS) $(LDFLAGS) $(TEST_OBJECTS) -o $(TEST_TARGET) -pthread -l$(NAME) -lgtest $(LIBRARIES)

build/$(TESTDIR)/%.o: $(TESTDIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(INCLUDE_PATHS) $(CXXFLAGS) $(GTEST_I) -MM -MF $(patsubst %.o,%.d,$@) -MT $@ -c $<
	$(CXX) $(INCLUDE_PATHS) $(CXXFLAGS) $(GTEST_I) $< -c -o $@

build/$(TESTDIR)/gtest_main.o: $(GTEST)/googletest/src/gtest_main.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(GTEST_I) $< -c -o $@

include $(DEPS) $(TEST_DEPS)

clean:
	rm -rf sch/Kernel.h build $(TARGET) $(TEST_TARGET)
