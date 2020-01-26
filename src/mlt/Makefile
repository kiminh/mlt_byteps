.PHONY: clean debug_build clean_all

CXX := g++
INCPATH := -I./include -I./
CFLAGS := -g -Wall -Werror -std=c++17
LDFLAGS := -lpthread

DEPS_PATH ?= $(shell pwd)/deps

DEBUG := 1
ifeq ($(DEBUG), 1)
	CFLAGS += -fsanitize=address -ftrapv -fstack-protector -DPRISM_LOG_STACK_TRACE -DPRISM_LOG_STACK_TRACE_SIZE=128
else
	CFLAGS += -O2 -DNDEBUG
endif

APP := build/server build/client build/test_mlt

#SRCS := $(shell find . -maxdepth 1 -type f -name "*.cc")
SRCS := $(wildcard *.cc)
#SRCS := app_context.cc
_OBJS := $(patsubst %,build/%,$(SRCS:.cc=.o))
OBJS := $(filter-out $(APP:=.o),$(_OBJS))

all: $(APP)

build/client: build/client.o $(OBJS)
	$(CXX) $(INCPATH) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/server: build/server.o $(OBJS)
	$(CXX) $(INCPATH) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/test_mlt: build/test_mlt.o $(OBJS)
	$(CXX) $(INCPATH) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/%.o: %.cc
	@mkdir -p $(@D)
	$(CXX) $(INCPATH) -std=c++17 -MM -MT build/$*.o $< >build/$*.d
	$(CXX) $(INCPATH) $(CFLAGS) -c $< -o $@

-include build/*.d
-include build/*/*.d

debug_build:
	@echo APP = $(APP)
	@echo SRCS = $(SRCS)
	@echo _OBJS = $(_OBJS)
	@echo OBJS = $(OBJS)
	@echo CFLAGS = $(CFLAGS)
	@echo INCPATH = $(INCPATH)
	@echo LDFLAGS = $(LDFLAGS)

clean:
	rm -rfv $(OBJS) $(APP:=.o)
	rm -rfv $(OBJS:.o=.d) $(APP:=.d)

-include test/test.mk
test: $(TEST)

clean_all: clean clean_test
	# rm -rf $(DEPS_PATH)
