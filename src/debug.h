#ifndef __debug_h__
#define __debug_h__

#include <cstdio>
#include <cstring>
#include <cerrno>

FILE * debug_get_logfile();
void debug_set_logfile(FILE *);
const char * debug_formatted_timestamp();

// avoid unused parameter warnings
#define UNUSED_PARAMETER( A ) do { (void)(A); } while (0)

#define _CLEAN_ERRNO() (errno == 0 ? "None" : strerror(errno))

#define _LOG_FUNC(LEVEL, MSG, ...)   fprintf(debug_get_logfile(), \
        LEVEL " [%s] [%s:%d] " MSG "\n", \
        debug_formatted_timestamp(), __FILE__, __LINE__, ##__VA_ARGS__)

#define _LOG_FUNC_ERRNO(LEVEL, MSG, ...)   fprintf(debug_get_logfile(), \
        LEVEL " [%s] [%s:%d] [errno:%s] " MSG "\n", \
        debug_formatted_timestamp(), __FILE__, __LINE__, _CLEAN_ERRNO(), ##__VA_ARGS__)


#define log_info(MSG, ...)  _LOG_FUNC("INFO", MSG, ##__VA_ARGS__)
#define log_warn(MSG, ...)  _LOG_FUNC_ERRNO("WARN", MSG, ##__VA_ARGS__)
#define log_error(MSG, ...)  _LOG_FUNC_ERRNO("ERROR", MSG, ##__VA_ARGS__)


#ifdef DEBUG    /* debugging mode enabled */


#ifdef __GNUC__ // only available on gcc
#define log_debug(MSG, ...)   fprintf(debug_get_logfile(), \
        "DEBUG" " [%s] [%s:%d %s] " MSG "\n", \
        debug_formatted_timestamp(), __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__);
#else
#define log_debug(MSG, ...)  _LOG_FUNC("DEBUG", MSG, ##__VA_ARGS__)
#endif


#else           /* debugging mode disabled */
#define log_debug(MSG, ...)
#endif


#define check(A, M, ...) if(!(A)) { log_error(M, ##__VA_ARGS__); errno=0; goto error; }
#define check_silent(A, M, ...) if(!(A)) { log_debug(M, ##__VA_ARGS__); errno=0; goto error; }

//#define fail(M, ...) { log_error(M, ##__VA_ARGS__); errno=0; goto error; }

#define check_mem(A) check((A), "Out of memory.")

#endif
