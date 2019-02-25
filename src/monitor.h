/*
  +----------------------------------------------------------------------+
  | perf                                                                 |
  +----------------------------------------------------------------------+
  | Copyright (c) The PHP Group                                          |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */
#ifndef HAVE_PERF_MONITOR_H
#define HAVE_PERF_MONITOR_H

#include <pthread.h>

typedef struct _php_perf_monitor_t {
	pthread_mutex_t  mutex;
	pthread_cond_t   condition;
	volatile int32_t state;
} php_perf_monitor_t;

#define PHP_PERF_WAITING 0x00000001
#define PHP_PERF_COLLECT 0x00000010
#define PHP_PERF_CLOSE   0x00000100
#define PHP_PERF_DONE    0x00001000

php_perf_monitor_t* php_perf_monitor_create(void);
int php_perf_monitor_lock(php_perf_monitor_t *m);
int32_t php_perf_monitor_check(php_perf_monitor_t *m, int32_t state);
int php_perf_monitor_unlock(php_perf_monitor_t *m);
int32_t php_perf_monitor_waiting(php_perf_monitor_t *m);
int32_t php_perf_monitor_broadcast(php_perf_monitor_t *m);
int32_t php_perf_monitor_wait(php_perf_monitor_t *m, int32_t state);
int32_t php_perf_monitor_wait_timed(php_perf_monitor_t *monitor, int32_t state, zend_long timeout);
int32_t php_perf_monitor_wait_locked(php_perf_monitor_t *m, int32_t state);
void php_perf_monitor_set(php_perf_monitor_t *monitor, int32_t state, zend_bool lock);
void php_perf_monitor_unset(php_perf_monitor_t *m, int32_t state);
void php_perf_monitor_destroy(php_perf_monitor_t *);
#endif
