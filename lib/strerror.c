#include	<stdio.h>

#ifndef LINUX
extern char	*sys_errlist[];
extern int	sys_nerr;
#endif

char *
strerror(int err)
{
    static char buf[1024];
    
    /* 使用现代的 strerror_r 函数替代过时的 sys_nerr/sys_errlist */
    if (strerror_r(err, buf, sizeof(buf)) != 0) {
        snprintf(buf, sizeof(buf), "Unknown error %d", err);
    }
    return buf;
}
