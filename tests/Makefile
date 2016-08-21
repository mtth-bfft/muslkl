include ../config.mak

.PHONY: default all tests memcached nginx clean

default: tests
all: tests memcached nginx

${TESTS_BUILD}/%: %.c ${SGX_MUSL_CC} | ${TESTS_BUILD}
	${SGX_MUSL_CC} ${MY_CFLAGS} -o $@ $< ${MY_LDFLAGS}

# Run tests (in a sub-target to prevent Make from parallelising different tests)
tests: $(TESTS_OBJ)
	@${MAKE} -j1 serial_tests

tests_norun: $(TESTS_OBJ)

serial_tests:
	@printf "    [*] 01-compiler: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/01-compiler >/dev/null; \
		[ $$? -eq 42 ] && echo "OK"
	@printf "    [*] 02-pthreads: "
	@MUSL_NOLKL=1 ${MUSLKLSGX_STD_RUN_OPTS} ${TESTS_BUILD}/02-pthreads >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 03-pthreads-sleep: "
	@MUSL_NOLKL=1 ${TESTS_BUILD}/03-pthreads-sleep >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
#	@printf "    [*] 04-lthreads-sleep: "
#	@MUSL_NOLKL=1 ${MUSLKLSGX_STD_RUN_OPTS} ${TESTS_BUILD}/04-lthreads-sleep; \
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
	@MUSL_NOLKL=1 ${MUSLKLSGX_STD_RUN_OPTS} ${TESTS_BUILD}/18-lkl-host-timer >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@printf "    [*] 19-lkl-boot: "
	@MUSL_NOLKL=0 ${MUSLKLSGX_STD_RUN_OPTS} MUSL_TAP="" MUSL_HD="" ${TESTS_BUILD}/19-lkl-boot >/dev/null; \
		[ $$? -eq 42 ] && echo "OK"
	@printf "    [*] 20-lkl-disk: "
	@cd ${TESTS}; tar -xvf 20-lkl-disk.tar.gz >/dev/null
	@MUSL_NOLKL=0 ${MUSLKLSGX_STD_RUN_OPTS} MUSL_TAP="" MUSL_HD=${TESTS}/20-lkl-disk.img ${TESTS_BUILD}/20-lkl-disk >/dev/null; \
		[ $$? -eq 0 ] && echo "OK"
	@rm -f ${TESTS}/20-lkl-disk.img
	@printf "    [*] 21-lkl-net: "
	@(MUSL_NOLKL=0 ${MUSLKLSGX_STD_RUN_OPTS} MUSL_TAP=tap0 MUSL_IP4=10.0.1.1 MUSL_GW4=10.0.1.254 MUSL_MASK4=24 MUSL_HD="" \
		${TESTS_BUILD}/21-lkl-net >/dev/null &) && (ping -W1 -c15 -i0.2 -D -O -q -n 10.0.1.1 >/dev/null); \
		[ $$? -eq 0 ] && echo "OK"


libevent: ${LIBEVENT_BUILD} | ${LIBEVENT}/.git
	cd ${LIBEVENT}; [ -f configure ] || CC=${SGX_MUSL_CC} ./autogen.sh
	cd ${LIBEVENT}; MUSL_NOLKL=0 ${MUSLKLSGX_STD_RUN_OPTS} MUSL_HD="" MUSL_TAP="" CC=${SGX_MUSL_CC} ./configure \
		--disable-shared \
		--disable-openssl \
		--prefix=${LIBEVENT_BUILD}
	#FIXME: autoconf fails to detect type sizes. Hard-code them for x86_64 with a sed.
	sed  -i -e 's/EVENT__SIZEOF_INT $$/EVENT__SIZEOF_INT 4/' \
		-e 's/EVENT__SIZEOF_LONG $$/EVENT__SIZEOF_LONG 8/' \
		-e 's/EVENT__SIZEOF_LONG_LONG $$/EVENT__SIZEOF_LONG_LONG 8/' \
		-e 's/EVENT__SIZEOF_OFF_T $$/EVENT__SIZEOF_OFF_T 8/' \
		-e 's/EVENT__SIZEOF_PTHREAD_T $$/EVENT__SIZEOF_PTHREAD_T 8/' \
		-e 's/EVENT__SIZEOF_SHORT $$/EVENT__SIZEOF_SHORT 2/' \
		-e 's/EVENT__SIZEOF_SIZE_T $$/EVENT__SIZEOF_SIZE_T 8/' \
		-e 's/EVENT__SIZEOF_VOID_P $$/EVENT__SIZEOF_VOID_P 8/' ${LIBEVENT}/include/event2/event-config.h
	+${MAKE} -C ${LIBEVENT}
	+${MAKE} -C ${LIBEVENT} install

memcached: libevent ${MEMCACHED_BUILD} | ${MEMCACHED}/.git
	cd ${MEMCACHED}; [ -f configure ] || ./autogen.sh
	cd ${MEMCACHED}; MUSL_NOLKL=0 ${MUSLKLSGX_STD_RUN_OPTS} MUSL_HD="" MUSL_TAP="" CC=${SGX_MUSL_CC} ./configure \
		--with-libevent=${LIBEVENT_BUILD} \
		--mandir=/tmp \
		--disable-coverage \
		--prefix=${MEMCACHED_BUILD}
	+${MAKE} -C ${MEMCACHED}
	+${MAKE} -C ${MEMCACHED} install

nginx: ${NGINX_BUILD} | ${NGINX}/.git
	cd ${NGINX}; MUSL_NOLKL=0 ${MUSLKLSGX_STD_RUN_OPTS} MUSL_HD="" MUSL_TAP="" CC=${SGX_MUSL_CC} ./auto/configure \
		${NGINX_CONFIGURE_OPTS} \
		--prefix=${NGINX_BUILD}/ \
		--conf-path=/data/conf/nginx.conf \
		--sbin-path=/none \
		--pid-path=/tmp/nginx.pid \
		--lock-path=/tmp/nginx.lock \
		--error-log-path=/tmp/error.log \
		--http-log-path=/tmp/access.log \
		--http-client-body-temp-path=/tmp/client-body \
		--http-proxy-temp-path=/tmp/proxy \
		--http-fastcgi-temp-path=/tmp/fastcgi \
		--http-fastcgi-temp-path=/tmp/fastcgi \
		--http-uwsgi-temp-path=/tmp/uwsgi \
		--http-scgi-temp-path=/tmp/scgi \
		--without-http_gzip_module \
		--without-http_rewrite_module \
		--with-ipv6
	cd ${NGINX}; make

# Build directories initialisation (one-shot after git clone and make clean)
${LIBEVENT_BUILD} ${MEMCACHED_BUILD} ${NGINX_BUILD} ${TESTS_BUILD}:
	@mkdir -p $@

# Submodule initialisation (one-shot after git clone)
${LIBEVENT}/.git ${MEMCACHED}/.git ${NGINX}/.git:
	[ "$(FORCE_SUBMODULES_VERSION)" = "true" ] || git submodule update --init $($@:.git=)
clean:
	rm -f $(TESTS_OBJ)
	${MAKE} -C ${LIBEVENT} clean || /bin/true
	rm -f ${LIBEVENT}/Makefile
	rm -f ${LIBEVENT}/configure
	${MAKE} -C ${MEMCACHED} clean || /bin/true
	rm -f ${MEMCACHED}/Makefile
	rm -f ${MEMCACHED}/configure
