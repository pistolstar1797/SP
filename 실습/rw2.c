int readcnt, writecnt;
sem_t rmutex, wmutex, readTry, w; 

void reader(void) {
    P(&readTry);
    P(&rmutex);
    readcnt++;
    if (readcnt == 1)   /* first in */
        P(&w);
    V(&rmutex);
    V(&readTry);

    /* reading */

    P(&rmutex);
    readcnt--;
    if (readcnt == 0)   /* last out */
        V(&w);
    V(&rmutex);
}

void writer(void) {
    P(&wmutex);
    writecnt++;
    if (writecnt == 1)  /* first in */
        P(&readTry);
    V(&wmutex);

    P(&w);

    /* writing */

    V(&w);

    P(&wmutex);
    writecnt--;
    if (writecnt == 0)  /* last out */
        V(&readTry);
    V(&wmutex);
}