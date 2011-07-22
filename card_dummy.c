/* Routines for reading/writing SIM card
 * Token is stored at the last SMS record
 * Author: Proton
 * E-mail: feisuzhu@163.com
 * File created: 2011-2-5 21:02
 * License: BSD
 */

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>

#include "common.h"

#define LEN sizeof(token)

char data[LEN];
const char *fn = "/dev/shm/dummycard";

static int open_card()
{
    int fd;
    
    memset(data, 0xFF, sizeof data);
    fd = open(fn, O_RDONLY);
    read(fd, data, sizeof data);
    close(fd);
    return 1;
}

static int close_card()
{
    int fd;
    
    fd = open(fn, O_WRONLY | O_TRUNC | O_CREAT, 0777);
    write(fd, data, sizeof data);
    close(fd);
    return 1;
}

int read_token(token *buf)
{
    open_card(); 
    memcpy(buf, data, LEN);
    close_card(); 
    return 1;
}

int write_token(token *buf)
{
    open_card();
    memcpy(data, buf, LEN);
    close_card();
    return 1;
}
