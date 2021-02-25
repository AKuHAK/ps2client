#ifndef __WINTHREAD_H__
#define __WINTHREAD_H__

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef HANDLE pthread_mutex_t;

int pthread_mutex_init(pthread_mutex_t *mutex, void *unused);

int pthread_mutex_destroy(pthread_mutex_t *mutex);

int pthread_mutex_lock(pthread_mutex_t *mutex);

int pthread_mutex_unlock(pthread_mutex_t *mutex);

#endif /*_WIN32 */

#endif /*__WINTHREAD_H__*/
