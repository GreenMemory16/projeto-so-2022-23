#ifndef __UTILS_PIPES_H__
#define __UTILS_PIPES_H__

#include "protocol.h"
/**
 * Creates a pipe with the given name.
 * If the pipe already exists, it is deleted first.
 */
void pipe_create(char *pipeName);

/**
 * Opens a pipe with the given name.
 *      mode: O_RDONLY or O_WRONLY
 *
 * Returns the pipe file descriptor.
 */
int pipe_open(char *pipeName, int mode);

/**
 * Writes a packet to the given pipe.
 */
void pipe_write(int pipe, packet_t *packet);

/**
 * Closes the given pipe.
 */
void pipe_close(int pipe);

/**
 * Reads a packet from the given pipe.
 */
packet_t pipe_read(int pipe);

/**
 * Deletes the pipe with the given name.
 */
void pipe_destroy(char *pipeName);

#endif