#------------------------------------------------------------------------------
#   make
#   make run ARGS="configs/lru.cfg"
#   make gtest
#   make testrun
#   make check
#   make e2e
#   make clean
#
#   make gtest GTEST_ARGS="--gtest_filter=ArcCacheTest.*"
#   make OUT_O_DIR=debug CXXFLAGS="-g -O0" gtest
#   make CXX=clang++
#------------------------------------------------------------------------------

.DEFAULT_GOAL := all

ifeq ($(origin CXX), default)
  CXX := g++
endif

CXXFLAGS ?= -O2 -g \
            -Wshadow -Wconversion -Wsign-conversion \
            -Wnon-virtual-dtor -Woverloaded-virtual \
            -Wold-style-cast -Wcast-qual -Wcast-align \
            -Wformat=2 -Wunused -Wnull-dereference \
            -Werror

CPPFLAGS ?=
LDFLAGS  ?=
LDLIBS   ?=

OUT_O_DIR ?= build
SRC_DIR   ?= source
TEST_DIR  ?= tests
INC_DIR   ?= include

TARGET := $(OUT_O_DIR)/cache
GTEST_BIN := $(OUT_O_DIR)/$(TEST_DIR)/google_tests

GTEST_LDLIBS ?= -lgtest_main -lgtest -pthread
GTEST_ARGS ?=

override CPPFLAGS += -I$(INC_DIR)
override CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic

#------------------------------------------------------------------------------

SRC := $(wildcard $(SRC_DIR)/*.cpp)
MAIN_SRC := $(SRC_DIR)/main.cpp
COMMON_SRC := $(filter-out $(MAIN_SRC),$(SRC))

SRC_OBJ := $(patsubst %.cpp,$(OUT_O_DIR)/%.o,$(SRC))
COMMON_OBJ := $(patsubst %.cpp,$(OUT_O_DIR)/%.o,$(COMMON_SRC))

GTEST_SRC := $(wildcard $(TEST_DIR)/*_gtest.cpp)
GTEST_OBJ := $(patsubst %.cpp,$(OUT_O_DIR)/%.o,$(GTEST_SRC))

DEPS := $(sort $(SRC_OBJ:.o=.d) $(GTEST_OBJ:.o=.d))

#------------------------------------------------------------------------------

.PHONY: all
all: $(TARGET)

$(TARGET): $(SRC_OBJ)
	@mkdir -p $(@D)
	$(CXX) $^ -o $@ $(LDFLAGS) $(LDLIBS)

# Общее правило компиляции с зависимостями от заголовков.
$(OUT_O_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

#------------------------------------------------------------------------------
# Google Test
#------------------------------------------------------------------------------

.PHONY: gtest testrun check

ifneq ($(strip $(GTEST_SRC)),)

$(GTEST_OBJ): override CXXFLAGS += -pthread

$(GTEST_BIN): $(GTEST_OBJ) $(COMMON_OBJ)
	@mkdir -p $(@D)
	$(CXX) $^ -o $@ $(LDFLAGS) $(LDLIBS) $(GTEST_LDLIBS)

gtest: $(GTEST_BIN)
	$(GTEST_BIN) $(GTEST_ARGS)

else

gtest:
	@echo "No Google Test sources found: $(TEST_DIR)/*_gtest.cpp"
	@exit 1

endif

testrun: gtest
check: gtest


#------------------------------------------------------------------------------

.PHONY: run
run: $(TARGET)
	$(TARGET) $(ARGS)

.PHONY: e2e
e2e: $(TARGET)
	@bash $(TEST_DIR)/run_e2e.sh

#------------------------------------------------------------------------------

.PHONY: clean
clean:
	rm -rf -- "$(OUT_O_DIR)"

-include $(DEPS)