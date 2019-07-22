CXX ?= c++
CXXFLAGS := -I ../include/ -std=c++11 -fno-omit-frame-pointer

test: test_shared_ptr test_util_string test_lurlparser

test_shared_ptr: build/test_shared_ptr
	build/test_shared_ptr

test_util_string: build/test_util_string
	build/test_util_string

test_lurlparser: build/test_lurlparser
	build/test_lurlparser

build:
	@mkdir build

build/test_shared_ptr: test_shared_ptr.cpp ../src/memory/SharedPtr.cpp | build
	$(CXX) $(CXXFLAGS) -o build/test_shared_ptr test_shared_ptr.cpp ../src/memory/SharedPtr.cpp

build/test_util_string: test_util_string.cpp ../src/util_string.cpp | build
	$(CXX) $(CXXFLAGS) -o build/test_util_string test_util_string.cpp ../src/util_string.cpp

build/test_lurlparser: test_lurlparser.cpp ../src/LUrlParser/LUrlParser.cpp | build
	$(CXX) $(CXXFLAGS) -o build/test_lurlparser test_lurlparser.cpp ../src/LUrlParser/LUrlParser.cpp

clean: | build
	rm -rf build

.PHONY: test test_shared_ptr test_util_string test_lurlparser clean
