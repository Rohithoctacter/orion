CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
CFLAGS = -std=c99 -Wall -O2
LDFLAGS = -lm
TARGET = orion

ifdef FORCE_TARGET_WINDOWS
CXX = x86_64-w64-mingw32-g++
CC = x86_64-w64-mingw32-gcc
CXXFLAGS += -DTARGET_WINDOWS
CFLAGS += -DTARGET_WINDOWS
LDFLAGS = 
TARGET = orion-windows.exe
endif

ifdef FORCE_TARGET_LINUX
CXXFLAGS += -DTARGET_LINUX
CFLAGS += -DTARGET_LINUX
LDFLAGS = -lm
TARGET = orion-linux
endif

ifdef FORCE_TARGET_MACOS
CXXFLAGS += -DTARGET_MACOS
CFLAGS += -DTARGET_MACOS
LDFLAGS = -lm
TARGET = orion-macos
endif

SOURCES = main.cpp lexer.cpp types.cpp codegen.cpp ast_impl.cpp
OBJECTS = $(SOURCES:.cpp=.o)
C_OBJECTS = runtime.o
ALL_OBJECTS = $(OBJECTS) $(C_OBJECTS)

all: $(TARGET)

$(TARGET): $(ALL_OBJECTS)
	$(CXX) $(ALL_OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

runtime.o: runtime.c
	$(CC) $(CFLAGS) -c runtime.c -o runtime.o

clean:
	rm -f *.o orion orion-windows orion-windows.exe orion-linux orion-macos

windows:
	$(MAKE) FORCE_TARGET_WINDOWS=1 clean
	$(MAKE) FORCE_TARGET_WINDOWS=1

linux:
	$(MAKE) FORCE_TARGET_LINUX=1 clean $(TARGET)

.PHONY: all clean windows linux