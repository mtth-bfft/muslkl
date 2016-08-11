# Disable verbose output and implicit rules (harder to debug)
.SUFFIXES:
MAKEFLAGS += --no-print-directory

#
# Directory locations
#

#TODO: use autoconf or auto detect
LINUX_HEADERS_INC ?= /usr/include

FORCE_SUBMODULES_VERSION ?= false

ROOT_DIR ?= $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(ROOT_DIR)/build
TESTS ?= $(ROOT_DIR)/tests
TESTS_BUILD ?= $(BUILD_DIR)/tests
TESTS_SRC ?= $(sort $(wildcard $(TESTS)/*.c))
TESTS_OBJ ?= $(addprefix $(TESTS_BUILD)/, $(notdir $(TESTS_SRC:.c=)))
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

#
# Configuration flags
#

TESTS_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -ggdb3 -O0 -rdynamic -I${LKL_BUILD}/include/ -isystem ${SGX_MUSL}/src/internal/
TESTS_LDFLAGS ?=

#
# Actual targets
#

all: sgx-musl tests

${BUILD_DIR} ${TESTS_BUILD} ${TOOLS_BUILD} ${LKL_BUILD} ${HOST_MUSL_BUILD} ${SGX_MUSL_BUILD}:
	@mkdir -p $@

#FIXME: add CFLAGS="-fPIC" to host_musl too?
host-musl: submodules ${HOST_MUSL_BUILD}
	cd ${HOST_MUSL}; [ -f config.mak ] || ./configure \
		--prefix=${HOST_MUSL_BUILD} \
		--enable-debug \
		--disable-optimize \
		--disable-shared
	+${MAKE} -C ${HOST_MUSL} install
	ln -fs ${LINUX_HEADERS_INC}/linux/ ${HOST_MUSL_BUILD}/include/
	ln -fs ${LINUX_HEADERS_INC}/asm/ ${HOST_MUSL_BUILD}/include/
	ln -fs ${LINUX_HEADERS_INC}/asm-generic/ ${HOST_MUSL_BUILD}/include/

lkl: submodules host-musl ${LKL_BUILD} ${TOOLS_BUILD}
	+DESTDIR=${LKL_BUILD} ${MAKE} -C ${LKL}/tools/lkl CC="${HOST_MUSL_CC}" PREFIX="" \
		ALL_LIBRARIES=liblkl.a libraries_install
	+DESTDIR=${LKL_BUILD} ${MAKE} -C ${LKL}/tools/lkl CC="${HOST_MUSL_CC}" PREFIX="" \
		headers_install
	#TODO: apply before makes, and to the entire ${LKL} folder?
	# Bugfix, prefix symbol that collides with musl's one
	find ${LKL_BUILD}/include/ -type f -exec sed -i 's/struct ipc_perm/struct lkl_ipc_perm/' {} \;
	${HOST_MUSL_CC} ${TESTS_CFLAGS} -o ${TOOLS_BUILD}/lkl_syscalls ${TOOLS}/lkl_syscalls.c ${TESTS_LDFLAGS}
	${TOOLS_BUILD}/lkl_syscalls > ${LKL_BUILD}/include/lkl/syscall.h

sgx-musl: submodules lkl ${SGX_MUSL_BUILD}
	cd ${SGX_MUSL}; [ -f config.mak ] || ./configure \
		--prefix=${SGX_MUSL_BUILD} \
		--lklheaderdir=${LKL_BUILD}/include/ \
		--lkllib=${LKL_LIB} \
		--enable-debug \
		--disable-optimize \
		--disable-shared
	+${MAKE} -C ${SGX_MUSL} install

${TESTS_BUILD}/%: ${TESTS}/%.c sgx-musl ${TESTS_BUILD}
	${SGX_MUSL_CC} ${TESTS_CFLAGS} -o $@ $< ${TESTS_LDFLAGS}

.PHONY: submodules host-musl sgx-musl lkl tests testrun clean

submodules:
	[ "$(FORCE_SUBMODULES_VERSION)" = "true" ] || git submodule init
	[ "$(FORCE_SUBMODULES_VERSION)" = "true" ] || git submodule update

tests: $(TESTS_OBJ)
	${MAKE} -j1 runtests

runtests:
	@printf "    [*] 01-compiler: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/01-compiler >/dev/null; \
		[ $$? -eq 42 ] && echo "OK"
	@printf "    [*] 02-pthreads: "
	@MUSL_NOLKL=1 MUSL_ETHREADS=2 MUSL_STHREADS=2 ${TESTS_BUILD}/02-pthreads >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 03-pthreads-sleep: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/03-pthreads-sleep >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
#	@printf "    [*] 04-lthreads-sleep: "
#	@MUSL_NOLKL=1 MUSL_ETHREADS=6 MUSL_STHREADS=6 ${TESTS_BUILD}/04-lthreads-sleep; \
#		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 10-lkl-host-print: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/10-lkl-host-print 2>&1 | grep -qi ok; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 11-lkl-host-panic: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/11-lkl-host-panic >/dev/null 2>&1; \
		[ $$? -ne 0 ] && echo "OK"
	@rm -f /tmp/encl-lb-*
	@printf "    [*] 12-lkl-host-mem: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/12-lkl-host-mem >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 13-lkl-host-thread: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/13-lkl-host-thread >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 14-lkl-host-tls: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/14-lkl-host-tls >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 15-lkl-host-mutex: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/15-lkl-host-mutex >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 16-lkl-host-semaphore: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/16-lkl-host-semaphore >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 17-lkl-host-time: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/17-lkl-host-time >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 18-lkl-host-timer: "
	@MUSL_NOLKL=1 MUSL_ETHREADS=4 MUSL_STHREADS=4 ${TESTS_BUILD}/18-lkl-host-timer >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 19-lkl-boot: "
	@MUSL_NOLKL=0 MUSL_ETHREADS=2 MUSL_STHREADS=2 MUSL_TAP="" MUSL_HD="" ${TESTS_BUILD}/19-lkl-boot >/dev/null; \
		[ $$? -eq 42 ] && echo "OK"
	@printf "    [*] 20-lkl-disk: "
	@MUSL_NOLKL=0 MUSL_ETHREADS=2 MUSL_STHREADS=2 MUSL_TAP="" MUSL_HD=${TESTS}/20-lkl-disk.img:ro ${TESTS_BUILD}/20-lkl-disk >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 21-lkl-net: "
	@(MUSL_NOLKL=0 MUSL_ETHREADS=4 MUSL_STHREADS=4 MUSL_HD="" MUSL_TAP=tap0 MUSL_IP4=10.0.10.10 MUSL_GW4=10.0.10.10 ${TESTS_BUILD}/21-lkl-net >/dev/null &) && (ping -W1 -c15 -i0.2 -D -O -q -n 10.0.10.10 >/dev/null);  \
		[ $$? -eq 0 ] && echo "OK"
	@rm /tmp/encl-lib-*

clean:
	rm -rf ${BUILD_DIR}
	+${MAKE} -C ${HOST_MUSL} distclean
	+${MAKE} -C ${SGX_MUSL} distclean
	+${MAKE} -C ${LKL} clean
	+${MAKE} -C ${LKL}/tools/lkl clean

