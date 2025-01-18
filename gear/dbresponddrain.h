#ifndef _DBRESPONDDRAIN_H_
#define _DBRESPONDDRAIN_H_

extern void db_respond_drain_context_init(long overdue, long period);
extern void db_respond_drain_context_destroy(void);
extern void* db_respond_drain(void *arg);
extern void db_respond_drain_waitfor_started(void);
extern bool db_respond_drain_isstarted(void);
extern void db_respond_drain_stop(void);

#endif //_DBRESPONDDRAIN_H_