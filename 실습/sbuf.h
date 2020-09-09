#include "csapp.h"

typedef struct {
  int *buf;
  int n;
  int front;
  int rear;
} sbuf_t;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
int sbuf_empty(sbuf_t *sp);
int sbuf_full(sbuf_t *sp);