# Disable verbose output and implicit rules (harder to debug)
.SUFFIXES:
MAKEFLAGS += --no-print-directory

#
# Directory locations
#

#TODO: use autoconf or auto detect
LINUX_HEADERS_INC ?= /usr/include

ROOT_DIR ?= $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(ROOT_DIR)/build
TESTS ?= $(ROOT_DIR)/tests
TESTS_BUILD ?= $(BUILD_DIR)/tests
TESTS_SRC ?= $(sort $(wildcard $(TESTS)/*.c))
TESTS_OBJ ?= $(addprefix $(TESTS_BUILD)/, $(notdir $(TESTS_SRC:.c=)))
LKL ?= $(ROOT_DIR)/lkl
LKL_BUILD ?= ${BUILD_DIR}/lkl
HOST_MUSL ?= $(ROOT_DIR)/host-musl
HOST_MUSL_BUILD ?= $(BUILD_DIR)/host-musl
HOST_MUSL_CC ?= ${HOST_MUSL_BUILD}/bin/musl-gcc
SGX_MUSL ?= $(ROOT_DIR)/sgx-musl
SGX_MUSL_BUILD ?= ${BUILD_DIR}/sgx-musl
SGX_MUSL_CC ?= ${SGX_MUSL_BUILD}/bin/sgxmusl-gcc

#
# Configuration flags
#

#DEBUG ?= true
#CFLAGS_ALL ?= -std=c11 -Wall -Wextra -Werror
#ifeq ($(DEBUG),true)
#	CFLAGS_ALL += -ggdb3 -O0
#else
#	CFLAGS_ALL += -O2
#endif

#
# Actual targets
#

all: ${HOST_MUSL_CC} lkl

${BUILD_DIR} ${TESTS_BUILD} ${LKL_BUILD} ${HOST_MUSL_BUILD} ${SGX_MUSL_BUILD}:
	@mkdir -p $@


${HOST_MUSL_CC}: ${HOST_MUSL_BUILD}
	cd ${HOST_MUSL}; [ -f config.mak ] || ./configure \
		--prefix=${HOST_MUSL_BUILD} \
		--enable-debug \
		--disable-optimize \
		--disable-shared
	+${MAKE} -C ${HOST_MUSL} install

${SGX_MUSL_CC}: ${SGX_MUSL_BUILD}
	cd ${SGX_MUSL}; [ -f config.mak ] || ./configure \
		--prefix=${SGX_MUSL_BUILD} \
		--enable-debug \
		--disable-optimize \
		--disable-shared
	+${MAKE} -C ${SGX_MUSL} install
	ln -fs ${LINUX_HEADERS_INC}/linux/ ${SGX_MUSL_BUILD}/include/
	ln -fs ${LINUX_HEADERS_INC}/asm/ ${SGX_MUSL_BUILD}/include/
	ln -fs ${LINUX_HEADERS_INC}/asm-generic/ ${SGX_MUSL_BUILD}/include/

${TESTS_BUILD}/%: ${TESTS}/%.c ${TESTS_BUILD} ${SGX_MUSL_CC}
	${SGX_MUSL_CC} ${CFLAGS} -o $@ $< ${LDFLAGS}

.PHONY: host-musl sgx-musl lkl tests clean

host-musl: ${HOST_MUSL_CC}
sgx-musl: ${SGX_MUSL_CC}
lkl: ${SGX_MUSL_CC} ${LKL_BUILD}
	+${MAKE} V=1 CC="${SGX_MUSL_CC}" -C ${LKL}/tools/lkl liblkl.a
	+${MAKE} V=1 CC="${SGX_MUSL_CC}" -C ${LKL}/tools/lkl DESTDIR=${LKL_BUILD} PREFIX="" headers_install
	+${MAKE} V=1 CC="${SGX_MUSL_CC}" -C ${LKL}/tools/lkl DESTDIR=${LKL_BUILD} PREFIX="" libraries_install

tests: $(TESTS_OBJ)
	@for test in $(TESTS_OBJ); do \
		echo " [*] $$test :"; \
		($$test && echo "     OK") || echo "     FAILED"; \
	done

clean:
	rm -rf ${BUILD_DIR}
	+${MAKE} -C ${HOST_MUSL} distclean
	+${MAKE} -C ${SGX_MUSL} distclean
	+${MAKE} -C ${LKL} clean
	+${MAKE} -C ${LKL}/tools/lkl clean

