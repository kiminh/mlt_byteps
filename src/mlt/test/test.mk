.PHONY: debug_test clean_test

TEST_SRC = $(wildcard test/test_*.cc)
TEST = $(patsubst test/test_%.cc, test/test_%, $(TEST_SRC))

INCPATH += -I$(DEPS_PATH)/include
LDFLAGS += -L$(DEPS_PATH)/lib -lbenchmark -lpthread

-include make/deps.mk

test/%: test/%.cc $(OBJS) $(GOOGLE_BENCHMARK)
	$(CXX) $(INCPATH) -std=c++17 -MM -MT test/$* $< >test/$*.d
	$(CXX) $(INCPATH) $(CFLAGS) -o $@ $(filter %.cc %.a %.o, $^) $(LDFLAGS)

-include test/*.d

debug_test:
	@echo CFLAGS = $(CFLAGS)
	@echo INCPATH = $(INCPATH)
	@echo LDFLAGS = $(LDFLAGS)
	@echo OBJS = $(OBJS)
	@echo DEPS_PATH = $(DEPS_PATH)

clean_test:
	rm -rfv $(TEST)
	rm -rfv $(TEST:=.o) $(TEST:=.d)
