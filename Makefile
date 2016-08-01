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

#DEBUG ?= true
#CFLAGS_ALL ?= -std=c11 -Wall -Wextra -Werror
#ifeq ($(DEBUG),true)
#	CFLAGS_ALL += -ggdb3 -O0 -rdynamic
#else
#	CFLAGS_ALL += -O2
#endif

TESTS_CFLAGS ?= -std=c99 -Wall -Wextra -Werror -ggdb3 -O0 -rdynamic -I${LKL_BUILD}/include/
TESTS_LDFLAGS ?=

#
# Actual targets
#

all: ${SGX_MUSL_CC}

${BUILD_DIR} ${TESTS_BUILD} ${LKL_BUILD} ${HOST_MUSL_BUILD} ${SGX_MUSL_BUILD}:
	@mkdir -p $@

#FIXME: add CFLAGS="-fPIC" to host_musl too?
${HOST_MUSL_CC}: ${HOST_MUSL_BUILD}
	cd ${HOST_MUSL}; [ -f config.mak ] || ./configure \
		--prefix=${HOST_MUSL_BUILD} \
		--enable-debug \
		--disable-optimize \
		--disable-shared
	+${MAKE} -C ${HOST_MUSL} install
	ln -fs ${LINUX_HEADERS_INC}/linux/ ${HOST_MUSL_BUILD}/include/
	ln -fs ${LINUX_HEADERS_INC}/asm/ ${HOST_MUSL_BUILD}/include/
	ln -fs ${LINUX_HEADERS_INC}/asm-generic/ ${HOST_MUSL_BUILD}/include/

${SGX_MUSL_CC}: ${LKL_LIB} ${SGX_MUSL_BUILD}
	cd ${SGX_MUSL}; [ -f config.mak ] || ./configure \
		--prefix=${SGX_MUSL_BUILD} \
		--lklheaderdir=${LKL_BUILD}/include/ \
		--lkllib=${LKL_LIB} \
		--enable-debug \
		--disable-optimize \
		--disable-shared
	+${MAKE} -C ${SGX_MUSL} install

${LKL_LIB}: lkl

${TESTS_BUILD}/%: ${TESTS}/%.c ${TESTS_BUILD} ${SGX_MUSL_CC}
	${SGX_MUSL_CC} ${TESTS_CFLAGS} -o $@ $< ${TESTS_LDFLAGS}

.PHONY: host-musl sgx-musl lkl tests clean

host-musl: ${HOST_MUSL_CC}

sgx-musl: ${SGX_MUSL_CC}

lkl: ${HOST_MUSL_CC} ${LKL_BUILD}
	+DESTDIR=${LKL_BUILD} ${MAKE} -C ${LKL}/tools/lkl CC="${HOST_MUSL_CC}" PREFIX="" \
		ALL_LIBRARIES=liblkl.a libraries_install
	+DESTDIR=${LKL_BUILD} ${MAKE} -C ${LKL}/tools/lkl CC="${HOST_MUSL_CC}" PREFIX="" \
		headers_install
	# Bugfix, prefix symbol that collides with musl's one
	find ${LKL_BUILD}/include/ -type f -exec sed -i 's/struct ipc_perm/struct lkl_ipc_perm/' {} \;

tests: $(TESTS_OBJ)
	${MAKE} -j1 testrun

testrun:
	@printf "    [*] 01-compiler: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/01-compiler; \
		[ $$? -eq 42 ] && echo "OK"
	@printf "    [*] 02-lkl-host-print: "
	@RES=$$(MUSL_NOLKL=1 ${TESTS_BUILD}/02-lkl-host-print); \
		[ $$? -eq 0 -a $$RES = "OK" ] && echo "OK"
	@printf "    [*] 03-lkl-host-panic: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/03-lkl-host-panic >/dev/null 2>&1; \
		[ $$? -ne 0 ] && echo "OK"
	@printf "    [*] 04-lkl-host-mem: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/04-lkl-host-mem; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 05-lkl-host-thread: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/05-lkl-host-thread; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 06-lkl-host-semaphore: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/06-lkl-host-semaphore; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 07-lkl-host-mutex: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/07-lkl-host-mutex; \
		[ $$? -eq 0 ] && echo "OK
	@printf "    [*] 08-lkl-host-tls: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/08-lkl-host-tls; \
		[ $$? -eq 0 ] && echo "OK"

clean:
	rm -rf ${BUILD_DIR}
	+${MAKE} -C ${HOST_MUSL} distclean
	+${MAKE} -C ${SGX_MUSL} distclean
	+${MAKE} -C ${LKL} clean
	+${MAKE} -C ${LKL}/tools/lkl clean

