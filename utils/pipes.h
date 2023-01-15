#ifndef __UTILS_PIPES_H__
#define __UTILS_PIPES_H__

#include "protocol.h"

void pipe_create(char *pipeName);
int pipe_open(char *pipeName, int mode);
void pipe_write(int pipe, packet_t *packet);
void pipe_close(int pipe);
packet_t pipe_read(int pipe);
void pipe_destroy(char *pipeName);

#endif