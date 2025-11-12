
/* ************************************************************
 * GOAL: Allow for minimal thread usage in multithreaded applications
 *
 * As I don't like using any design even slightly complicated when it
 * comes to threads, the API provided is the bare minimum needed to
 * implement a multi-producer/multi-consumer queue.
 *
 * Such a pattern allows very high-confidence multi-threaded applications:
 *
 *  Each producer creates an object, writes the pointer to it into the
 *  queue and moves on to creating the next object.
 *
 *  Each consumer waits on the queue, removes the next item, works with
 *  it, deletes it, and waits on the queue again.
 *
 * Another pattern that this library is intended to eventually support
 * is pre- created object pools. This allows large and expensive objects
 * to all be created once at program startup, stored in a pool and
 * handed out to any caller who needs a new object of that instance.
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
typedef HANDLE osal_sem_t;
#else
#include <pthread.h>
#include <semaphore.h>
typedef pthread_t osal_thread_t;
typedef pthread_mutex_t osal_mutex_t;
typedef sem_t osal_semaphore_t;
#endif

typedef void (osal_thread_func_t) (void *);

#ifdef __cplusplus
extern "C" {
#endif



   /* ***********************************************************************
    * Thread functions
    */

   // Start a new thread, thread is started running.
   bool osal_thread_new (osal_thread_t *thandle, osal_thread_func_t *fptr,
                         void *param);

   // Wait for the specified threads to complete execution. Thread handles are
   // specified as an array and nthreads specifies the length of the array
   bool osal_thread_wait (osal_thread_t *threads, size_t nthreads);

   // Causes the current thread to sleep for not less than the specified number of
   // milliseconds.
   void osal_thread_sleep (size_t milliseconds);

   // Gets the thread id of the calling thread
   osal_thread_t osal_thread_self (void);

   // Once a thread has completed (see `osal_thread_wait()` above), call this
   // function to clean up all resources held by the thread.
   void osal_thread_del (osal_thread_t *thandle);



   /* ***********************************************************************
    * Mutex functions
    */

   // Create a new mutex. Named mutexes are not supported. The newly created
   // mutex is a recursive mutex.
   bool osal_mutex_new (osal_mutex_t *mutex);

   // Delete a mutex created with `osal_mutex_new()`. If the mutex
   // specified is currently acquired by a thread, the behaviour
   // is undefined.
   void osal_mutex_del (osal_mutex_t *mutex);

   // Acquire the mutex with blocking. If the mutex is already held this
   // call returns immediately.
   //
   // Returns true if the mutex was acquired, false if it was not.
   bool osal_mutex_acquire (osal_mutex_t *mutex);

   // Attempt to acquire the mutex. If the mutex cannot be acquired false
   // is returned. If the mutex was acquired true is returned. This function
   // always returns immediately without blocking.
   bool osal_mutex_acquire_try (osal_mutex_t *mutex);

   // A wrapper around `acquire_try`; this function attempts to acquire the
   // mutex `retry` times, sleeping `interval_ms` milliseconds between each
   // attempt.
   //
   // Returns true if the mutex was acquired and false if it was not
   // acquired.
   bool osal_mutex_acquire_retry (osal_mutex_t *mutex, size_t retry,
                                                       size_t interval_ms);

   // Release a mutex that was acquired.
   //
   // Returns true if the mutex was released, false if it was not.
   bool osal_mutex_release (osal_mutex_t *mutex);



   /* ***********************************************************************
    * Atomic operations/intrinsics
    */

   // Perform atomic read (load) and writes (store).
   uint64_t osal_atomic_load (volatile uint64_t *dst);
   void osal_atomic_store (volatile uint64_t *dst, uint64_t value);

   // Perform atomic additions and atomic subtractions. Returns the original
   // value of `dst` (value prior to the operation). The operand is either
   // added to or subtracted from `dst`, with the result stored in dst.
   uint64_t osal_atomic_add (volatile uint64_t *dst, uint64_t operand);
   uint64_t osal_atomic_sub (volatile uint64_t *dst, uint64_t operand);


   // Perform an atomic compare and exchange. Compares the value `target`
   // with the value `comparand`:
   //    equal:   Writes `newval` into target, returns true
   //    !equal:  Returns false
   bool osal_cmpxchange (volatile uint64_t *target,
                         uint64_t newval, uint64_t comparand);



   /* ***********************************************************************
    * Futex functions
    */

   // Acquire a fast mutex. A fast mutex is an in-process mutex that will
   // never cause a kernel context-switch. The target must be initialised
   // to zero before any acquisitions and releases are performed.
   //
   // Returns true if the fast mutex is acquired, false if it was not.
   bool osal_futex_acquire (uint64_t *target, const char *id);

   // Release a fast mutex. A fast mutex is an in-process mutex that will
   // never cause a kernel context-switch. The target must be initialised
   // to zero before any acquisitions and releases are performed.
   //
   // Returns true if the fast mutex was released, false if it is still
   // held.
   bool osal_futex_release (uint64_t *target, const char *id);



   /* ***********************************************************************
    * Semaphore functions (Sorry MacOS)
    */
   // Create a new semaphore. Named semaphores are not supported. The newly
   // created will be initialised with the specified `value`. Initialising a
   // semaphore that has already been initialised is an error.
   bool osal_semaphore_new (osal_semaphore_t *sem, unsigned int value);

   // Delete a semaphore created with `osal_semaphore_new()`.
   void osal_semaphore_del (osal_semaphore_t *sem);

   // Increment the sempahore. On overflow false is returned, otherwise true
   // is returned.
   bool osal_semaphore_post (osal_semaphore_t *sem);

   // These are the functions that wait for a positive semaphore count, then
   // decrement it and returns a true or false value.

   // This function blocks until the semaphore reaches a positive count.
   bool osal_semaphore_wait (osal_semaphore_t *sem);

   // This function does not block, and returns true if the semaphore was
   // positive and false if the semaphore is zero.
   bool osal_semaphore_wait_try (osal_semaphore_t *sem);

   // This is a wrapper around `_wait_try`; it returns true if the semaphore
   // was signalled and false if it was not.
   //
   // This function will perform a nonblocking `_wait_try`, and retry the
   // operation `retry` times, sleeping `interval_ms` milliseconds between
   // retry attempts.
   bool osal_semaphore_wait_retry (osal_semaphore_t *sem, size_t retry,
                                                          size_t interval_ms);

#ifdef __cplusplus
};
#endif


#endif


