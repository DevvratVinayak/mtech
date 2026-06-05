#ifndef _EXEC_HOOK_H
#define _EXEC_HOOK_H

#include <linux/types.h>

typedef void (*exec_trace_enter_t)(const char *func);
typedef void (*exec_trace_exit_t)(const char *func);


extern exec_trace_enter_t exec_trace_enter_hook;
extern exec_trace_exit_t  exec_trace_exit_hook;
extern int optimization_on;

#endif /* _EXEC_HOOK_H */
