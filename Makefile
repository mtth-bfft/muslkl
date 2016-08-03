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

TESTS_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -ggdb3 -O0 -rdynamic -I${LKL_BUILD}/include/
TESTS_LDFLAGS ?=

#
# Actual targets
#

all: sgx-musl tests

${BUILD_DIR} ${TESTS_BUILD} ${LKL_BUILD} ${HOST_MUSL_BUILD} ${SGX_MUSL_BUILD}:
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

sgx-musl: submodules lkl ${SGX_MUSL_BUILD}
	cd ${SGX_MUSL}; [ -f config.mak ] || ./configure \
		--prefix=${SGX_MUSL_BUILD} \
		--lklheaderdir=${LKL_BUILD}/include/ \
		--lkllib=${LKL_LIB} \
		--enable-debug \
		--disable-optimize \
		--disable-shared
	+${MAKE} -C ${SGX_MUSL} install

lkl: submodules host-musl ${LKL_BUILD}
	+DESTDIR=${LKL_BUILD} ${MAKE} -C ${LKL}/tools/lkl CC="${HOST_MUSL_CC}" PREFIX="" \
		ALL_LIBRARIES=liblkl.a libraries_install
	+DESTDIR=${LKL_BUILD} ${MAKE} -C ${LKL}/tools/lkl CC="${HOST_MUSL_CC}" PREFIX="" \
		headers_install
	# Bugfix, prefix symbol that collides with musl's one
	find ${LKL_BUILD}/include/ -type f -exec sed -i 's/struct ipc_perm/struct lkl_ipc_perm/' {} \;

${TESTS_BUILD}/%: ${TESTS}/%.c sgx-musl ${TESTS_BUILD}
	${SGX_MUSL_CC} ${TESTS_CFLAGS} -o $@ $< ${TESTS_LDFLAGS}

.PHONY: submodules host-musl sgx-musl lkl tests testrun clean

submodules:
	git submodule init
	git submodule update

tests: $(TESTS_OBJ)
	${MAKE} -j1 testrun

testrun:
	@rm -f /tmp/encl-lib*
	@printf "    [*] 01-compiler: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/01-compiler; \
		[ $$? -eq 42 ] && echo "OK"
	@printf "    [*] 02-pthreads: "
	@MUSL_NOLKL=1 MUSL_ETHREADS=2 MUSL_STHREADS=2 ${TESTS_BUILD}/02-pthreads >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 03-pthreads-sleep: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/03-pthreads-sleep >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
#	@printf "    [*] 04-lthreads: "
#	@MUSL_NOLKL=1 ${TESTS_BUILD}/04-lthreads; \
#		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 05-lkl-host-print: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/05-lkl-host-print 2>&1 | grep -qi ok; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 06-lkl-host-panic: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/06-lkl-host-panic >/dev/null 2>&1; \
		[ $$? -ne 0 ] && echo "OK"
	@printf "    [*] 07-lkl-host-mem: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/07-lkl-host-mem >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 08-lkl-host-thread: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/08-lkl-host-thread >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 09-lkl-host-tls: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/09-lkl-host-tls >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 10-lkl-host-mutex: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/10-lkl-host-mutex >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 11-lkl-host-semaphore: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/11-lkl-host-semaphore >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 12-lkl-host-time: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/12-lkl-host-time >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 13-lkl-host-timer: "
	@MUSL_NOLKL=1 MUSL_ETHREADS=2 MUSL_STHREADS=2 ${TESTS_BUILD}/13-lkl-host-timer >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@rm -f /tmp/encl-lib*

clean:
	rm -rf ${BUILD_DIR}
	+${MAKE} -C ${HOST_MUSL} distclean
	+${MAKE} -C ${SGX_MUSL} distclean
	+${MAKE} -C ${LKL} clean
	+${MAKE} -C ${LKL}/tools/lkl clean

