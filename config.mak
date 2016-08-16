# Disable verbose output and implicit rules (harder to debug)
.SUFFIXES:
MAKEFLAGS += --no-print-directory

MUSLKLSGX_STD_RUN_OPTS ?= MUSL_ETHREADS=4 MUSL_STHREADS=4

#TODO: use autoconf or auto detect
LINUX_HEADERS_INC ?= /usr/include

FORCE_SUBMODULES_VERSION ?= false

ROOT_DIR ?= $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(ROOT_DIR)/build
TESTS ?= $(ROOT_DIR)/tests
TESTS_BUILD ?= $(BUILD_DIR)/tests
LIBEVENT ?= ${TESTS}/80-memcached/libevent
LIBEVENT_BUILD ?= ${TESTS_BUILD}/80-memcached/libevent
MEMCACHED ?= ${TESTS}/80-memcached/memcached
MEMCACHED_BUILD ?= ${TESTS_BUILD}/80-memcached/memcached
NGINX ?= ${TESTS}/70-nginx
NGINX_BUILD ?= ${TESTS_BUILD}/70-nginx
TOOLS ?= ${ROOT_DIR}/tools
TOOLS_BUILD ?= $(BUILD_DIR)/tools
LKL ?= $(ROOT_DIR)/lkl
LKL_BUILD ?= ${BUILD_DIR}/lkl
LKL_LIB ?= ${LKL_BUILD}/lib/liblkl.a
HOST_MUSL ?= $(ROOT_DIR)/host-musl
HOST_MUSL_BUILD ?= $(BUILD_DIR)/host-musl
HOST_MUSL_CC ?= ${HOST_MUSL_BUILD}/bin/musl-gcc
SGX_MUSL ?= $(ROOT_DIR)/sgx-musl
SGX_MUSL_BUILD ?= ${BUILD_DIR}/sgx-musl
SGX_MUSL_CC ?= ${SGX_MUSL_BUILD}/bin/sgxmusl-gcc

CFLAGS ?= -std=c11 -Wall -Wextra -Werror -rdynamic -isystem ${SGX_MUSL}/src/internal/
LDFLAGS ?=

DEBUG ?= false

MUSL_CONFIGURE_OPTS ?=
MUSL_CFLAGS ?= -fPIC
NGINX_CONFIGURE_OPTS ?=

ifeq ($(DEBUG),true)
	MUSL_CONFIGURE_OPTS += --disable-optimize --enable-debug
	MUSL_CFLAGS += -g -O0 -DDEBUG
	NGINX_CONFIGURE_OPTS += --with-debug
	CFLAGS += -ggdb3 -O0
else
	CFLAGS += -O3
endif
