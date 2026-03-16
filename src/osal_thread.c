
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <inttypes.h>

#ifdef PLATFORM_Windows
#include <process.h>
#endif


#include "osal_thread.h"

#ifdef PLATFORM_Windows
typedef unsigned int thread_return_t;
#else
typedef void *thread_return_t;
#endif

struct trunner_param_t {
   osal_thread_func_t *fptr;
   void *param;
};

static thread_return_t trunner (void *param)
{
   struct trunner_param_t *tr = param;
   tr->fptr (tr->param);
   free (tr);
#ifdef PLATFORM_Windows
   return 0;
#else
   return NULL;
#endif
}


#define MAX_RETRY_DURATION_MS    (5000)

/* ***************************************************** */

#ifdef PLATFORM_Windows

bool osal_thread_new (osal_thread_t *thandle, osal_thread_func_t *fptr, void *param)
{
   struct trunner_param_t *tr = malloc (sizeof *tr);
   if (!tr) {
      return false;
   }

   tr->fptr = fptr;
   tr->param = param;

   *thandle = (HANDLE)_beginthreadex (NULL, 0, trunner, tr, 0, NULL);

   return *thandle != 0;
}

size_t osal_thread_wait_retry (osal_thread_t *threads, size_t nthreads,
                               size_t retry, size_t interval_ms)
{
   bool rc = true;
   for (size_t i=0; i<nthreads; i++) {
      DWORD e = WaitForSingleObject (threads[i], INFINITE);
      rc = rc && e == WAIT_OBJECT_0;
   }
   return rc;
}

void osal_thread_sleep_ms (size_t milliseconds)
{
   DWORD mask = (DWORD)0xffffffffULL;
   DWORD param = (DWORD)(micro_s & mask);
   Sleep (param);
}

#if 0
void osal_thread_del (osal_thread_t *thandle)
{
    CloseHandle (*thandle);
}
#endif

bool osal_mutex_new (osal_mutex_t *mutex)
{
   *mutex = CreateMutex (NULL, false, NULL);
   return *mutex != NULL;
}

void osal_mutex_del (osal_mutex_t *mutex)
{
   CloseHandle (*mutex);
}

#if 0
bool osal_mutex_acquire (osal_mutex_t *mutex)
{
   DWORD rc = WaitForSingleObject (*mutex, 1);
   return rc == WAIT_OBJECT_0;
}
#endif

bool osal_mutex_acquire_try (osal_mutex_t *mutex)
{
   // TODO:
   return false;
}

bool osal_mutex_acquire_retry (osal_mutex_t *mutex, size_t retry,
                                                    size_t interval_ms)
{
   for (size_t i=0; i<=retry; i++) {
      if (osal_mutex_acquire_try (mutex))
         return true;
      osal_thread_sleep_ms (interval_ms);
   }
   return false;
}

void osal_mutex_release (osal_mutex_t *mutex)
{
   ReleaseMutex (*mutex);
}

#endif

/* ***************************************************** */
#ifdef PLATFORM_POSIX


bool osal_thread_new (osal_thread_t *thandle, osal_thread_func_t *fptr, void *param)
{
   struct trunner_param_t *tr = malloc (sizeof *tr);
   if (!tr) {
      return false;
   }

   tr->fptr = fptr;
   tr->param = param;

   bool ret = pthread_create (thandle, NULL, trunner, tr) == 0;
   if (!ret) {
      *thandle = (uint64_t)-1; // TODO: Not portable!!!
   }
   return ret;
}

static void thread_del (osal_thread_t *thread)
{
   *thread = (uint64_t)-1;
}

static bool thread_wait_retry (osal_thread_t *thread, size_t retry, size_t interval_ms)
{
   size_t duration = interval_ms;
   for (size_t i=0; i<retry; i++) {
      if (*thread == (pthread_t)-1)
         continue;

      if ((pthread_join (*thread, NULL)) == 0) {
         thread_del (thread);
         return true;
      }

      duration = (i + 1) * interval_ms;
      if (duration > MAX_RETRY_DURATION_MS)
         duration = MAX_RETRY_DURATION_MS;
      osal_thread_sleep_ms (duration);
   }
   return false;
}

size_t osal_thread_wait_retry (osal_thread_t *threads, size_t nthreads,
                               size_t retry, size_t interval_ms)
{
   for (size_t i=0; i < nthreads; i++) {
      if (!(thread_wait_retry (&threads[i], retry, interval_ms)))
         return i;
   }
   return nthreads;
}

void osal_thread_sleep_ms (size_t ms)
{
   if (!ms)
      return;

   struct timespec tv, rem;

   tv.tv_sec = (int64_t)ms / 1000;
   tv.tv_nsec = ((int64_t)ms % 1000) * 1000000;

   nanosleep (&tv, &rem);
}

osal_thread_t osal_thread_self (void)
{
   return pthread_self ();
}

bool osal_mutex_new (osal_mutex_t *mutex)
{
   return pthread_mutex_init(mutex, NULL) == 0;
}

void osal_mutex_del (osal_mutex_t *mutex)
{
   pthread_mutex_destroy (mutex);
}

bool osal_mutex_acquire_try (osal_mutex_t *mutex)
{
   if ((pthread_mutex_trylock (mutex)) == 0) {
      return true;
   }
   return false;
}

bool osal_mutex_acquire_retry (osal_mutex_t *mutex, size_t retry,
                                                    size_t interval_ms)
{
   size_t duration = interval_ms;
   for (size_t i=0; i<=retry; i++) {
      if ((osal_mutex_acquire_try (mutex)) == true) {
         return true;
      }
      duration = i * interval_ms;
      if (duration > MAX_RETRY_DURATION_MS)
         duration = MAX_RETRY_DURATION_MS;
      osal_thread_sleep_ms (duration);
   }
   return false;
}

bool osal_mutex_release (osal_mutex_t *mutex)
{
   if ((pthread_mutex_unlock (mutex)) == 0) {
      return true;
   }
   return false;
}

uint64_t osal_atomic_load (volatile uint64_t *dst)
{
   // For Windows: InterlockedExchangeAnd64 (dst, 0xffffffffffffffff);
   uint64_t ret;
   __atomic_load (dst, &ret, __ATOMIC_ACQUIRE);
   return ret;
}

void osal_atomic_store (volatile uint64_t *dst, uint64_t value)
{
   // TODO: For Windows: InterlockedExchangeAdd64 (dst, 0);
   __atomic_store (dst, &value, __ATOMIC_RELEASE);
}

uint64_t osal_atomic_add (volatile uint64_t *dst, uint64_t operand)
{
   return __atomic_fetch_add (dst, operand, __ATOMIC_SEQ_CST);
}

uint64_t osal_atomic_sub (volatile uint64_t *dst, uint64_t operand)
{
   return __atomic_fetch_sub (dst, operand, __ATOMIC_SEQ_CST);
}


bool osal_cmpxchange (volatile uint64_t *target,
                      uint64_t newval, uint64_t comparand)
{
   // TODO: For Windows: InterlockedCompareExchange64 (dst, newval, comparand);
   return __atomic_compare_exchange_n (target,
                                       &comparand,
                                       newval,
                                       false,
                                       __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
}

#endif


bool osal_fastlock_acquire_try (volatile uint64_t *target, const char *id)
{
   (void)id;
   if (osal_cmpxchange (target, 1, 0)) {
      return true;
   }
   return false;
}

bool osal_fastlock_acquire_retry (volatile uint64_t *target, const char *id,
                               size_t retry, size_t interval_ms)
{
   (void)id;
   size_t duration = interval_ms;
   for (size_t i=0; i<retry; i++) {
      if ((osal_cmpxchange (target, 1, 0)))
         return true;
      duration = (i + 1) * interval_ms;
      if (duration > MAX_RETRY_DURATION_MS)
         duration = MAX_RETRY_DURATION_MS;
      osal_thread_sleep_ms (duration);
   }
   return false;
}

void osal_fastlock_release (volatile uint64_t *target, const char *id)
{
    (void)id;
    osal_atomic_store (target, 0);
}

bool osal_semaphore_new (osal_semaphore_t *sem, unsigned int value)
{
   return sem_init (sem, 0, value) == 0;
}

void osal_semaphore_del (osal_semaphore_t *sem)
{
   sem_destroy (sem);
}

bool osal_semaphore_post (osal_semaphore_t *sem)
{
   return sem_post (sem) == 0;
}

bool osal_semaphore_wait_try (osal_semaphore_t *sem)
{
   return sem_trywait (sem) == 0;
}

bool osal_semaphore_wait_retry (osal_semaphore_t *sem, size_t retry,
                                                       size_t interval_ms)
{
   size_t duration = interval_ms;
   for (size_t i=0; i<=retry; i++) {
      if (osal_semaphore_wait_try (sem))
         return true;
      duration = (i + 1) * interval_ms;
      if (duration > MAX_RETRY_DURATION_MS)
         duration = MAX_RETRY_DURATION_MS;
      osal_thread_sleep_ms (duration);
   }
   return false;
}
