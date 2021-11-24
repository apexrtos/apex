#
# The Apex make system
#

#
# This file probably shouldn't be edited unless you are really sure of what you are doing.
#
# Externally provided variables are in CAPITALS.
#

MAKECMDGOALS ?=

# Useful tools
SED := sed

# Extensions for C++ files
CXX_EXT := cpp cc C cxx c++

# Some useful constants for string and variable manipulation in gnu make
empty :=
space := $(empty) $(empty)
comma := ,

# Search path for sources & output (if Apex is a subdir)
ifeq ($(CONFIG_SRCDIR),$(CONFIG_APEXDIR))
    APEX_SUBDIR :=
    VPATH := $(CONFIG_SRCDIR)
else
    APEX_SUBDIR := $(subst $(CONFIG_SRCDIR)/,,$(CONFIG_APEXDIR))/
    VPATH := $(CONFIG_SRCDIR):$(CONFIG_APEXDIR):$(APEX_SUBDIR)
endif

# Expand prerequisite lists
.SECONDEXPANSION:

# Keep intermediate files
.SECONDARY:

# Phony target to force rule execution
.PHONY: force

# Clear optional configuration variables
ifeq ($(origin CONFIG_DEFS),undefined)
    CONFIG_DEFS :=
endif
ifeq ($(origin CONFIG_MAP),undefined)
    CONFIG_MAP :=
endif
ifeq ($(origin CONFIG_CXXFLAGS),undefined)
    CONFIG_CXXFLAGS :=
endif
ifeq ($(origin CONFIG_CFLAGS),undefined)
    CONFIG_CFLAGS :=
endif
ifeq ($(origin CONFIG_ASFLAGS),undefined)
    CONFIG_ASFLAGS :=
endif
ifeq ($(origin CONFIG_SSTRIP),undefined)
    CONFIG_SSTRIP :=
endif
ifeq ($(origin CONFIG_CROSS_COMPILE),undefined)
    CONFIG_CROSS_COMPILE :=
endif
ifeq ($(origin CONFIG_CFLAGS_$(CONFIG_COMPILER)),undefined)
    CONFIG_CFLAGS_$(CONFIG_COMPILER) :=
endif
ifeq ($(origin CONFIG_CXXFLAGS_$(CONFIG_COMPILER)),undefined)
    CONFIG_CXXFLAGS_$(CONFIG_COMPILER) :=
endif
ifeq ($(origin CONFIG_DEFS_$(CONFIG_COMPILER)),undefined)
    CONFIG_DEFS_$(CONFIG_COMPILER) :=
endif
ifeq ($(origin CONFIG_LDFLAGS),undefined)
    CONFIG_LDFLAGS :=
endif
ifeq ($(origin CONFIG_LDFLAGS_$(CONFIG_COMPILER)),undefined)
    CONFIG_LDFLAGS_$(CONFIG_COMPILER) :=
endif
ifeq ($(origin CONFIG_USER_CFLAGS),undefined)
    CONFIG_USER_CFLAGS :=
endif

#
# Make paths relative to $(CONFIG_SRCDIR) or absolute
#
# Synopsis: $(call fn_relative_path,<base>,<path_list>)
#
define fn_relative_path
$(filter-out $(CONFIG_SRCDIR),$(patsubst $(CONFIG_SRCDIR)/%,%,$(abspath $(call fn_path_join,$(1),$(2)))))
endef

#
# Join paths
#
# Synopsis $(call fn_path_join,<base>,<path_list>)
#
define fn_path_join
$(strip $(addprefix $(1),$(filter-out /%,$(2))) $(filter /%,$(2)))
endef

#
# Rule to create a directory
#
%/:
	mkdir -p $@

#
# Rule to check & update flags
#
# Synopsis $(eval $(call fn_flags_rule,<flags_file>,<flags>)
#
define fn_flags_rule
    # fn_flags_rule

    $(1): force | $$(CURDIR)/$$$$(dir $$$$@)
	$$(if $$(or $$(filter undefined,$$(origin $$@)),$$(subst x$$($$@),,x$$(strip $(2)))$$(subst x$$(strip $(2)),,x$$($$@))),$$(file >$$@,$$@ := $$(strip $(2))),)
endef

#
# Function to process SOURCES, INCLUDE, CFLAGS, CXXFLAGS, ASFLAGS variables
#
define fn_process_sources
    # fn_process_sources

    ifeq ($$(origin SOURCES),undefined)
        $$(error SOURCES is not defined)
    endif
    ifeq ($$(origin INCLUDE),undefined)
        INCLUDE :=
    endif

    # set target variables
    $(tgt)_CFLAGS := $$(CFLAGS)
    $(tgt)_CXXFLAGS := $$(CXXFLAGS) $$(CXXFLAGS_$$(COMPILER))
    $(tgt)_ASFLAGS := $$(ASFLAGS)
    $(tgt)_INCLUDE := $$(addprefix -I,$$(call fn_path_join,$$(mk_dir),$$(INCLUDE)))
    $(tgt)_SOURCES := $$(call fn_relative_path,$$(mk_dir),$$(SOURCES))
    $(tgt)_BASENAME := $$(basename $$($(tgt)_SOURCES))
    $(tgt)_OBJS := $$(addsuffix .o,$$($(tgt)_BASENAME))
    $(tgt)_DEPS := $$(addsuffix .d,$$($(tgt)_BASENAME))
    $(tgt)_FLAGS := $$(addsuffix .*flags,$$($(tgt)_BASENAME))
    $(tgt)_CLEAN := $$(addsuffix .*,$$($(tgt)_BASENAME))
    $(tgt)_CXX_SOURCES := $$(filter $(addprefix %.,$(CXX_EXT)),$$($(tgt)_SOURCES))
    $(tgt)_COMPILER := $$(COMPILER)
    ifeq ($$(COMPILER),gcc)
        $(tgt)_CC := $$(CROSS_COMPILE)gcc
        $(tgt)_CXX := $$(CROSS_COMPILE)g++
        $(tgt)_AS := $$(CROSS_COMPILE)as
    endif
    ifeq ($$(COMPILER),clang)
        $(tgt)_CC := $$(CROSS_COMPILE)clang
        $(tgt)_CXX := $$(CROSS_COMPILE)clang++
        $(tgt)_AS := $$(CROSS_COMPILE)clang
        $(tgt)_ASFLAGS += -c
    endif
    $(tgt)_CPP := $$(CROSS_COMPILE)cpp
    $(tgt)_LD := $$(CROSS_COMPILE)ld
    $(tgt)_AR := $$(CROSS_COMPILE)ar
    $(tgt)_RANLIB := $$(CROSS_COMPILE)ranlib
    $(tgt)_OBJCOPY := $$(CROSS_COMPILE)objcopy
    $(tgt)_OBJDUMP := $$(CROSS_COMPILE)objdump
    $(tgt)_STRIP := $$(CROSS_COMPILE)strip
    $(tgt)_SSTRIP := $$(SSTRIP)

    # remember target for each object
    $$(foreach F,$$(addsuffix _TGT,$$($(tgt)_BASENAME)),$$(eval $$(F) := $(tgt)))

    # kill undefined variable warnings
    $$(foreach X,$$($(tgt)_BASENAME),$$(eval $$(call fn_clear_extra_cflags,$$(X),$$(COMPILER))))

    # include dependencies
    -include $$($(tgt)_DEPS)

    # include built object flags
    -include $$($(tgt)_FLAGS)

    # reset variables
    $$(eval undefine SOURCES)
    $$(eval undefine INCLUDE)
endef

#
# Rule to build an elf file
#
define fn_elf_rule
    # fn_elf_rule

    # avoid warnings on optional variables
    ifeq ($$(origin LIBS),undefined)
        LIBS :=
    endif
    ifeq ($$(origin LDSCRIPT),undefined)
        LDSCRIPT :=
    endif

    ifneq ($$(MAP),)
        LDFLAGS += -Xlinker -M=$(tgt).map
    endif

    # set target variables
    $(tgt)_LDSCRIPT := $$(call fn_path_join,$$(mk_dir),$$(LDSCRIPT))
    $(tgt)_LDFLAGS := $$(LDFLAGS) $$(LDFLAGS_$$(COMPILER))
    $(tgt)_LIBS := $$(patsubst $$(call fn_relative_path,$$(mk_dir),.)/-l%,-l%,$$(call fn_relative_path,$$(mk_dir),$$(LIBS)))

    # reset variables
    $$(eval undefine LDSCRIPT)
    $$(eval undefine LDFLAGS)
    $$(eval undefine LIBS)

    # include built object flags
    -include $(tgt).elfflags

    $$(eval $$(fn_process_sources))

    # switch compilers for C/C++
    ifeq ($$($(tgt)_CXX_SOURCES),)
        $$(eval $$(call fn_flags_rule,$(tgt).elfflags,$$($(tgt)_CFLAGS) $$($(tgt)_LDSCRIPT) $$($(tgt)_OBJS) $$($(tgt)_LIBS) $$($(tgt)_LDFLAGS)))
        $(tgt).elf: $$($(tgt)_LDSCRIPT) $$($(tgt)_OBJS) $$(filter-out -l%,$$($(tgt)_LIBS)) $(tgt).elfflags
		+$$($(tgt)_CC) $$($(tgt)_CFLAGS) -o $$@ $$(addprefix -T,$$($(tgt)_LDSCRIPT)) $$($(tgt)_OBJS) $$($(tgt)_LIBS) $$($(tgt)_LDFLAGS)
    else
        $$(eval $$(call fn_flags_rule,$(tgt).elfflags,$$($(tgt)_CXXFLAGS) $$($(tgt)_LDSCRIPT) $$($(tgt)_OBJS) $$($(tgt)_LIBS) $$($(tgt)_LDFLAGS)))
        $(tgt).elf: $$($(tgt)_LDSCRIPT) $$($(tgt)_OBJS) $$(filter-out -l%,$$($(tgt)_LIBS)) $(tgt).elfflags
		+$$($(tgt)_CXX) $$($(tgt)_CXXFLAGS) -o $$@ $$(addprefix -T,$$($(tgt)_LDSCRIPT)) $$($(tgt)_OBJS) $$($(tgt)_LIBS) $$($(tgt)_LDFLAGS)
    endif
endef

#
# Rule to build an executable
#
define fn_exec_rule
    # fn_exec_rule

    $$(eval $$(fn_elf_rule))
    $(tgt): $(tgt).elf
	cp $$< $$@
    ifeq ($$($(tgt)_SSTRIP),)
	$$($(tgt)_STRIP) -s $$@
    else
	sstrip $$@
    endif
endef

#
# Rule to build a binary image
#
define fn_binary_rule
    # fn_binary_rule

    # clear optional variables
    ifeq ($$(origin BINFLAGS),undefined)
        BINFLAGS :=
    endif

    # set target variables
    $(tgt)_BINFLAGS := $$(BINFLAGS)

    # reset variables
    $$(eval undefine BINFLAGS)

    # include built object flags
    -include $(tgt).binflags

    $$(eval $$(fn_elf_rule))
    $$(eval $$(call fn_flags_rule,$(tgt).binflags,$$($(tgt)_BINFLAGS)))
    $(tgt): $(tgt).elf $(tgt).binflags
	$$($(tgt)_OBJCOPY) $$($(tgt)_BINFLAGS) -O binary $$< $$@
endef

#
# Rule to build a userspace program
#
define fn_prog_rule
    # fn_prog_rule

    CFLAGS := $(CONFIG_USER_CFLAGS) $(CFLAGS)
    CXXFLAGS := $(CONFIG_USER_CFLAGS) $(CXXFLAGS)
    LDFLAGS := $(CONFIG_USER_LDFLAGS) $(LDFLAGS)

    $$(eval $$(fn_exec_rule))
endef

#
# Rule to build a userspace library
#
define fn_lib_rule
    # fn_lib_rule

    CFLAGS := $(CONFIG_USER_CFLAGS) $(CFLAGS)
    CXXFLAGS := $(CONFIG_USER_CFLAGS) $(CXXFLAGS)

    # include built object flags
    -include $(tgt).libflags

    $$(eval $$(fn_process_sources))
    $$(eval $$(call fn_flags_rule,$(tgt).libflags,$$($(tgt)_OBJS)))
    $(tgt): $$($(tgt)_OBJS) $(tgt).libflags
	rm -f $$@
	$$($(tgt)_AR) rc $$@ $$($(tgt)_OBJS)
	$$($(tgt)_RANLIB) $$@
endef

#
# Rule to build a kernel library
#
define fn_klib_rule
    # fn_klib_rule

    # include built object flags
    -include $(tgt).klibflags

    $$(eval $$(fn_process_sources))
    $$(eval $$(call fn_flags_rule,$(tgt).klibflags,$$($(tgt)_OBJS)))
    $(tgt): $$($(tgt)_OBJS) $(tgt).klibflags
	rm -f $$@
	$$($(tgt)_AR) rc $$@ $$($(tgt)_OBJS)
	$$($(tgt)_RANLIB) $$@
endef

#
# Rule to build a custom binary
#
define fn_custom_rule
    # fn_custom_rule
endef

#
# Rule to create a '.a' archive
#
define fn_archive_a_rule
    # fn_archive_a_rule

    # include built object flags
    -include $(tgt).flags

    $(tgt)_AR := $$(CROSS_COMPILE)ar
    $(tgt)_CLEAN :=
    $(tgt)_SOURCES := $$(SOURCES)

    $$(eval $$(call fn_flags_rule,$(tgt).flags,$$($(tgt)_SOURCES)))

    $(tgt): $(tgt).flags $$($(tgt)_SOURCES)
	rm -f $$@
	$$($(tgt)_AR) rcS $$@ $$(wordlist 2,$$(words $$^),$$^)

    $$(eval undefine SOURCES)
endef

#
# Rule to build executable boot image
#
# SOURCES must list the bootloader, kernel and any boot archives in that order.
#
# The resultant boot image is laid out as follows:
# 1. Boot loader
# 2. Zero terminated array of file sizes (32bit big endian)
# 3. Padding up to 8-byte boundary
# 4. Apex kernel
# 5. Boot files
#
define fn_bootimg_rule
    # fn_bootimg_rule

    # include built object flags
    -include $(tgt).flags

    $(tgt)_CLEAN :=
    $(tgt)_BOOTLOADER := $$(firstword $$(SOURCES))
    $(tgt)_FILES := $$(wordlist 2,$$(words $$(SOURCES)),$$(SOURCES))

    $$(eval $$(call fn_flags_rule,$(tgt).flags,$$($(tgt)_BOOTLOADER) $$($(tgt)_FILES)))

    $(tgt): $$($(tgt)_BOOTLOADER) $$($(tgt)_FILES) $(tgt).flags
	cat $$($(tgt)_BOOTLOADER) > $$@
	$$(foreach a,$$($(tgt)_FILES),perl -e "print pack('N', `stat -c %s $$a`)" >> $$@;)
	perl -e "print pack('N', 0)" >> $$@
	truncate -s $$$$((($$$$(stat -c %s $$@) + 7) & -8)) $$@
	cat $$($(tgt)_FILES) >> $$@

    $$(eval undefine SOURCES)
endef

#
# Rule to clean
#
define fn_clean_rule
    # fn_clean_rule

    clean: $(tgt)_clean
    .PHONY: $(tgt)_clean
    $(tgt)_clean:
	rm -f $(tgt) $(tgt).* $$($(tgt)_CLEAN)
endef

#
# Set extra cflags empty if not defined to kill undefined variable warnings
#
define fn_clear_extra_cflags
    # fn_clear_extra_cflags

    ifndef $(1)_EXTRA_CFLAGS
        $(1)_EXTRA_CFLAGS :=
    endif

    ifndef $(1)_EXTRA_CFLAGS_$(2)
        $(1)_EXTRA_CFLAGS_$(2) :=
    endif
endef

#
# Rules to build objects
#
$(eval $(call fn_flags_rule,%.cflags,$$($$($$*_TGT)_CFLAGS) $$($$*_EXTRA_CFLAGS) $$($$*_EXTRA_CFLAGS_$$($$($$*_TGT)_COMPILER)) $$($$($$*_TGT)_INCLUDE)))
$(eval $(call fn_flags_rule,%.cxxflags,$$($$($$*_TGT)_CXXFLAGS) $$($$*_EXTRA_CFLAGS) $$($$*_EXTRA_CFLAGS_$$($$($$*_TGT)_COMPILER)) $$($$($$*_TGT)_INCLUDE)))
$(eval $(call fn_flags_rule,%.asflags,$$($$($$*_TGT)_ASFLAGS)))

define cpp_rule
%.o : %.$1 %.cxxflags
	+$$($$($$*_TGT)_CXX) -c -MD -MP $$($$($$*_TGT)_CXXFLAGS) $$($$*_EXTRA_CFLAGS) $$($$*_EXTRA_CFLAGS_$$($$($$*_TGT)_COMPILER)) $$($$($$*_TGT)_INCLUDE) -o $$@ $$<
endef
$(foreach ext,$(CXX_EXT),$(eval $(call cpp_rule,$(ext))))

%.o: %.c %.cflags
	+$($($*_TGT)_CC) -c -MD -MP $($($*_TGT)_CFLAGS) $($*_EXTRA_CFLAGS) $($*_EXTRA_CFLAGS_$($($*_TGT)_COMPILER)) $($($*_TGT)_INCLUDE) -o $@ $<

%.s: %.S %.cflags
	$($($*_TGT)_CPP) -MD -MP -MT $*.s $($($*_TGT)_CFLAGS) $($*_EXTRA_CFLAGS) $($*_EXTRA_CFLAGS_$($($*_TGT)_COMPILER)) $($($*_TGT)_INCLUDE) -D__ASSEMBLY__ $< -o $@

%.o: %.s %.asflags
	$($($*_TGT)_AS) $($($*_TGT)_ASFLAGS) $($($*_TGT)_INCLUDE) -o $@ $<

#
# Process a '.mk' file.
#
# Synopsis: $(eval $(call fn_process_mkfile,<path_to_mkfile>))
#
define fn_process_mkfile
    # fn_process_mkfile

    # empty base variables which can be defined by a mk file
    DEFAULT :=
    TYPE := TYPE_is_not_set
    TARGET :=
    MK :=

    # set all variables which can have configured defaults
    DEFS := $(CONFIG_DEFS)
    DEFS_$(CONFIG_COMPILER) := $(CONFIG_DEFS_$(CONFIG_COMPILER))
    CFLAGS := $(CONFIG_MCPU) $(CONFIG_CFLAGS)
    CFLAGS_$(CONFIG_COMPILER) := $(CONFIG_CFLAGS_$(CONFIG_COMPILER))
    CXXFLAGS := $(CONFIG_MCPU) $(CONFIG_CXXFLAGS)
    CXXFLAGS_$(CONFIG_COMPILER) := $(CONFIG_CXXFLAGS_$(CONFIG_COMPILER))
    ASFLAGS := $(CONFIG_MCPU) $(CONFIG_ASFLAGS)
    LDFLAGS := $(CONFIG_LDFLAGS)
    LDFLAGS_$(CONFIG_COMPILER) := $(CONFIG_LDFLAGS_$(CONFIG_COMPILER))
    MAP := $(CONFIG_MAP)
    CROSS_COMPILE := $(CONFIG_CROSS_COMPILE)
    COMPILER := $(CONFIG_COMPILER)
    SSTRIP := $(CONFIG_SSTRIP)

    # remember MAKEFILE_LIST before including mk file
    prev := $$(abspath $$(MAKEFILE_LIST))

    # include mk file
    include $(1)

    # add definitions to CFLAGS and CXXFLAGS
    CFLAGS += $$(DEFS) $$(DEFS_$$(COMPILER))
    CXXFLAGS += $$(DEFS) $$(DEFS_$$(COMPILER))

    # directory containing mk file we just processed
    mk_dir := $$(dir $$(firstword $$(filter-out $$(prev),$$(abspath $$(MAKEFILE_LIST)))))

    # mk_dir is empty if a makefile is included twice
    ifneq ($$(mk_dir),)
        ifneq ($$(TARGET),)
            # relative path to target
            tgt := $$(patsubst $(CONFIG_BUILDDIR)/%,%,$$(call fn_relative_path,$$(mk_dir),$$(TARGET)))

            # force dependency on default targets
            ifneq ($$(DEFAULT),)
                default_tgts += $$(tgt)
            endif

            # define rules based on target type
            ifeq ($$(origin fn_$$(TYPE)_rule),undefined)
                $$(error no rule for TYPE=$$(TYPE))
            endif
            $$(eval $$(fn_$$(TYPE)_rule))

            # clean
            $$(eval $$(fn_clean_rule))

            $$(tgt)_rule_exists := y
        endif

        # include any MK files
        ifneq ($$(MK),)
            mk_dir_stack := $$(mk_dir) $$(mk_dir_stack)
            $$(foreach f,$$(call fn_relative_path,$$(firstword $$(mk_dir_stack)),$$(MK)),$$(eval $$(call fn_process_mkfile,$$(f))))
            mk_dir_stack := $$(wordlist 2,$$(words $$(mk_dir_stack)),$$(mk_dir_stack))
        endif
    endif

    # reset variables
    $$(eval undefine ASFLAGS)
    $$(eval undefine CFLAGS)
    $$(eval undefine CFLAGS_$(CONFIG_COMPILER))
    $$(eval undefine COMPILER)
    $$(eval undefine CROSS_COMPILE)
    $$(eval undefine CXXFLAGS)
    $$(eval undefine CXXFLAGS_$(CONFIG_COMPILER))
    $$(eval undefine DEFAULT)
    $$(eval undefine DEFS)
    $$(eval undefine DEFS_$(CONFIG_COMPILER))
    $$(eval undefine LDFLAGS)
    $$(eval undefine LDFLAGS_$(CONFIG_COMPILER))
    $$(eval undefine MAP)
    $$(eval undefine MK)
    $$(eval undefine SSTRIP)
    $$(eval undefine TARGET)
    $$(eval undefine TYPE)
endef

# Check if this version of 'make' supports $(eval)
eval_available :=
$(eval eval_available := y)
ifneq ($(eval_available),y)
    $(error 'make' does not support $$(eval))
endif

# Make sure we are running in a configured Apex environment
ifeq ($(CONFIG_SRCDIR),)
    $(error Run 'configure' before 'make')
endif

# Globals used by fn_process_mkfile
mk_dir_stack :=
default_tgts :=

# Trick to silence naughty 3rd party makefiles that ignore -s
SHUTUP_IF_SILENT := $(if $(findstring -s,$(_MFLAGS)),> /dev/null,)

# Default target
.PHONY: all
all:

# Include any extra rules
ifneq ($(origin CONFIG_CUSTOM_MK),undefined)
    include $(CONFIG_CUSTOM_MK)
endif

# Build any configured default targets
ifneq ($(origin CONFIG_DEFAULT_TGTS),undefined)
    default_tgts += $(CONFIG_DEFAULT_TGTS)
endif

# Include main mk files
$(foreach f,$(MK),$(eval $(call fn_process_mkfile,$(f))))

# Force dependency on default targets
all: $(default_tgts)

# Generic clean rule
.PHONY: clean
clean:

# Convenience to run under qemu
ifneq ($(origin CONFIG_QEMU_CMD),undefined)
.PHONY: run
run: $(CONFIG_QEMU_IMG)
	$(CONFIG_QEMU_CMD) $<
endif
