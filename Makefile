include config.mak

.PHONY: host-musl lkl openssl sgx-musl tests tools clean

default: all
# Default is to build everything and run a basic test suite
all: sgx-musl tests

tests: sgx-musl tools
	+${MAKE} -C ${TESTS}

# Vanilla Musl compiler
host-musl ${HOST_MUSL_CC}: | ${HOST_MUSL}/.git ${HOST_MUSL_BUILD}
	cd ${HOST_MUSL}; [ -f config.mak ] || CFLAGS="$(MUSL_CFLAGS)" ./configure \
		$(MUSL_CONFIGURE_OPTS) \
		--prefix=${HOST_MUSL_BUILD} \
		--disable-shared
	+${MAKE} -C ${HOST_MUSL} install
	ln -fs ${LINUX_HEADERS_INC}/linux/ ${HOST_MUSL_BUILD}/include/
	ln -fs ${LINUX_HEADERS_INC}/asm/ ${HOST_MUSL_BUILD}/include/
	ln -fs ${LINUX_HEADERS_INC}/asm-generic/ ${HOST_MUSL_BUILD}/include/

# LKL's static library and include/ header directory
lkl ${LIBLKL}: ${HOST_MUSL_CC} | ${LKL}/.git ${LKL_BUILD}
	+DESTDIR=${LKL_BUILD} ${MAKE} -C ${LKL}/tools/lkl CC=${HOST_MUSL_CC} PREFIX="" \
		ALL_LIBRARIES=liblkl.a libraries_install
	+DESTDIR=${LKL_BUILD} ${MAKE} -C ${LKL}/tools/lkl CC=${HOST_MUSL_CC} PREFIX="" \
		headers_install
	#TODO: apply before makes, and to the entire ${LKL} folder?
	# Bugfix, prefix symbol that collides with musl's one
	find ${LKL_BUILD}/include/ -type f -exec sed -i 's/struct ipc_perm/struct lkl_ipc_perm/' {} \;
	+${MAKE} headers_install -C ${LKL} ARCH=lkl INSTALL_HDR_PATH=${LKL_BUILD}/

# OpenSSL's static library and include/ header directory
openssl ${LIBCRYPTO}: ${HOST_MUSL_CC} | ${OPENSSL}/.git ${OPENSSL_BUILD}
	cd ${OPENSSL}; [ -f Makefile ] || CC=${HOST_MUSL_CC} ./config \
		--prefix=${OPENSSL_BUILD}/ \
		--openssldir=${OPENSSL_BUILD}/openssl/ \
		threads no-zlib no-shared
	sed -i 's/^CFLAG=/CFLAG= -fPIC /' ${OPENSSL}/Makefile
	+CC=${HOST_MUSL_CC} ${MAKE} -C ${OPENSSL} depend
	+CC=${HOST_MUSL_CC} ${MAKE} -C ${OPENSSL}
	+CC=${HOST_MUSL_CC} ${MAKE} -C ${OPENSSL} install

tools: ${TOOLS_OBJ}

# Generic tool rule (doesn't actually depend on lkl_lib, but on LKL headers)
${TOOLS_BUILD}/%: ${TOOLS}/%.c ${HOST_MUSL_CC} ${LIBCRYPTO} ${LKL_LIB} | ${TOOLS_BUILD}
	${HOST_MUSL_CC} ${MY_CFLAGS} -I${LKL_BUILD}/include/ -o $@ $< ${MY_LDFLAGS}

# More headers required by SGX-Musl not exported by LKL, given by a custom tool's output
${LKL_SGXMUSL_HEADERS}: ${LKL_BUILD}/include/lkl/%.h: ${TOOLS_BUILD}/lkl_%
	$< > $@

# LKL-SGX-Musl compiler
sgx-musl ${SGX_MUSL_CC}: ${LIBLKL} ${LIBCRYPTO} ${LKL_SGXMUSL_HEADERS} | ${SGX_MUSL_BUILD}
	cd ${SGX_MUSL}; [ -f config.mak ] || CFLAGS="$(MUSL_CFLAGS)" ./configure \
		$(MUSL_CONFIGURE_OPTS) \
		--prefix=${SGX_MUSL_BUILD} \
		--lklheaderdir=${LKL_BUILD}/include/ \
		--lkllib=${LIBLKL} \
		--opensslheaderdir=${OPENSSL_BUILD}/include/ \
		--openssllib=${LIBCRYPTO} \
		--disable-shared
	+${MAKE} -C ${SGX_MUSL} install

# Build directories (one-shot after git clone or clean)
${BUILD_DIR} ${TOOLS_BUILD} ${LKL_BUILD} ${HOST_MUSL_BUILD} ${SGX_MUSL_BUILD} ${OPENSSL_BUILD}:
	@mkdir -p $@

# Submodule initialisation (one-shot after git clone)
${HOST_MUSL}/.git ${LKL}/.git ${OPENSSL}/.git ${SGX_MUSL}/.git:
	[ "$(FORCE_SUBMODULES_VERSION)" = "true" ] || git submodule update --init $($@:.git=)

clean:
	rm -rf ${BUILD_DIR}
	+${MAKE} -C ${HOST_MUSL} distclean || true
	+${MAKE} -C ${SGX_MUSL} distclean || true
	+${MAKE} -C ${OPENSSL} clean || true
	+${MAKE} -C ${LKL} clean || true
	+${MAKE} -C ${LKL}/tools/lkl clean || true
	+${MAKE} -C ${TESTS} clean || true
	rm -f ${HOST_MUSL}/config.mak
	rm -f ${SGX_MUSL}/config.mak
	rm -f ${OPENSSL}/Makefile
