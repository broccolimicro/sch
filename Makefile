NAME          = sch
DEPEND        = interpret_phy phy boolean
TEST_DEPEND   = interpret_phy phy boolean

COVERAGE ?= 0

ifeq ($(COVERAGE),0)
CXXFLAGS = -std=c++20 -g -Wall -fmessage-length=0 -O2
LDFLAGS  =
else
CXXFLAGS = -std=c++20 -g -Wall -fmessage-length=0 -O0 --coverage -fprofile-arcs -ftest-coverage
LDFLAGS  = --coverage -fprofile-arcs -ftest-coverage 
endif

CXXFLAGS += -D CL_HPP_MINIMUM_OPENCL_VERSION=120 -D CL_HPP_TARGET_OPENCL_VERSION=120 -D CL_HPP_ENABLE_EXCEPTIONS 

PYTHON_RELEASE = python$(shell python3 -c "import sys;sys.stdout.write('{}.{}'.format(sys.version_info[0],sys.version_info[1]))")


SRCDIR        = $(NAME)
INCLUDE_PATHS = $(DEPEND:%=-I../%) -I../gdstk/build/include $(shell python3-config --includes) -I.
LIBRARY_PATHS = $(DEPEND:%=-L../%) -L$(shell python3-config --prefix)/lib -L.
LIBRARIES     = $(DEPEND:%=-l%) -l$(PYTHON_RELEASE)

SOURCES	     := $(shell mkdir -p $(SRCDIR); find $(SRCDIR) -name '*.cpp')
OBJECTS	     := $(SOURCES:%.cpp=build/%.o)
DEPS         := $(shell mkdir -p build/$(SRCDIR); find build/$(SRCDIR) -name '*.d')
TARGET	      = lib$(NAME).a

TESTDIR       = tests

ifndef GTEST
override GTEST=../../googletest
endif

TEST_INCLUDE_PATHS = -I$(GTEST)/googletest/include $(TEST_DEPEND:%=-I../%) -I../gdstk/include -I.
TEST_LIBRARY_PATHS = -L$(GTEST)/build/lib $(TEST_DEPEND:%=-L../%) -L.
TEST_LIBRARIES = -l$(NAME) $(TEST_DEPEND:%=-l%) -pthread -lgtest

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
    TEST_LIBRARIES += -l:libgdstk.a -l:libclipper.a -l:libqhullstatic_r.a -lz
    TEST_LIBRARY_PATHS += -L../gdstk/build/lib -L../gdstk/build/lib64
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        CXXFLAGS += -D LINUX
        TEST_LIBRARIES += -l:libgdstk.a -l:libclipper.a -l:libqhullstatic_r.a -lz -lOpenCL
        TEST_LIBRARY_PATHS += -L../gdstk/build/lib -L../gdstk/build/lib64
    endif
    ifeq ($(UNAME_S),Darwin)
        CXXFLAGS += -D OSX -mmacos-version-min=12.0 -Wno-missing-braces
        INCLUDE_PATHS += -I$(shell brew --prefix opencl-headers)/include -I$(shell brew --prefix opencl-clhpp-headers)/include -I$(shell brew --prefix qhull)/include
        TEST_LIBRARY_PATHS += -L../gdstk/build/lib -L$(shell brew --prefix qhull)/lib
        TEST_LIBRARIES += -lgdstk -lclipper -lqhullstatic_r -lz -framework OpenCL
        LDFLAGS	      += -Wl,-rpath,/opt/homebrew/opt/python@3.15/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.14/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.13/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.12/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.11/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.10/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.09/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python/Frameworks/Python.framework/Versions/Current/lib
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

coverage: clean
	$(MAKE) COVERAGE=1 tests
	./$(TEST_TARGET) || true  # Continue even if tests fail
	lcov --capture --directory build/$(SRCDIR) --output-file coverage.info
	lcov --ignore-errors unused --remove coverage.info '/usr/include/*' '*/googletest/*' '*/tests/*' --output-file coverage_filtered.info
	genhtml coverage_filtered.info --output-directory coverage_report

$(TARGET): $(OBJECTS)
	ar rvs $(TARGET) $(OBJECTS)

build/$(SRCDIR)/%.o: $(SRCDIR)/%.cpp 
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) -MM -MF $(patsubst %.o,%.d,$@) -MT $@ -c $<
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) -c -o $@ $<

sch/Placer.cpp: sch/Kernel.h

sch/Kernel.h: cl/placer.cpp
	printf "#pragma once\n\nnamespace sch {\n\nconst string placer_cpp_string = " > sch/Kernel.h
	cat cl/placer.cpp | sed 's/\\/\\\\/g;s/"/\\"/g;s/^/"/g;s/$$/\\n"/g' >> sch/Kernel.h
	printf ";\n}\n" >> sch/Kernel.h

$(TEST_TARGET): $(TEST_OBJECTS) $(OBJECTS) $(TARGET)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_LIBRARY_PATHS) $(TEST_OBJECTS) $(TEST_LIBRARIES) -o $(TEST_TARGET)

build/$(TESTDIR)/%.o: $(TESTDIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(TEST_INCLUDE_PATHS) -MM -MF $(patsubst %.o,%.d,$@) -MT $@ -c $<
	$(CXX) $(CXXFLAGS) $(TEST_INCLUDE_PATHS) $< -c -o $@

build/$(TESTDIR)/gtest_main.o: $(GTEST)/googletest/src/gtest_main.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(TEST_INCLUDE_PATHS) $< -c -o $@

include $(DEPS) $(TEST_DEPS)

clean:
	rm -rf sch/Kernel.h build $(TARGET) $(TEST_TARGET) coverage.info coverage_filtered.info coverage_report *.gcda *.gcno

clean-test:
	rm -rf build/$(TESTDIR) $(TEST_TARGET)

clean-coverage:
	rm -rf coverage.info coverage_filtered.info coverage_report *.gcda *.gcno
