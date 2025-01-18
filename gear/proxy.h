#ifndef _PROXY_H_
#define _PROXY_H_

#include <pthread.h>
#include <stdbool.h>

#include "define.h"

/*ProxyActivationState should be the same as proxy*/
enum ProxyActivationState {
  ACTIVATION_INIT = 0,
  ACTIVATION_IDLE = 1,

  ACTIVATION_REQUEST = 2,
  ACTIVATION_PROXY_DYING = 3,
  ACTIVATION_FAILED = 4,
  ACTIVATION_SUCCESS = 5,
  ACTIVATION_DONE = 6,
  ACTIVATION_TERMINATION = 99
};

/*ProxyActivationShm should be the same as proxy*/
typedef struct ProxyActivationShm {
    pthread_mutex_t lock;
    pthread_cond_t dispatcher_wakeup;
    pthread_cond_t proxy_wakeup;
    char proxy_name[PROXYNAMEBUFLEN];
    pid_t pid;
    enum ProxyActivationState state;
} ProxyActivationShm;

/*ProxyChannelState should be the same as proxy*/
enum ProxyChannelState {
    CHANNEL_INIT = 0,
    CHANNEL_IDLE = 1,
    CHANNEL_REQUEST = 2,
    CHANNEL_ACKNOWLEDGED = 3,
    CHANNEL_FAILS = 4,
    CHANNEL_DONE = 5,
    CHANNEL_STOP_REQUEST = 6,
    CHANNEL_TERMINATION = 99
};

/*ProxyChannelShm should be the same as proxy*/
typedef struct ProxyChannelShm {
    pthread_mutex_t lock;
    pthread_cond_t dispatcher_wakeup;
    pthread_cond_t proxy_wakeup;
    pthread_cond_t idle;
    enum ProxyChannelState state;
    char rid[UUIDBUFLEN]; //request id
    size_t payload_buff_length;
} ProxyChannelShm;

typedef struct ProxyChannelContext {
    char *proxy_name;
    bool stop;
    pthread_mutex_t lock;
    pthread_cond_t wakeup;
    ProxyChannelShm *shm;
} ProxyChannelContext;

enum ProxySubscribeState {
    SUBSCRIBE_INIT = 0,
    SUBSCRIBE_IDLE = 1,
    SUBSCRIBE_ANSWER = 2,
    SUBSCRIBE_FAILED = 3,
    SUBSCRIBE_DONE = 4,
    SUBSCRIBE_TERMINATION = 99
};

/*ProxySubscribeShm should be the same as proxy*/
typedef struct ProxySubscribeShm {
    pthread_mutex_t lock;
    pthread_cond_t dispatcher_wakeup;
    pthread_cond_t proxy_wakeup;
    enum ProxySubscribeState state;
    char aid[UUIDBUFLEN]; //answer id
    size_t payload_buff_length;
    bool remove_request;
} ProxySubscribeShm;

typedef struct ProxySubscribeContext {
    char *proxy_name;
    bool stop;
    ProxySubscribeShm *shm;
} ProxySubscribeContext;

/*ProxyAliveMutexShm should be the same as proxy GonggoAliveMutexShm*/
typedef struct ProxyAliveMutexShm {
    pthread_mutex_t lock;
    bool alive;    
} ProxyAliveMutexShm;

typedef struct ProxyAliveContext {
    char *proxy_name;
    pid_t pid;
    volatile bool dead;
    ProxyAliveMutexShm *shm;
} ProxyAliveContext;

extern void proxy_thread_killer_init(void);
extern void proxy_thread_terminating_set(const char *proxy_name);
extern void proxy_thread_terminating_drop(const char *proxy_name);
extern bool proxy_thread_zombie(const char *proxy_name);
extern void proxy_thread_kill(const char* proxy_name);
extern void proxy_thread_kill_all(void);    
extern void proxy_thread_killer_destroy(void);

#endif //_PROXY_H_