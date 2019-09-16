CXX ?= c++
CXXFLAGS := -I ../include/ -std=c++11 -fno-omit-frame-pointer

test: test_ordered_map test_util_string test_offset

test_shared_ptr: build/test_shared_ptr
	build/test_shared_ptr

test_ordered_map: build/test_ordered_map
	build/test_ordered_map

test_util_string: build/test_util_string
	build/test_util_string

test_source_data: build/test_source_data
	build/test_source_data

test_lurlparser: build/test_lurlparser
	build/test_lurlparser

test_offset: build/test_offset
	build/test_offset

build:
	@mkdir build

build/test_shared_ptr: test_shared_ptr.cpp ../src/memory/SharedPtr.cpp | build
	$(CXX) $(CXXFLAGS) ../src/allocator.cpp ../src/memory/SharedPtr.cpp -o build/test_shared_ptr test_shared_ptr.cpp

build/test_ordered_map: test_ordered_map.cpp ../src/memory/SharedPtr.cpp | build
	$(CXX) $(CXXFLAGS) ../src/allocator.cpp -o build/test_ordered_map test_ordered_map.cpp

build/test_util_string: test_util_string.cpp ../src/util_string.cpp | build
	$(CXX) $(CXXFLAGS) ../src/allocator.cpp ../src/util_string.cpp -o build/test_util_string test_util_string.cpp

build/test_source_data: test_source_data.cpp ../src/source.cpp ../src/offset.cpp ../src/position.cpp | build
	$(CXX) $(CXXFLAGS) ../src/allocator.cpp ../src/randomize.cpp -o build/test_source_data test_source_data.cpp ../src/source.cpp ../src/source_span.cpp ../src/source_state.cpp ../src/offset.cpp ../src/position.cpp

build/test_lurlparser: test_lurlparser.cpp ../src/LUrlParser/LUrlParser.cpp | build
	$(CXX) $(CXXFLAGS) -o build/test_lurlparser test_lurlparser.cpp ../src/LUrlParser/LUrlParser.cpp

build/test_offset: test_offset.cpp ../src/offset.cpp | build
	$(CXX) $(CXXFLAGS) ../src/allocator.cpp -o build/test_offset test_offset.cpp ../src/offset.cpp

clean: | build
	rm -rf build

.PHONY: test test_shared_ptr test_ordered_map test_util_string test_source_data test_lurlparser test_offset clean
