# Compiler and flags
CXX := g++
BASE_CXXFLAGS := -Wall -Wextra -std=c++17 -Isrc -Iinclude

# -O3 only for main driver
ifeq ($(DRIVER),main)
CXXFLAGS := $(BASE_CXXFLAGS) -O3
else
CXXFLAGS := $(BASE_CXXFLAGS) -O3
endif

# Folders
SRC_DIR := src
DRIVER_DIR := drivers
BUILD_DIR := build
BIN_DIR := bin

# Library sources (all .cpp in src/)
LIB_SRCS := $(wildcard $(SRC_DIR)/*.cpp)
LIB_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(LIB_SRCS))

# Default driver (override: make DRIVER=test_main)
DRIVER ?= main
DRIVER_SRC := $(DRIVER_DIR)/$(DRIVER).cpp
DRIVER_OBJ := $(BUILD_DIR)/$(DRIVER).o
TARGET := $(BIN_DIR)/$(DRIVER)

# Build all
all: $(TARGET)

# Link library + driver into final executable
$(TARGET): $(LIB_OBJS) $(DRIVER_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Compile library sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile driver source
$(BUILD_DIR)/%.o: $(DRIVER_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create directories if they don't exist
$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean