HOST_CC ?= cc
FISICS_CC ?= /Users/calebsv/Desktop/CodeWork/fisiCs/fisics
BUILD_TOOLCHAIN ?= clang
PACKAGE_TOOLCHAIN ?= $(BUILD_TOOLCHAIN)
TEST_TOOLCHAIN ?= clang
RELEASE_TOOLCHAIN ?= clang
PKG_CONFIG ?= pkg-config
TARGET_CONTRACT_HELPER ?= ../bin/desktop_release_target_contract.sh
SUPPORTED_TOOLCHAINS := clang fisics

ifeq ($(filter $(BUILD_TOOLCHAIN),$(SUPPORTED_TOOLCHAINS)),)
$(error Unsupported BUILD_TOOLCHAIN '$(BUILD_TOOLCHAIN)' (expected one of: $(SUPPORTED_TOOLCHAINS)))
endif
ifeq ($(filter $(PACKAGE_TOOLCHAIN),$(SUPPORTED_TOOLCHAINS)),)
$(error Unsupported PACKAGE_TOOLCHAIN '$(PACKAGE_TOOLCHAIN)' (expected one of: $(SUPPORTED_TOOLCHAINS)))
endif
ifeq ($(filter $(TEST_TOOLCHAIN),$(SUPPORTED_TOOLCHAINS)),)
$(error Unsupported TEST_TOOLCHAIN '$(TEST_TOOLCHAIN)' (expected one of: $(SUPPORTED_TOOLCHAINS)))
endif
ifeq ($(filter $(RELEASE_TOOLCHAIN),$(SUPPORTED_TOOLCHAINS)),)
$(error Unsupported RELEASE_TOOLCHAIN '$(RELEASE_TOOLCHAIN)' (expected one of: $(SUPPORTED_TOOLCHAINS)))
endif

# Build the application when running plain `make`.
.DEFAULT_GOAL := app
