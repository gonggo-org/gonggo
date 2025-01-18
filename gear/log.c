#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

#include "log.h"
#include "define.h"
#include "globaldata.h"

#ifdef HAVE_ST_BIRTHTIME
#define birthtime(x) x.st_birthtime
#else
#define birthtime(x) x.st_ctime
#endif

//property:
static bool has_gonggo_log_lock = false;
static pthread_mutex_t gonggo_log_lock;
static pid_t gonggo_log_pid = 0;
static const char *gonggo_log_path = NULL;

void gonggo_log_context_init(pid_t pid, const char *path) {
    if(!has_gonggo_log_lock) {
        pthread_mutexattr_t mtx_attr;
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
        pthread_mutex_init(&gonggo_log_lock, &mtx_attr);
        pthread_mutexattr_destroy(&mtx_attr);
        has_gonggo_log_lock = true;
    }    
    gonggo_log_pid = pid;
    gonggo_log_path = path;
}

void gonggo_log_context_destroy(void) {
    if(has_gonggo_log_lock) {
        pthread_mutex_destroy(&gonggo_log_lock);
        has_gonggo_log_lock = false;
    }
    gonggo_log_pid = 0;
    gonggo_log_path = NULL;
}

void gonggo_log(const char *level, const char *fmt, ...) {
    time_t t;
	struct tm tm_now, tm_file;
	char filepath[100], tmstr[TMSTRBUFLEN], *buf;
	struct stat st;
	int flags, filenum, n;
	struct timespec *tspec;
    va_list args;
    size_t size;
    mode_t mode;

    if(has_gonggo_log_lock) {
        pthread_mutex_lock(&gonggo_log_lock);
    }

    flags = 0;
    t = time(NULL);
    tm_now = *localtime(&t);
    strftime(tmstr, TMSTRBUFLEN, "%a", &tm_now);
    sprintf(filepath, "%s/%s-%s.log", gonggo_log_path, gonggo_name, tmstr);

    mode = 0;
    if(stat(filepath, &st) == 0) {
        tspec = (struct timespec*)&birthtime(st);
		t = tspec->tv_sec + (tspec->tv_nsec/1000000000);
		tm_file = *localtime(&t);
		if( tm_now.tm_year == tm_file.tm_year && tm_now.tm_mon == tm_file.tm_mon && tm_now.tm_mday == tm_file.tm_mday)
			flags = O_APPEND;
		else
			flags = O_TRUNC | O_APPEND;
    } else {
        if(errno == ENOENT) {
            flags = O_CREAT | O_APPEND;
            mode = S_IRUSR | S_IWUSR;
        }
    }

    if(flags!=0) {
        filenum = open(filepath, flags | O_WRONLY, mode);
		if(filenum != -1) {
            buf = NULL;
            size = 0;

            va_start(args, fmt);
            n = vsnprintf(buf, size, fmt, args);
            va_end(args);

            size = (size_t) n + 1; //one extra byte for zero string terminator
            buf = malloc(size);

            if( buf != NULL ) {
                va_start(args, fmt);
                n = vsnprintf(buf, size, fmt, args);
                va_end(args);

                if( n > 0 ) {
                    strftime(tmstr, TMSTRBUFLEN, "%Y-%m-%d %H:%M:%S %Z", &tm_now);
                    write(filenum, tmstr, strlen(tmstr));
                    snprintf(tmstr, TMSTRBUFLEN, " [%d] ", gonggo_log_pid);
                    write(filenum, tmstr, strlen(tmstr));
                    write(filenum, level, strlen(level));
                    write(filenum, ": ", 2);
                    write(filenum, buf, n);
                    write(filenum, "\n", 1);
                }

                free(buf);
            }

			close(filenum);
		}
    }

    if(has_gonggo_log_lock) {
        pthread_mutex_unlock(&gonggo_log_lock);
    }

    return;
}