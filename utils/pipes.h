#ifndef __UTILS_PIPES_H__
#define __UTILS_PIPES_H__

void pipe_create(char *pipeName);
int pipe_open(char *pipeName, int mode);
void pipe_close(int pipe);
void pipe_destroy(char *pipeName);

#endif