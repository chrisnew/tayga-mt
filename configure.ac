AC_INIT(tayga, 0.9.2)
AC_CONFIG_SRCDIR(nat64.c)
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AC_CHECK_LIB([apr-1], [apr_pool_initialize, apr_queue_create, apr_thread_create])
AC_CHECK_LIB([aprutil-1], [apr_queue_push])
AC_CHECK_HEADER(apr-1/apu.h)
AC_CHECK_HEADER(apr-1/apr_general.h)
AC_CHECK_HEADER(apr-1/apr_queue.h)
AC_CONFIG_HEADERS(config.h)

AC_PROG_CC

#CFLAGS='-g -Wall'
CFLAGS='-O3 -Wall'

tayga_conf_path=${sysconfdir}/tayga.conf

AC_SUBST(tayga_conf_path)

AC_OUTPUT([Makefile])
