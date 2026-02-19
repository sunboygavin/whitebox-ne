# Makefile for OpenConfig Adapter
# 
# This Makefile compiles the FRR to YANG and YANG to FRR adapters
# 
# Author: WhiteBox NE Team
# Date: 2024-02-20

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -I/usr/include/sysrepo
LDFLAGS = -lsysrepo

# Source files
FRR_TO_YANG_SRC = frr_to_yang.c
YANG_TO_FRR_SRC = yang_to_frr.c

# Output binaries
FRR_TO_YANG_BIN = frr_to_yang_adapter
YANG_TO_FRR_BIN = yang_to_frr_adapter

# Default target
all: $(FRR_TO_YANG_BIN) $(YANG_TO_FRR_BIN)

# Build FRR to YANG adapter
$(FRR_TO_YANG_BIN): $(FRR_TO_YANG_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built $(FRR_TO_YANG_BIN)"

# Build YANG to FRR adapter
$(YANG_TO_FRR_BIN): $(YANG_TO_FRR_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built $(YANG_TO_FRR_BIN)"

# Clean build artifacts
clean:
	rm -f $(FRR_TO_YANG_BIN) $(YANG_TO_FRR_BIN)
	@echo "Cleaned build artifacts"

# Install binaries to system
install: all
	install -m 755 $(FRR_TO_YANG_BIN) /usr/local/bin/
	install -m 755 $(YANG_TO_FRR_BIN) /usr/local/bin/
	@echo "Installed binaries to /usr/local/bin/"

# Uninstall binaries from system
uninstall:
	rm -f /usr/local/bin/$(FRR_TO_YANG_BIN)
	rm -f /usr/local/bin/$(YANG_TO_FRR_BIN)
	@echo "Removed binaries from /usr/local/bin/"

# Help target
help:
	@echo "OpenConfig Adapter Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Build all adapter binaries (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Build and install binaries to /usr/local/bin/"
	@echo "  uninstall  - Remove binaries from /usr/local/bin/"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  - gcc compiler"
	@echo "  - Sysrepo development libraries"
	@echo ""
	@echo "Usage:"
	@echo "  make          # Build all adapters"
	@echo "  make install  # Build and install"
	@echo "  make clean    # Clean up"

.PHONY: all clean install uninstall help
