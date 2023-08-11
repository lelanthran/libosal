
/* ************************************************************
 * GOAL: Allow for minimal thread usage in multithreaded applications
 * 
 * As I don't like using any design even slightly complicated when it
 * comes to threads, the API provided is the bare minimum needed to
 * implement a multi-producer/mult-consumer queue.
 * 
 * Such a pattern allows very high-confidence multi-threaded applications:
 * 
 *  Each producer creates an object, writes the pointer to it into the
 *  queue and moves on to creating the next object.
 *
 *  Each consumer waits on the queue, removes the next item, works with
 *  it, deletes it, and waits on the queue again.
 *
 * This really is a fearless concurrency pattern than can be used in
 * any language (not just over-hyped ones). There is no risk of races.
 * 
 * Another pattern that this library is intended to support is pre-
 * created object pools. This allows large and expensive objects to all
 * be created once at program startup, stored in a pool and handed out
 * to any caller who needs a new object of that instance.
 * 
 * This requires semaphore support, so I expect to implement the
 * wrappers for semaphores once I get to the point where I need the pool
 * for some project or another.
 * 
 */
#ifndef H_OSAL_THREAD
#define H_OSAL_THREAD

#ifdef PLATFORM_Windows
#include <windows.h>
typedef HANDLE osal_thread_t;
typedef HANDLE osal_mutex_t;
#else
#include <pthread.h>
typedef pthread_t osal_thread_t;
typedef pthread_mutex_t osal_mutex_t;
#endif

typedef void (osal_thread_func_t) (void *);

#ifdef __cplusplus
extern "C" {
#endif

   // Start a new thread, thread is started running.
   bool osal_thread_new (osal_thread_t *thandle, osal_thread_func_t *fptr, void *param);

   // Wait for the specified threads to complete execution. Thread handles are specified
   // as an array and nthreads specifies the length of the array
   bool osal_thread_wait (osal_thread_t *threads, size_t nthreads);

   // Causes the current thread to sleep for not less than the specified number of
   // milliseconds.
   void osal_thread_sleep (size_t micro_s);

   // Once a thread has completed (see `osal_thread_wait()` above), call this function
   // to clean up all resources held by the thread.
   void osal_thread_del (osal_thread_t *thandle);


   // Create a new mutex. Named mutexes are not supported.
   bool osal_mutex_new (osal_mutex_t *mutex);

   // Delete a mutex created with `osal_mutex_new()`. If the mutex
   // specified is currently acquired by a thread, the behaviour
   // is undefined.
   void osal_mutex_del (osal_mutex_t *mutex);

   // Acquire the mutex. While the mutex is acquired, other callers attempting to
   // acquire the same mutex will block. Only one caller will ever hold the
   // same mutex at the same time.
   bool osal_mutex_acquire (osal_mutex_t *mutex);

   // Release a mutex that was acquired.
   void osal_mutex_release (osal_mutex_t *mutex);


#ifdef __cplusplus
};
#endif


#endif


