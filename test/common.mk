TARGET := test
TYPE := exec
CROSS_COMPILE :=

# Compiler can be overridden, e.g. to do clang build with gcc configuration
# COMPILER := gcc

FLAGS += -Wall -g -O2
CFLAGS := $(FLAGS)
CXXFLAGS := $(FLAGS) -std=gnu++20
LDFLAGS := -lgtest -lgtest_main
CFLAGS_gcc :=
CFLAGS_clang :=
CXXFLAGS_gcc :=
CXXFLAGS_clang :=
LDFLAGS_gcc :=
LDFLAGS_clang :=

INCLUDE := \
	$(CONFIG_APEXDIR)/test \
	$(CONFIG_APEXDIR) \
	$(CONFIG_APEXDIR)/sys/include

SOURCES := \
	src/circular_buffer.cpp \
	src/init_rand.cpp \
	src/page.cpp \
