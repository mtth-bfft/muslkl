# Disable verbose output and implicit rules (harder to debug)
#.SUFFIXES:
#MAKEFLAGS += --no-print-directory

MUSLKLSGX_STD_RUN_OPTS ?= MUSL_ETHREADS=4 MUSL_STHREADS=4

#TODO: use autoconf or auto detect
LINUX_HEADERS_INC ?= /usr/include

FORCE_SUBMODULES_VERSION ?= false

ROOT_DIR ?= $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(ROOT_DIR)/build
TESTS ?= $(ROOT_DIR)/tests
TESTS_BUILD ?= $(BUILD_DIR)/tests
TESTS_SRC ?= $(sort $(wildcard $(TESTS)/*.c))
TESTS_OBJ ?= $(addprefix $(TESTS_BUILD)/, $(notdir $(TESTS_SRC:.c=)))
LIBEVENT ?= ${TESTS}/80-memcached/libevent
LIBEVENT_BUILD ?= ${TESTS_BUILD}/80-memcached/libevent
MEMCACHED ?= ${TESTS}/80-memcached/memcached
MEMCACHED_BUILD ?= ${TESTS_BUILD}/80-memcached/memcached
NGINX ?= ${TESTS}/70-nginx
NGINX_BUILD ?= ${TESTS_BUILD}/70-nginx
TOOLS ?= ${ROOT_DIR}/tools
TOOLS_BUILD ?= $(BUILD_DIR)/tools
TOOLS_SRC ?= $(wildcard $(TOOLS)/*.c)
TOOLS_OBJ ?= $(addprefix $(TOOLS_BUILD)/, $(notdir $(TOOLS_SRC:.c=)))
OPENSSL ?= ${ROOT_DIR}/openssl
OPENSSL_BUILD ?= ${BUILD_DIR}/openssl
LIBCRYPTO ?= ${OPENSSL_BUILD}/lib/libcrypto.a
LKL ?= $(ROOT_DIR)/lkl
LKL_BUILD ?= ${BUILD_DIR}/lkl
LIBLKL ?= ${LKL_BUILD}/lib/liblkl.a
HOST_MUSL ?= $(ROOT_DIR)/host-musl
HOST_MUSL_BUILD ?= $(BUILD_DIR)/host-musl
HOST_MUSL_CC ?= ${HOST_MUSL_BUILD}/bin/musl-gcc
SGX_MUSL ?= $(ROOT_DIR)/sgx-musl
SGX_MUSL_BUILD ?= ${BUILD_DIR}/sgx-musl
SGX_MUSL_CC ?= ${SGX_MUSL_BUILD}/bin/sgxmusl-gcc
# Headers not exported by LKL and built by a custom tool's output cat to the file instead
LKL_SGXMUSL_HEADERS ?= ${LKL_BUILD}/include/lkl/bits.h ${LKL_BUILD}/include/lkl/syscalls.h

# sgxmusl-gcc options, just for our test suite and tools
MY_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -isystem ${SGX_MUSL}/src/internal/ -I${OPENSSL_BUILD}/include/
MY_LDFLAGS ?= -L${OPENSSL_BUILD}/lib -lcrypto

DEBUG ?= false

MUSL_CONFIGURE_OPTS ?=
MUSL_CFLAGS ?= -fPIC -D__USE_GNU
NGINX_CONFIGURE_OPTS ?=

ifeq ($(DEBUG),true)
	MUSL_CONFIGURE_OPTS += --disable-optimize --enable-debug
	MUSL_CFLAGS += -g -O0 -DDEBUG
	NGINX_CONFIGURE_OPTS += --with-debug
	MY_CFLAGS += -ggdb3 -O0
else
	MY_CFLAGS += -O3
endif
