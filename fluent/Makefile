CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Iinclude
LDFLAGS  := -lpthread

SRCS := src/main.cpp src/value.cpp src/core.cpp src/exec.cpp \
        src/vars.cpp src/control.cpp src/io.cpp src/system.cpp src/fileio.cpp

TARGET := fluent

ifeq ($(OS),Windows_NT)
    TARGET := fluent.exe
    LDFLAGS += -lws2_32
endif

.PHONY: all clean install

all:
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(SRCS) -o build/$(TARGET) $(LDFLAGS)
	@echo ""
	@echo "✓ Build complete: build/$(TARGET)"

clean:
	rm -rf build

install: all
ifeq ($(OS),Windows_NT)
	copy build\$(TARGET) %USERPROFILE%\fluent\$(TARGET)
else
	sudo cp build/$(TARGET) /usr/local/bin/$(TARGET)
	@echo "✓ Installed to /usr/local/bin/$(TARGET)"
endif
