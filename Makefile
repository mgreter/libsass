OS       ?= $(shell uname -s)
CC       ?= cc
CXX      ?= c++
RM       ?= rm -f
CP       ?= cp -a
CHDIR    ?= cd
MKDIR    ?= mkdir
RMDIR    ?= rmdir
WINDRES  ?= windres
# Solaris/Illumos flavors
# ginstall from coreutils
ifeq ($(OS),SunOS)
INSTALL  ?= ginstall
endif
INSTALL  ?= install
CFLAGS   ?= -Wall
CXXFLAGS ?= -Wall
LDFLAGS  ?= -Wall
ifndef COVERAGE
  CFLAGS   += -O3 -pipe -DNDEBUG -fomit-frame-pointer
  CXXFLAGS += -O3 -pipe -DNDEBUG -fomit-frame-pointer
  LDFLAGS  += -O3 -pipe -DNDEBUG -fomit-frame-pointer
else
  CFLAGS   += -O1 -fno-omit-frame-pointer
  CXXFLAGS += -O1 -fno-omit-frame-pointer
  LDFLAGS  += -O1 -fno-omit-frame-pointer
endif
ifeq "$(LIBSASS_GPO)" "generate"
  CFLAGS   += -fprofile-instr-generate
  CXXFLAGS += -fprofile-instr-generate
  LDFLAGS  += -fprofile-instr-generate -Wl,-fprofile-instr-generate
endif
ifeq "$(LIBSASS_GPO)" "use"
  CFLAGS   += -fprofile-use
  CXXFLAGS += -fprofile-use
  LDFLAGS  += -fprofile-use -Wl,-fprofile-instr-use
endif
CAT ?= $(if $(filter $(OS),Windows_NT),type,cat)

ifdef WASM
  CFLAGS   += -D_WASM
  CXXFLAGS += -D_WASM
endif

ifdef WASM
	UNAME := WebAssembly
else
ifneq (,$(findstring /cygdrive/,$(PATH)))
	UNAME := Cygwin
else
ifneq (,$(findstring Windows_NT,$(OS)))
	UNAME := Windows
else
ifneq (,$(findstring mingw32,$(MAKE)))
	UNAME := Windows
else
ifneq (,$(findstring MINGW32,$(shell uname -s)))
	UNAME := Windows
else
	UNAME := $(shell uname -s)
endif
endif
endif
endif
endif

ifndef LIBSASS_VERSION
	ifneq ($(wildcard ./.git/ ),)
		LIBSASS_VERSION ?= $(shell git describe --abbrev=4 --dirty --always --tags)
	endif
	ifneq ($(wildcard VERSION),)
		LIBSASS_VERSION ?= $(shell $(CAT) VERSION)
	endif
endif
ifdef LIBSASS_VERSION
	CFLAGS   += -DLIBSASS_VERSION="\"$(LIBSASS_VERSION)\""
	CXXFLAGS += -DLIBSASS_VERSION="\"$(LIBSASS_VERSION)\""
endif

ifdef EMSCRIPTEN
  CXXFLAGS += -std=c++17
  LDFLAGS  += -std=c++17
else
  CXXFLAGS += -std=c++11
  LDFLAGS  += -std=c++11
endif

ifeq (Windows,$(UNAME))
	ifneq ($(BUILD),shared)
		STATIC_ALL     ?= 1
	endif
	STATIC_LIBGCC    ?= 1
	STATIC_LIBSTDCPP ?= 1
else
	STATIC_ALL       ?= 0
	STATIC_LIBGCC    ?= 0
	STATIC_LIBSTDCPP ?= 0
endif

ifndef SASS_LIBSASS_PATH
	SASS_LIBSASS_PATH = $(CURDIR)
endif
ifdef SASS_LIBSASS_PATH
	CFLAGS   += -I $(SASS_LIBSASS_PATH)/include
	CXXFLAGS += -I $(SASS_LIBSASS_PATH)/include
else
	# this is needed for mingw
	CFLAGS   += -I include
	CXXFLAGS += -I include
endif

CFLAGS   += $(EXTRA_CFLAGS)
CXXFLAGS += $(EXTRA_CXXFLAGS)
LDFLAGS  += $(EXTRA_LDFLAGS)

LDLIBS = -lm

# link statically into lib
# makes it a lot more portable
# increases size by about 50KB
ifeq ($(STATIC_ALL),1)
	LDFLAGS += -static
endif
ifeq ($(STATIC_LIBGCC),1)
	LDFLAGS += -static-libgcc
endif
ifeq ($(STATIC_LIBSTDCPP),1)
	LDFLAGS += -static-libstdc++
endif

ifeq ($(UNAME),Darwin)
	CFLAGS += -stdlib=libc++
	CXXFLAGS += -stdlib=libc++
	LDFLAGS += -stdlib=libc++
endif

ifneq (Windows,$(UNAME))
	ifneq (FreeBSD,$(UNAME))
		ifneq (OpenBSD,$(UNAME))
			ifndef WASM
				LDFLAGS += -ldl
				LDLIBS += -ldl
			endif
		endif
	endif
endif

ifneq ($(BUILD),shared)
	BUILD := static
endif
ifeq ($(DEBUG),1)
	BUILD := debug-$(BUILD)
endif

ifndef TRAVIS_BUILD_DIR
	ifeq ($(OS),SunOS)
		PREFIX ?= /opt/local
	else
		PREFIX ?= /usr/local
	endif
else
	PREFIX ?= $(TRAVIS_BUILD_DIR)
endif

SASS_SASSC_PATH ?= sassc
SASS_SPEC_PATH ?= sass-spec
SASS_SPEC_SPEC_DIR ?= spec
SASSC_BIN = $(SASS_SASSC_PATH)/bin/sassc
RUBY_BIN = ruby

RESOURCES =
STATICLIB = lib/libsass.a
SHAREDLIB = lib/libsass.so
LIB_STATIC = $(SASS_LIBSASS_PATH)/lib/libsass.a
LIB_SHARED = $(SASS_LIBSASS_PATH)/lib/libsass.so
ifeq ($(UNAME),Darwin)
	SHAREDLIB = lib/libsass.dylib
	LIB_SHARED = $(SASS_LIBSASS_PATH)/lib/libsass.dylib
endif
ifeq (Windows,$(UNAME))
	SASSC_BIN = $(SASS_SASSC_PATH)/bin/sassc.exe
	RESOURCES += res/resource.rc
	SHAREDLIB  = lib/libsass.dll
	ifeq (shared,$(BUILD))
		CFLAGS    += -D ADD_EXPORTS
		CXXFLAGS  += -D ADD_EXPORTS
		LIB_SHARED  = $(SASS_LIBSASS_PATH)/lib/libsass.dll
	endif
else
ifdef WASM
	SASSC_BIN = $(SASS_SASSC_PATH)/bin/sassc.wasm
	SHAREDLIB  = lib/libsass.wasm
	LIB_SHARED  = $(SASS_LIBSASS_PATH)/lib/libsass.wasm
else
ifneq (Cygwin,$(UNAME))
	CFLAGS   += -fPIC
	CXXFLAGS += -fPIC
	LDFLAGS  += -fPIC
endif
endif
endif

include Makefile.conf
OBJECTS = $(addprefix src/,$(SOURCES:.cpp=.o))
COBJECTS = $(addprefix src/,$(CSOURCES:.c=.o))
HEADOBJS = $(addprefix src/,$(HPPFILES:.hpp=.hpp.gch))
RCOBJECTS = $(RESOURCES:.rc=.o)

ifdef WASM
WASMOBJECTS = src/wasm/libcxxabi_stubs.o
endif

DEBUG_LVL ?= NONE

CLEANUPS ?=
CLEANUPS += $(RCOBJECTS)
CLEANUPS += $(COBJECTS)
CLEANUPS += $(OBJECTS)
CLEANUPS += $(HEADOBJS)
CLEANUPS += $(LIBSASS_LIB)

all: $(BUILD)

debug: $(BUILD)

debug-static: LDFLAGS := -g $(filter-out -O2,$(LDFLAGS))
debug-static: CFLAGS := -g -DDEBUG -DDEBUG_LVL="$(DEBUG_LVL)" $(filter-out -O2,$(CFLAGS))
debug-static: CXXFLAGS := -g -DDEBUG -DDEBUG_LVL="$(DEBUG_LVL)" $(filter-out -O2,$(CXXFLAGS))
debug-static: static

debug-shared: LDFLAGS := -g $(filter-out -O2,$(LDFLAGS))
debug-shared: CFLAGS := -g -DDEBUG -DDEBUG_LVL="$(DEBUG_LVL)" $(filter-out -O2,$(CFLAGS))
debug-shared: CXXFLAGS := -g -DDEBUG -DDEBUG_LVL="$(DEBUG_LVL)" $(filter-out -O2,$(CXXFLAGS))
debug-shared: shared

lib:
	$(MKDIR) lib

lib/wasm: lib
	$(CHDIR) lib && $(CHDIR) wasm || $(MKDIR) wasm

lib/asmjs: lib
	$(CHDIR) lib && $(CHDIR) asmjs || $(MKDIR) asmjs

lib/wasmjs: lib
	$(CHDIR) lib && $(CHDIR) wasmjs || $(MKDIR) wasmjs

nodejs/dist: 
	$(CHDIR) nodejs && $(CHDIR) dist || $(MKDIR) dist

lib/libsass.a: $(COBJECTS) $(OBJECTS) $(WASMOBJECTS) | lib
	$(AR) rcvs $@ $(COBJECTS) $(OBJECTS) $(WASMOBJECTS)

lib/libsass.so: $(COBJECTS) $(OBJECTS) | lib
	$(CXX) -shared $(LDFLAGS) -o $@ $(COBJECTS) $(OBJECTS) $(LDLIBS)

lib/libsass.dylib: $(COBJECTS) $(OBJECTS) | lib
	$(CXX) -shared $(LDFLAGS) -o $@ $(COBJECTS) $(OBJECTS) $(LDLIBS) \
	-install_name @rpath/libsass.dylib

lib/libsass.dll: $(COBJECTS) $(OBJECTS) $(RCOBJECTS) | lib
	$(CXX) -shared $(LDFLAGS) -o $@ $(COBJECTS) $(OBJECTS) $(RCOBJECTS) $(LDLIBS) \
	-s -Wl,--subsystem,windows,--out-implib,lib/libsass.a

lib/libsass.wasm: $(COBJECTS) $(OBJECTS) $(WASMOBJECTS) | lib
	$(CXX) $(LDFLAGS) -o $@ $(COBJECTS) $(OBJECTS) $(WASMOBJECTS) $(LDLIBS)

$(RCOBJECTS): %.o: %.rc
	$(WINDRES) -i $< -o $@

$(OBJECTS): %.o: %.cpp $(HEADOBJS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(COBJECTS): %.o: %.c $(HEADOBJS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(HEADOBJS): %.hpp.gch: %.hpp
	$(CXX) $(CXXFLAGS) -x c++-header -c -o $@ $<

%: %.o static
	$(CXX) $(CXXFLAGS) -o $@ $+ $(LDFLAGS) $(LDLIBS)

install: install-$(BUILD)

static: $(STATICLIB)
shared: $(SHAREDLIB)

$(DESTDIR)$(PREFIX):
	$(MKDIR) $(DESTDIR)$(PREFIX)

$(DESTDIR)$(PREFIX)/lib: | $(DESTDIR)$(PREFIX)
	$(MKDIR) $(DESTDIR)$(PREFIX)/lib

$(DESTDIR)$(PREFIX)/include: | $(DESTDIR)$(PREFIX)
	$(MKDIR) $(DESTDIR)$(PREFIX)/include

$(DESTDIR)$(PREFIX)/include/sass: | $(DESTDIR)$(PREFIX)/include
	$(MKDIR) $(DESTDIR)$(PREFIX)/include/sass

$(DESTDIR)$(PREFIX)/include/%.h: include/%.h \
                                 | $(DESTDIR)$(PREFIX)/include/sass
	$(INSTALL) -v -m0644 "$<" "$@"

install-headers: $(DESTDIR)$(PREFIX)/include/sass.h \
                 $(DESTDIR)$(PREFIX)/include/sass/base.h \
                 $(DESTDIR)$(PREFIX)/include/sass/version.h \
                 $(DESTDIR)$(PREFIX)/include/sass/values.h \
                 $(DESTDIR)$(PREFIX)/include/sass/context.h \
                 $(DESTDIR)$(PREFIX)/include/sass/functions.h

$(DESTDIR)$(PREFIX)/lib/%.a: lib/%.a \
                             | $(DESTDIR)$(PREFIX)/lib
	@$(INSTALL) -v -m0755 "$<" "$@"

$(DESTDIR)$(PREFIX)/lib/%.so: lib/%.so \
                             | $(DESTDIR)$(PREFIX)/lib
	@$(INSTALL) -v -m0755 "$<" "$@"

$(DESTDIR)$(PREFIX)/lib/%.dll: lib/%.dll \
                               | $(DESTDIR)$(PREFIX)/lib
	@$(INSTALL) -v -m0755 "$<" "$@"

$(DESTDIR)$(PREFIX)/lib/%.dylib: lib/%.dylib \
                               | $(DESTDIR)$(PREFIX)/lib
	@$(INSTALL) -v -m0755 "$<" "$@"

install-static: $(DESTDIR)$(PREFIX)/lib/libsass.a

install-shared: $(DESTDIR)$(PREFIX)/$(SHAREDLIB) \
                install-headers

$(SASSC_BIN): $(BUILD)
	$(MAKE) -C $(SASS_SASSC_PATH) build-$(BUILD)

sassc: $(SASSC_BIN)
ifndef WASM
	$(SASSC_BIN) -v

version: $(SASSC_BIN)
	$(SASSC_BIN) -v
endif

test: test_build

test_build: $(SASSC_BIN)
	$(RUBY_BIN) $(SASS_SPEC_PATH)/sass-spec.rb -c $(SASSC_BIN) --impl libsass \
	--cmd-args "-I $(SASS_SPEC_PATH)/$(SASS_SPEC_SPEC_DIR)" \
	$(LOG_FLAGS) $(SASS_SPEC_PATH)/$(SASS_SPEC_SPEC_DIR)

test_full: $(SASSC_BIN)
	$(RUBY_BIN) $(SASS_SPEC_PATH)/sass-spec.rb -c $(SASSC_BIN) --impl libsass \
	--cmd-args "-I $(SASS_SPEC_PATH)/$(SASS_SPEC_SPEC_DIR)" \
	--run-todo $(LOG_FLAGS) $(SASS_SPEC_PATH)/$(SASS_SPEC_SPEC_DIR)

test_probe: $(SASSC_BIN)
	$(RUBY_BIN) $(SASS_SPEC_PATH)/sass-spec.rb -c $(SASSC_BIN) --impl libsass \
	--cmd-args "-I $(SASS_SPEC_PATH)/$(SASS_SPEC_SPEC_DIR)" \
	--probe-todo $(LOG_FLAGS) $(SASS_SPEC_PATH)/$(SASS_SPEC_SPEC_DIR)

test_interactive: $(SASSC_BIN)
	$(RUBY_BIN) $(SASS_SPEC_PATH)/sass-spec.rb -c $(SASSC_BIN) --impl libsass \
	--cmd-args "-I $(SASS_SPEC_PATH)/$(SASS_SPEC_SPEC_DIR)" \
	--interactive $(LOG_FLAGS) $(SASS_SPEC_PATH)/$(SASS_SPEC_SPEC_DIR)

clean-objects: | lib
	-$(RM) lib/*.a lib/*.so lib/*.dll lib/*.dylib lib/*.la lib/*.wasm
	-$(RMDIR) lib
clean: clean-objects
	$(RM) $(CLEANUPS)

clean-all:
	$(MAKE) -C $(SASS_SASSC_PATH) clean

lib-file: lib-file-$(BUILD)
lib-opts: lib-opts-$(BUILD)

lib-file-static:
	@echo $(LIB_STATIC)
lib-file-shared:
	@echo $(LIB_SHARED)
lib-opts-static:
	@echo -L"$(SASS_LIBSASS_PATH)/lib"
lib-opts-shared:
	@echo -L"$(SASS_LIBSASS_PATH)/lib -lsass"

# Build WebAssembly with JS glue
wasmjs: static lib/wasmjs
	emcc lib/libsass.a -o lib/wasmjs/libsass.js \
		-O3 --llvm-lto 1 --js-opts 1 --closure 0 \
		-s MODULARIZE=1 \
		-s EXPORT_NAME=libsass \
		-s EXPORTED_FUNCTIONS="['_sass_compile_emscripten']" \
		-s EXTRA_EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap', 'stringToUTF8']" \
		-s ENVIRONMENT=node \
		-s NODERAWFS=1 \
		-s WASM=1 \
		-s ASSERTIONS=0 \
		-s STANDALONE_WASM=0 \
		-s WASM_OBJECT_FILES=0 \
		-s DISABLE_EXCEPTION_CATCHING=0 \
		-s NODEJS_CATCH_EXIT=0 \
		-s WASM_ASYNC_COMPILATION=0 \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s MALLOC=dlmalloc

# Build asm.js version (most compatible)
asmjs: static lib/asmjs
	emcc lib/libsass.a -o lib/asmjs/libsass.js \
		-O3 --llvm-lto 1 --js-opts 1 --closure 0 \
		-Wno-almost-asm \
		-s MODULARIZE=1 \
		-s EXPORT_NAME=libsass \
		-s EXPORTED_FUNCTIONS="['_sass_compile_emscripten']" \
		-s EXTRA_EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap', 'stringToUTF8']" \
		-s ENVIRONMENT=node \
		-s NODERAWFS=1 \
		-s ASSERTIONS=0 \
		-s WASM=0 \
		-s STANDALONE_WASM=0 \
		-s WASM_OBJECT_FILES=0 \
		-s DISABLE_EXCEPTION_CATCHING=0 \
		-s NODEJS_CATCH_EXIT=0 \
		-s WASM_ASYNC_COMPILATION=1 \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s MALLOC=dlmalloc

# Build standalone WebAssembly
# Probably needs emsdk-upstream
wasm: static lib/wasm
	emcc lib/libsass.a -o lib/wasm/libsass.js \
		-O3 --llvm-lto 1 --js-opts 1 --closure 0 \
		-s EXPORTED_FUNCTIONS="['_sass_compile_emscripten']" \
		-s EXTRA_EXPORTED_RUNTIME_METHODS="['ccall', 'cwrap', 'stringToUTF8']" \
		-s ENVIRONMENT=node \
		-s NODERAWFS=1 \
		-s WASM=1 \
		-s STANDALONE_WASM=1 \
		-s WASM_OBJECT_FILES=1 \
		-s DISABLE_EXCEPTION_CATCHING=0 \
		-s NODEJS_CATCH_EXIT=0 \
		-s WASM_ASYNC_COMPILATION=0

web: wasmjs asmjs

nodejs/src/entrypoint.o: %.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

nodejs/src/functions.o: %.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

nodejs/src/importers.o: %.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# asmjs wasmjs
nodejs: nodejs/dist/libsass.js nodejs/dist/libsass.asm.js

nodejs/dist/libsass.js: lib/libsass.a nodejs/src/entrypoint.o nodejs/src/functions.o nodejs/src/importers.o
	em++ nodejs/src/entrypoint.o nodejs/src/functions.o nodejs/src/importers.o lib/libsass.a -o nodejs/dist/libsass.js \
		-O3 --llvm-lto 1 --js-opts 1 --closure 0 \
		--llvm-lto 1 \
		--js-opts 2 \
		-Wno-almost-asm \
		-s WASM=1 \
		-s ENVIRONMENT=node \
		-s NODERAWFS=1 \
		-s ASSERTIONS=0 \
		-s STANDALONE_WASM=0 \
		-s WASM_OBJECT_FILES=0 \
		-s DISABLE_EXCEPTION_CATCHING=0 \
		-s NODEJS_CATCH_EXIT=0 \
		-s WASM_ASYNC_COMPILATION=0 \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s MALLOC=dlmalloc \
		--bind \
		--js-library nodejs/src/workaround8806.js
		
nodejs/dist/libsass.asm.js: lib/libsass.a nodejs/src/entrypoint.o nodejs/src/functions.o nodejs/src/importers.o
	em++ nodejs/src/entrypoint.o nodejs/src/functions.o nodejs/src/importers.o lib/libsass.a -o nodejs/dist/libsass.asm.js \
		-O3 --llvm-lto 1 --js-opts 1 --closure 0 \
		--llvm-lto 1 \
		--js-opts 2 \
		-Wno-almost-asm \
		-s WASM=0 \
		-s ENVIRONMENT=node \
		-s NODERAWFS=1 \
		-s ASSERTIONS=0 \
		-s STANDALONE_WASM=0 \
		-s WASM_OBJECT_FILES=0 \
		-s DISABLE_EXCEPTION_CATCHING=0 \
		-s NODEJS_CATCH_EXIT=0 \
		-s WASM_ASYNC_COMPILATION=0 \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s MALLOC=dlmalloc \
		--bind \
		--js-library nodejs/src/workaround8806.js

js-debug: static
	emcc lib/libsass.a -o lib/libsass.js \
		-O0 \
		-s EXPORTED_FUNCTIONS="['_sass_compile_emscripten']" \
		-s EXTRA_EXPORTED_RUNTIME_METHODS=@exported_runtime_methods.json \
		-s WASM=0 \
		-s ENVIRONMENT=node \
		-s NODERAWFS=1 \
		-s DISABLE_EXCEPTION_CATCHING=0 \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s LEGACY_VM_SUPPORT=1 \
		-s EMTERPRETIFY=1 \
		-s EMTERPRETIFY_ASYNC=1 \
		-s EMTERPRETIFY_WHITELIST=@emterpreter_whitelist.json \
		-s ASSERTIONS=1 \
		-s SAFE_HEAP=1 \
		-s DEMANGLE_SUPPORT=1 \
		--profiling-funcs \
		--minify 0 \
		--memory-init-file 0

.PHONY: all web static shared sassc \
        version install-headers \
        clean clean-all clean-objects \
        debug debug-static debug-shared \
        install install-static install-shared \
        lib-opts lib-opts-shared lib-opts-static \
        lib-file lib-file-shared lib-file-static \
        test test_build test_full test_probe
.DELETE_ON_ERROR:
