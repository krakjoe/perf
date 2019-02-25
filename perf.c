/* perf extension for PHP */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <time.h>
#include <signal.h>
#include <pthread.h>

#include "php.h"
#include "ext/standard/info.h"

#include "src/monitor.h"

#include "php_perf.h"

typedef struct _php_perf_el_t {
    zend_string *filename;
    zend_string *scope;
    zend_string *function;
    zend_long    lineno;
} php_perf_el_t;

typedef struct _php_perf_globals_t {
    pthread_t thread;
    timer_t   timer;
    php_perf_monitor_t *monitor;
    zend_execute_data *frame;
} php_perf_globals_t;

ZEND_TLS php_perf_globals_t php_perf_globals;

#define PEG(e) php_perf_globals.e

static zend_bool php_perf_enabled = 0;
static zend_long php_perf_interval = -1;

void (*zend_interrupt_callback) (zend_execute_data *) = NULL;

static void php_perf_interrupt(zend_execute_data *frame) {
    PEG(frame) = frame;
    
    php_perf_monitor_set(
        PEG(monitor), PHP_PERF_COLLECT, 1);
    php_perf_monitor_wait(PEG(monitor), PHP_PERF_DONE);
    
    if (zend_interrupt_callback) {
        zend_interrupt_callback(frame);
    }
}

static void* php_perf_routine(void *pg) {
    php_perf_globals_t *peg = 
        (php_perf_globals_t*) pg;
    uint32_t state;
    HashTable *perf = 
        (HashTable*) calloc(1, sizeof(HashTable));
    
    zend_hash_init(perf, 10000, NULL, NULL, 1);
    
    php_perf_monitor_set(peg->monitor, PHP_PERF_WAITING, 1);

    while ((state = php_perf_monitor_wait(peg->monitor, PHP_PERF_COLLECT|PHP_PERF_CLOSE))) {
        php_perf_el_t el;

        if ((state & PHP_PERF_CLOSE)) {
            break;
        }

        /* safe to write element, be fast, 
            file, function, and (I think) class names are interned and persistent (do not copy) 
            may be an internal function call (check func->type) */
        el.filename = peg->frame->func->op_array.filename;
        el.lineno   = peg->frame->opline->lineno;

        zend_hash_next_index_insert_mem(
            perf, &el, sizeof(php_perf_el_t));

        php_perf_monitor_set(peg->monitor, PHP_PERF_DONE, 1);
    }
    
    php_perf_monitor_set(peg->monitor, PHP_PERF_DONE, 1);
    
    pthread_exit((void*) perf);

    return NULL;
}

/* {{{ */
PHP_MINIT_FUNCTION(perf)
{
    char *interval = getenv("PHP_PERF_INTERVAL");
    
    if (interval) {
        /* check if positive interval and within ms range */
        php_perf_interval = 
            zend_atoi(interval, strlen(interval));
        
        /* only set if sensible interval */    
        php_perf_enabled = 1;
        
        zend_interrupt_callback = zend_interrupt_function;
        zend_interrupt_function = php_perf_interrupt;
    }
    
    return SUCCESS;
} /* }}} */

/* {{{ */
static void php_perf_notify(union sigval sv) {
    zend_bool *interrupt = 
        (zend_bool*) sv.sival_ptr;
    *interrupt = 1;
} /* }}} */

/* {{{ */
static int php_perf_startup() {
    struct sigevent ev;
    struct itimerspec its;
    struct timespec ts;
    
    memset(&ev, 0, sizeof(ev));
    
    ev.sigev_notify = SIGEV_THREAD;
    ev.sigev_notify_function = php_perf_notify;
    ev.sigev_value.sival_ptr = &EG(vm_interrupt);
    
    if (timer_create(CLOCK_MONOTONIC, &ev, &PEG(timer)) != SUCCESS) {
        return FAILURE;
    }
    
    ts.tv_sec = 0;
    /* take interval in microseconds, 
        nano seconds are not useful to humans */
    ts.tv_nsec = php_perf_interval * 1000;
    
    its.it_interval = ts;
    its.it_value = ts;
    
    if (timer_settime(PEG(timer), 0, &its, NULL) != SUCCESS) {
        return FAILURE;
    }
    
    return SUCCESS;
} /* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(perf)
{
#if defined(ZTS) && defined(COMPILE_DL_PERF)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

    if (!php_perf_enabled) {
        return SUCCESS;
    }

    PEG(monitor) = php_perf_monitor_create();
    
    if (pthread_create(&PEG(thread), NULL, php_perf_routine, &php_perf_globals) != SUCCESS) {
        php_perf_monitor_destroy(PEG(monitor));
        return FAILURE;
    }

    if (php_perf_startup() != SUCCESS) {
        return FAILURE;
    }

    php_perf_monitor_wait(PEG(monitor), PHP_PERF_WAITING);
    
	return SUCCESS;
}
/* }}} */

/* {{{ */
static void php_perf_process(HashTable *perf) {
    uint32_t idx;
    php_perf_el_t *el;

    php_printf("got %d frames\n", zend_hash_num_elements(perf));
    
    /*
    ZEND_HASH_FOREACH_NUM_KEY_PTR(perf, idx, el) {
        do something with traces, write callgrind, I dunno
    } ZEND_HASH_FOREACH_END();
    */

    zend_hash_destroy(perf);
    free(perf);
} /* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(perf)
{
    HashTable *perf;

    if (!php_perf_enabled) {
        return SUCCESS;
    }
    
    timer_delete(PEG(timer));
    
    php_perf_monitor_set(
        PEG(monitor), PHP_PERF_CLOSE, 1);
    php_perf_monitor_wait(
        PEG(monitor), PHP_PERF_DONE);
    
    pthread_join(
        PEG(thread), (void*) &perf);
        
    php_perf_monitor_destroy(PEG(monitor));

    php_perf_process(perf);

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(perf)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "perf support", "enabled");
	php_info_print_table_end();
}
/* }}} */

/* {{{ perf_functions[]
 */
static const zend_function_entry perf_functions[] = {
	PHP_FE_END
};
/* }}} */

/* {{{ perf_module_entry
 */
zend_module_entry perf_module_entry = {
	STANDARD_MODULE_HEADER,
	"perf",
	perf_functions,
	PHP_MINIT(perf),
	NULL,
	PHP_RINIT(perf),
	PHP_RSHUTDOWN(perf),
	PHP_MINFO(perf),
	PHP_PERF_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PERF
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(perf)
#endif
