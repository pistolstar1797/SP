#include "csapp.h"
#include "sbuf.h"

/* Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t *sp, int n)
{
  sp->buf = Calloc(n, sizeof(int));
  sp->n = n;
  sp->front = sp->rear;
}

/* Clean up buffer sp */
void sbuf_deinit(sbuf_t *sp)
{
  Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int v)
{
  int next = sp->rear+1;
  if(next == sp->n)
    next = 0;
  while(next == sp->front)
    pthread_yield();
  sp->buf[sp->rear] = v;
  sp->rear = next;
}

int sbuf_remove(sbuf_t *sp)
{
  while(sp->front == sp->rear)
    pthread_yield();
  int next = sp->front+1;
  if(next == sp->n)
    next = 0;
  int val = sp->buf[sp->front];
  sp->front = next;
  return val;
}