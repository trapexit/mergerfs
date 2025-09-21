#pragma once

#include "fuse_msgbuf.h"

int fuse_receive_buf(struct fuse_session *se,
                     fuse_msgbuf_t       *msgbuf);
void fuse_process_buf(void *data,
                      const fuse_msgbuf_t *msgbuf,
                      struct fuse_chan *ch);
