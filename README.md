Apex
====

Apex is a real time operating system designed for use in small to medium scale
embedded systems. It implements a subset of the Linux syscall interface and
uses the musl C library.

This project initially borrowed heavily from the
[Prex](http://prex.sourceforge.net) project, but ended up largely rewritten.

Apex aims to be small, efficient and easy to understand. The main architectural
difference from Prex is that Apex is a monolithic rather than a microkernel
system.

Apex is under heavy development and should not be considered stable at this
time. Contributions, suggestions, comments and general feedback are welcome.


Motivation
==========

There are many RTOSes targeted at small microcontrollers with limited
resources. These generally require a programmer to gain detailed knowledge of
their libraries and interfaces in order to target them effectively. Often the
resultant project is heavily dependent on the chosen RTOS. This can make it
difficult to migrate an application to a new environment.

For large embedded systems there is Linux.

Apex tries to fit in between the two extremes, targeting systems where Linux is
too large and excluding very small microcontrollers.

Generally, systems with 1MiB of ROM & RAM are enough for the Apex kernel and a
rich set of userspace tools. Smaller systems can be targeted but will require
more careful optimisation.

The project has the following goals:
* Open source & royalty free.
* Secure & reliable.
* Small & efficient.
* Easy to understand.
* Binary compatibility with Linux (using musl libc).

Providing a Linux compatible environment has a number of benefits:
* Development effort can be focused entirely on the kernel as userspace tools &
  libraries are provided by external projects.
* The userspace environment is familiar to Linux developers.
* Toolchains for Linux development are readily available & simple to obtain.
* Applications can be developed & tested under Linux using advanced debugging
  tools (such as valgrind).

A (very) rough comparison of scale for a complete system:
Property			| Traditional RTOS  | Apex	    | Linux
--------			| ----------------  | ----	    | -----
Code size (kernel & userspace)  | < 0.5MiB	    | >= 0.5MiB	    | >= 4MiB
Memory size			| < 0.5MiB	    | >= 0.5MiB	    | >= 4MiB
Architecture support		| >= 8-bit	    | 32-bit(*)	    | runs on anything
(*) 64-bit will be supported in the future.


Architecture Support
====================

Architecture		| Status
------------		| ------
aarch64	(ARMv8-A)	| Future
arm	(ARMv7-A)	| Future
arm	(ARMv7-R)	| Future
arm	(ARMv7-M)	| Complete
arm	(ARMv8-A)	| Future
arm	(ARMv8-R)	| Future
arm	(ARMv8-M)	| Future
i386			| Future
m68k			| Future
microblaze		| Future
mips			| Future
mips64			| Future
mipsn32			| Future
or1k			| Future
powerpc	(Classic)	| Future
powerpc	(Book E)	| Planned
powerpc64		| Future
s390x			| Future
sh			| Future
x32			| Future
x86_64			| Future


Board Support
=============

Physical
--------

Vendor	| Board		    | CPU (Architecture)    | Status
------	| -----		    | ------------------    | ------
nxp	| imxrt1050-evkb    | Cortex-M7 (ARMv7-M)   | Complete
nxp	| mimxrt1060-evk    | Cortex-M7 (ARMv7-M)   | Complete

Virtual (QEMU)
--------------

Architecture	| Board		| CPU (Architecture)	| Status
------------	| -----		| ------------------	| ------
arm		| mps2-an385	| Cortex-M3 (ARMv7-M)	| Complete
arm		| virt		| Cortex-A15 (ARMv7-A)	| Future
aarch64		| virt		| Cortex-A53 (ARMv8-A)	| Future


Toolchain
=========

Apex requires the excellent musl libc to build. By far the easiest way to get a
musl libc toolchain is by using
[musl-cross-make](https://github.com/richfelker/musl-cross-make).

An example config.mak for armv7-m:

~~~~
TARGET = armv7m-linux-musleabi
GCC_CONFIG += --with-arch=armv7-m
GCC_CONFIG += --enable-languages=c,c++
GCC_CONFIG += --disable-libquadmath --disable-decimal-float
GCC_CONFIG += --enable-default-pie
GCC_CONFIG += --enable-cxx-flags="-ffunction-sections"
MUSL_CONFIG += --enable-debug
COMMON_CONFIG += CFLAGS="-g0 -Os" CXXFLAGS="-g0 -Os" LDFLAGS="-s"
COMMON_CONFIG += --disable-nls
COMMON_CONFIG += --with-debug-prefix-map=\$(CURDIR)=
~~~~


Quick Start Guide
=================

Assuming you have a toolchain (and qemu-system-arm) in your path the following
sequence of commands should build a working system which boots to a shell:

~~~~
$ git clone https://github.com/apexrtos/apex-examples.git
$ git -C apex-examples submodule update --init
$ mkdir build && cd build
$ ../apex-examples/configure --project=shell/config --machine=qemu/arm/mps2-an385
$ make run
~~~~

Note that qemu-system-arm >= v3.0.0 is recommended as it contains an important
[bug fix](https://lists.gnu.org/archive/html/qemu-devel/2018-04/msg03184.html)
for the emulated UART on the MPS2-AN385 board used in this example.


Structure
=========

Apex is split into multiple repositories:

~~~~
https://github.com/apexrtos
├── apex                        operating system kernel & boot loader
├── apex-examples               example projects
└── apex-tests                  test harnesses & projects
~~~~

The top level of the Apex sources can be summarised as:

~~~~
apex
├── boot                        Apex boot loader
├── cpu                         CPU specific configuration & sources
├── libc                        C libary (for bootloader & kernel)
├── libc++                      C++ library (for bootloader & kernel)
├── machine                     machine/board support files
├── mk                          build system
└── sys                         Apex kernel
    ├── arch                    architecture support
    ├── dev                     device drivers
    ├── fs                      file systems
    ├── kern                    kernel core
    ├── mem                     memory management
    └── sync                    synchronisation primitives
~~~~


Configuration & Build System
============================

The Apex build is controlled by a set of configuration files which describe the
project, machine and CPU to build for. These files can be located in the Apex
repository or in a project specific repository and can be freely mixed &
matched.

In Apex we use the following terms:
Term    | Meaning
----	| -------
Project	| Userspace applications, init scripts, boot configuration, custom drivers, optional machine and/or CPU configuration.
Machine | Board details, memory layout, minimal set of drivers, optional CPU configuration.
CPU	| CPU architecture & features.

Based on these configuration files the configure script generates a set of
files (under conf/) in the build directory which describe the kernel and
userspace configuration, and the files to build.
