dnl config.m4 for extension perf

PHP_ARG_ENABLE(perf, whether to enable perf support,
[  --enable-perf          Enable perf support], no)

if test "$PHP_PERF" != "no"; then
  AC_DEFINE(HAVE_PERF, 1, [ Have perf support ])
  
  PHP_ADD_BUILD_DIR($ext_builddir/src, 1)
  PHP_ADD_INCLUDE($ext_srcdir)

  PHP_NEW_EXTENSION(perf, perf.c src/monitor.c, $ext_shared)
fi
