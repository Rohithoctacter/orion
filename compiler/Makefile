# Orion Compiler Cross-Platform Makefile

# Detect current platform for default behavior
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Default platform-specific settings
CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS = -lm
TARGET = orion
PLATFORM_DEFINES = 

# Platform detection and configuration
ifeq ($(UNAME_S),Linux)
    PLATFORM_DEFINES = -DTARGET_LINUX
    LDFLAGS = -lm
    TARGET = orion
endif

ifeq ($(UNAME_S),Darwin)
    PLATFORM_DEFINES = -DTARGET_MACOS
    CXX = clang++
    CC = clang
    LDFLAGS = -lm
    TARGET = orion
endif

# Windows detection (MSYS2/MinGW-w64)
ifneq (,$(findstring MINGW,$(UNAME_S)))
    PLATFORM_DEFINES = -DTARGET_WINDOWS
    LDFLAGS =    # No -lm on Windows
    TARGET = orion.exe
endif

# Override for cross-compilation targets
ifdef FORCE_TARGET_WINDOWS
    PLATFORM_DEFINES = -DTARGET_WINDOWS
    LDFLAGS =    # No -lm on Windows
    TARGET = orion-windows
endif

ifdef FORCE_TARGET_LINUX
    PLATFORM_DEFINES = -DTARGET_LINUX
    LDFLAGS = -lm
    TARGET = orion-linux
endif

ifdef FORCE_TARGET_MACOS
    PLATFORM_DEFINES = -DTARGET_MACOS
    LDFLAGS = -lm
    TARGET = orion-macos
endif

# Add platform defines to compilation flags
CXXFLAGS += $(PLATFORM_DEFINES)

# Source files
SOURCES = main.cpp lexer.cpp types.cpp codegen.cpp ast_impl.cpp
OBJECTS = $(SOURCES:.cpp=.o)
C_SOURCES = runtime.c
C_OBJECTS = $(C_SOURCES:.c=.o)
ALL_OBJECTS = $(OBJECTS) $(C_OBJECTS)

# Default target
all: $(TARGET)

# Build the compiler
$(TARGET): $(ALL_OBJECTS)
        $(CXX) $(ALL_OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile individual source files
%.o: %.cpp ast.h lexer.h
        $(CXX) $(CXXFLAGS) -c $< -o $@

# Compile C runtime files
%.o: %.c
        gcc -std=c99 -Wall -O2 -c $< -o $@

# Clean build artifacts
clean:
        rm -f $(OBJECTS) $(C_OBJECTS) $(TARGET) orion-windows orion-linux orion-macos

# Cross-compilation targets
windows:
        @echo "Building Windows-targeting compiler..."
        $(MAKE) FORCE_TARGET_WINDOWS=1 clean $(TARGET)
        @echo "Windows compiler built as: $(TARGET)"

linux:
        @echo "Building Linux-targeting compiler..."  
        $(MAKE) FORCE_TARGET_LINUX=1 clean $(TARGET)
        @echo "Linux compiler built as: $(TARGET)"

macos:
        @echo "Building macOS-targeting compiler..."
        $(MAKE) FORCE_TARGET_MACOS=1 clean $(TARGET)
        @echo "macOS compiler built as: $(TARGET)"

# Show current platform configuration
info:
        @echo "Platform: $(UNAME_S)"
        @echo "Target: $(TARGET)"
        @echo "Defines: $(PLATFORM_DEFINES)"
        @echo "CXX: $(CXX)"
        @echo "LDFLAGS: $(LDFLAGS)"

# Install the compiler (optional)
install: $(TARGET)
        cp $(TARGET) /usr/local/bin/

# Uninstall the compiler
uninstall:
        rm -f /usr/local/bin/$(TARGET)

# Run tests (placeholder)
test: $(TARGET)
        @echo "Running tests..."
        @echo "Tests would go here"

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# Profile build
profile: CXXFLAGS += -pg
profile: LDFLAGS += -pg
profile: $(TARGET)

# Dependencies
main.o: main.cpp ast.h lexer.h
lexer.o: lexer.cpp lexer.h
types.o: types.cpp ast.h
codegen.o: codegen.cpp ast.h
ast_impl.o: ast_impl.cpp ast.h

.PHONY: all clean install uninstall test debug profile windows linux macos info