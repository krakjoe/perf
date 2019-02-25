/* perf extension for PHP */

#ifndef PHP_PERF_H
# define PHP_PERF_H

extern zend_module_entry perf_module_entry;
# define phpext_perf_ptr &perf_module_entry

# define PHP_PERF_VERSION "0.1.0"

# if defined(ZTS) && defined(COMPILE_DL_PERF)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

#endif	/* PHP_PERF_H */
