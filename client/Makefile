# Makefile for cross-platform reverse shell client

# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# Source files
SOURCES = client.cpp

# Target executable
TARGET = client

# Platform-specific settings
ifeq ($(OS),Windows_NT)
    # Windows settings
    TARGET := $(TARGET).exe
    LDFLAGS = -lws2_32
    CXXFLAGS += -DWIN32
else
    # Linux/Unix settings
    LDFLAGS = -pthread
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        CXXFLAGS += -DLINUX
    endif
    ifeq ($(UNAME_S),Darwin)
        CXXFLAGS += -DMACOS
    endif
endif

# Default target
all: $(TARGET)

# Build the client
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

# Clean build artifacts
clean:
	rm -f $(TARGET) $(TARGET).exe

# Install dependencies (placeholder)
install-deps:
	@echo "Installing dependencies..."
	@echo "On Ubuntu/Debian: sudo apt-get install build-essential"
	@echo "On CentOS/RHEL: sudo yum groupinstall 'Development Tools'"
	@echo "On Windows: Install MinGW or Visual Studio"

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# Release build
release: CXXFLAGS += -DNDEBUG
release: $(TARGET)

# Help
help:
	@echo "Available targets:"
	@echo "  all       - Build the client (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  debug     - Build with debug symbols"
	@echo "  release   - Build optimized release version"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  ./client [server_ip] [server_port]"
	@echo "  Default: ./client 127.0.0.1 9999"

.PHONY: all clean install-deps debug release help