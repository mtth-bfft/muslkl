include ../config.mak

TESTS_SRC ?= $(sort $(wildcard $(TESTS)/*.c))
TESTS_OBJ ?= $(addprefix $(TESTS_BUILD)/, $(notdir $(TESTS_SRC:.c=)))

.PHONY: default all tests runtests memcached

default: tests

all: tests memcached

ifeq ("$(wildcard $(SGX_MUSL_CC))","")
$(error "Please run `make sgx-musl` in the main directory to actually have a compiler to test")
endif

${TESTS_BUILD}/%: %.c
	${SGX_MUSL_CC} ${CFLAGS} -o $@ $< ${LDFLAGS}

tests: $(TESTS_OBJ)
	@${MAKE} -j1 serial_tests

serial_tests:
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
	@(MUSL_NOLKL=0 MUSL_ETHREADS=4 MUSL_STHREADS=4 MUSL_HD="" MUSL_TAP=tap0 MUSL_IP4=10.0.10.10 MUSL_GW4=10.0.10.254 ${TESTS_BUILD}/21-lkl-net >/dev/null &) && (ping -W1 -c15 -i0.2 -D -O -q -n 10.0.10.10 >/dev/null); \
		[ $$? -eq 0 ] && echo "OK"

libevent: submodules
	cd ${LIBEVENT}; [ -f configure ] || ./autogen.sh
	MUSL_NOLKL=0 MUSL_ETHREADS=6 MUSL_STHREADS=6 MUSL_HD="" MUSL_TAP="" CC=${SGX_MUSL_CC} ./configure \
		--disable-shared \
		--disable-openssl \
		--prefix=${LIBEVENT_BUILD}
	+${MAKE} -C ${LIBEVENT}
	+${MAKE} -C ${LIBEVENT} install

submodules:
	[ "$(FORCE_SUBMODULES_VERSION)" = "true" ] || git submodule init ${LIBEVENT}
	[ "$(FORCE_SUBMODULES_VERSION)" = "true" ] || git submodule update ${LIBEVENT}

memcached:
	cd tests/80-memcached; git checkout tags/1.4.25
	cd tests/80-memcached; [ -f configure ] || ./autogen.sh
	cd tests/80-memcached; MUSL_ETHREADS=6 MUSL_STHREADS=6 CC=${SGX_MUSL_CC} ./configure --prefix=${TESTS_BUILD}/80-memcached/ --with-libevent=${TESTS_BUILD}/80-libevent/ --mandir=/tmp --disable-coverage
	cd tests/80-memcached; make
	cd tests/80-memcached; make install
