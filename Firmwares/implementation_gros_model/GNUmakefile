# Persistent make overlay for STM32CubeMX-generated projects.
# CubeMX rewrites Makefile, but GNUmakefile is preferred by GNU make
# and is typically left untouched.

include Makefile

# Keep required retarget/system stubs even if CubeMX removes them.
USER_REQUIRED_C_SOURCES := Core/Src/syscalls.c Core/Src/sysmem.c
C_SOURCES_BASE := $(C_SOURCES)
C_SOURCES := $(C_SOURCES_BASE) $(filter-out $(C_SOURCES_BASE),$(USER_REQUIRED_C_SOURCES))
