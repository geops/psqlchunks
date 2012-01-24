#include <ctime>

#include "debug.h"

static FILE * log_file = NULL;
static char buf_timestamp[100];

FILE * debug_get_logfile()
{
    return log_file != NULL ? log_file : stderr;
}


void debug_set_logfile(FILE * lf)
{
    log_file = lf;
}


const char * debug_formatted_timestamp()
{
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    strftime(buf_timestamp ,100 ,"%Y-%m-%d %X", timeinfo);

    return buf_timestamp;
}

