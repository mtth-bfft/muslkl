include config.mak

default: all

all: sgx-musl tests

host-musl: | submodules ${HOST_MUSL_BUILD}
	cd ${HOST_MUSL}; [ -f config.mak ] || CFLAGS="$(MUSL_CFLAGS)" ./configure \
		$(MUSL_CONFIGURE_OPTS) \
		--prefix=${HOST_MUSL_BUILD} \
		--disable-shared
	+${MAKE} -C ${HOST_MUSL} install
	ln -fs ${LINUX_HEADERS_INC}/linux/ ${HOST_MUSL_BUILD}/include/
	ln -fs ${LINUX_HEADERS_INC}/asm/ ${HOST_MUSL_BUILD}/include/
	ln -fs ${LINUX_HEADERS_INC}/asm-generic/ ${HOST_MUSL_BUILD}/include/

lkl: host-musl | submodules ${LKL_BUILD} ${TOOLS_BUILD}
	+DESTDIR=${LKL_BUILD} ${MAKE} -C ${LKL}/tools/lkl CC="${HOST_MUSL_CC}" PREFIX="" \
		ALL_LIBRARIES=liblkl.a libraries_install
	+DESTDIR=${LKL_BUILD} ${MAKE} -C ${LKL}/tools/lkl CC="${HOST_MUSL_CC}" PREFIX="" \
		headers_install
	#TODO: apply before makes, and to the entire ${LKL} folder?
	# Bugfix, prefix symbol that collides with musl's one
	find ${LKL_BUILD}/include/ -type f -exec sed -i 's/struct ipc_perm/struct lkl_ipc_perm/' {} \;
	${HOST_MUSL_CC} ${CFLAGS} -o ${TOOLS_BUILD}/lkl_syscalls ${TOOLS}/lkl_syscalls.c ${LDFLAGS}
	${TOOLS_BUILD}/lkl_syscalls > ${LKL_BUILD}/include/lkl/syscall.h

sgx-musl: lkl | submodules ${SGX_MUSL_BUILD}
	cd ${SGX_MUSL}; [ -f config.mak ] || CFLAGS="$(MUSL_CFLAGS)" ./configure \
		$(MUSL_CONFIGURE_OPTS) \
		--prefix=${SGX_MUSL_BUILD} \
		--lklheaderdir=${LKL_BUILD}/include/ \
		--lkllib=${LKL_LIB} \
		--disable-shared
	+${MAKE} -C ${SGX_MUSL} install

.PHONY: submodules host-musl sgx-musl lkl tests clean

${BUILD_DIR} ${TOOLS_BUILD} ${LKL_BUILD} ${HOST_MUSL_BUILD} ${SGX_MUSL_BUILD}:
	@mkdir -p $@

tests: sgx-musl
	+${MAKE} -C ${TESTS} tests

submodules:
	[ "$(FORCE_SUBMODULES_VERSION)" = "true" ] || git submodule init ${LKL} ${HOST_MUSL} ${SGX_MUSL}
	[ "$(FORCE_SUBMODULES_VERSION)" = "true" ] || git submodule update ${LKL} ${HOST_MUSL} ${SGX_MUSL}

clean:
	rm -rf ${BUILD_DIR}
	+${MAKE} -C ${HOST_MUSL} distclean
	+${MAKE} -C ${SGX_MUSL} distclean
	+${MAKE} -C ${LKL} clean
	+${MAKE} -C ${LKL}/tools/lkl clean
	+${MAKE} -C ${TESTS} clean
