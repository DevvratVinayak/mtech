#ifndef _EXEC_TRACE_H
#define _EXEC_TRACE_H

#include <linux/types.h>

typedef void (*exec_trace_enter_t)(const char *func);
typedef void (*exec_trace_exit_t)(const char *func);


extern exec_trace_enter_t exec_trace_enter_hook;
extern exec_trace_exit_t  exec_trace_exit_hook;


static inline void trace_exec_enter(const char *func)
{
    exec_trace_enter_t fn;
    fn = exec_trace_enter_hook;

    if (unlikely(fn))
        fn(func);
}

static inline void trace_exec_exit(const char *func)
{
    exec_trace_exit_t fn;

    fn = exec_trace_exit_hook;

    if (unlikely(fn))
        fn(func);
}


#define TRACE_EXEC_START()  trace_exec_enter(__func__)
#define TRACE_EXEC_END()    trace_exec_exit(__func__)

#endif
