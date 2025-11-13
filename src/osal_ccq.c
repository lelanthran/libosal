#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include "osal_ccq.h"
#include "osal_thread.h"
#include "osal_timer.h"

#undef USE_MUTEX
#define USE_FASTLOCK 1

// #define USE_MUTEX 1
// #undef USE_FASTLOCK

struct message_t {
   void *message;
   uint64_t nq_time;
};

struct osal_ccq_t {
   struct message_t *array;
   size_t array_len;
   size_t index_insert;
   size_t index_retrieve;

   osal_semaphore_t semaphore;

#ifdef USE_MUTEX
   osal_mutex_t mutex;
#endif

#ifdef USE_FASTLOCK
   uint64_t mutex;
#endif
};


#ifdef USE_MUTEX
#  define ACQUIRE_LOCK(ptr,retry,interval)      (osal_mutex_acquire_retry (ptr,retry,interval))
#  define RELEASE_LOCK(ptr)                     (osal_mutex_release (ptr))
#endif

#ifdef USE_FASTLOCK
#  define ACQUIRE_LOCK(ptr,retry,interval)      (osal_fastlock_acquire_try (ptr, "nq"))
#  define RELEASE_LOCK(ptr)                     (osal_fastlock_release (ptr, "dq"), true)
#endif


void osal_ccq_dump (osal_ccq_t *ccq)
{
   if (!ccq) {
      fprintf (stdout, "NULL ccq_t object\n");
      return;
   }

#ifdef USE_FASTLOCK
   fprintf (stdout, "register %" PRIu64 "\n", ccq->mutex);
#endif

#ifdef USE_MUTEX
   fprintf (stdout, "using mutex, no count\n");
#endif

}

osal_ccq_t *osal_ccq_new (size_t nelements)
{
   bool error = true;
   osal_ccq_t *ret = calloc (1, sizeof *ret);
   if (!ret)
      goto cleanup;

   if (!(osal_semaphore_new (&ret->semaphore, 0)))
      goto cleanup;

#ifdef USE_MUTEX
   if (!(osal_mutex_new (&ret->mutex)))
      goto cleanup;
#endif
#ifdef USE_FASTLOCK
   ret->mutex = 0;
#endif

   if (!(ret->array = malloc (sizeof *ret->array * nelements)))
      goto cleanup;

   ret->array_len = nelements;
   ret->index_retrieve = (size_t)-1;

   error = false;
cleanup:
   if (error) {
      osal_ccq_del (ret);
      ret = NULL;
   }

   return ret;
}


void osal_ccq_del (osal_ccq_t *ccq)
{
   if (!ccq)
      return;

   osal_semaphore_del (&ccq->semaphore);

#ifdef USE_MUTEX
   osal_mutex_del (&ccq->mutex);
#endif

   free (ccq->array);

   free (ccq);
}

bool osal_ccq_nq (osal_ccq_t *ccq, void *message)
{
   bool ret = false;
   uint64_t now = osal_timer_since_start();
   bool acquired = false;

   if (!(ACQUIRE_LOCK (&ccq->mutex, 2, 1)))
      goto cleanup;

   acquired = true;

   /* **************************************************************
    * Tricky!
    */

   // If the insertion point matches the retrieval point, the queue
   // is full and so we have to bail
   if (ccq->index_insert == ccq->index_retrieve)
      goto cleanup;

   // Insert the message (with the time) at the insertion point. We
   // need to do this because the retrieval point might be unset
   // ((size_t)-1), and it *can* be that even when the insertion point
   // is non-zero (queue empties faster than filling).
   ccq->array[ccq->index_insert].message = message;
   ccq->array[ccq->index_insert].nq_time = now;

   // If the retrieval point is unset ((size_t)-1), then we must
   // set it to the element we just inserted.
   if (ccq->index_retrieve == (size_t)-1) {
      ccq->index_retrieve = ccq->index_insert;
   }

   // Increment the insertion point and wraparound when we exceed
   // the arrays capacity.
   if (++ccq->index_insert >= ccq->array_len) {
      ccq->index_insert = 0;
   }

   // We are done. Now the retrieval will take care of the rest of
   // the complexity.

   ret = true;
cleanup:

   if (acquired) {
      bool released = false;
      for (size_t i=0; i<1000; i++) {
         if ((RELEASE_LOCK (&ccq->mutex)) == true) {
            released = true;
            break;
         }
         // osal_thread_sleep (1);
      }
      ret = ret && released;
   }

   osal_semaphore_post (&ccq->semaphore);
   return ret;
}

static bool ccq_dq (osal_ccq_t *ccq, void **dst, uint64_t *nq_time,
                    size_t retry, size_t interval_ms)
{
   bool ret = false;
   bool acquired = false;

   if (dst) {
      *dst = NULL;
   }
   if (nq_time) {
      *nq_time = 0;
   }

   if (!(osal_semaphore_wait_retry (&ccq->semaphore, retry, interval_ms)))
      return true;

   if (!(ACQUIRE_LOCK (&ccq->mutex, retry, interval_ms))) {
      // Return the semaphore token we got, as we could not acquire the lock
      osal_semaphore_post (&ccq->semaphore);
      return true;
   }

   acquired = true;

   /* **************************************************************
    * More trickness!
    */

   // If the queue is already empty, bail.
   if (ccq->index_retrieve == (size_t)-1) {
      ret = true;
      goto cleanup;
   }

   // Populate the outbound parameters
   *dst = ccq->array[ccq->index_retrieve].message;
   if (nq_time) {
      *nq_time = ccq->array[ccq->index_retrieve].nq_time;
   }

   // Increment the retrieval point. There are two possibilities
   // after incrementing:
   //    1. We wrap around
   //    2. We have an empty queue
   if (++ccq->index_retrieve >= ccq->array_len) {  // Wraparound
      ccq->index_retrieve = 0;
   }
   if (ccq->index_retrieve == ccq->index_insert) { // Empty queue
      ccq->index_retrieve = (size_t)-1;
   }

   ret = true;
cleanup:

   if (acquired) {
      bool released = false;
      for (size_t i=0; i<1000; i++) {
         if ((RELEASE_LOCK (&ccq->mutex)) == true) {
            released = true;
            break;
         }
         // osal_thread_sleep (1);
      }
      ret = ret && released;
   }
   return ret;
}

bool osal_ccq_dq_try (osal_ccq_t *ccq, void **dst, uint64_t *nq_time)
{
   return ccq_dq (ccq, dst, nq_time, 1, 1);
}

bool osal_ccq_dq_retry (osal_ccq_t *ccq, void **dst, uint64_t *nq_time,
                        size_t retry, size_t interval_ms)
{
   return ccq_dq (ccq, dst, nq_time, retry, interval_ms);
}

size_t osal_ccq_count (osal_ccq_t *ccq)
{
   size_t ret = 0;
   bool acquired = false;

   if (!(ACQUIRE_LOCK (&ccq->mutex, 1, 1))) {
      goto cleanup;
   }

   acquired = true;
   ret = ccq->index_insert - ccq->index_retrieve;

cleanup:
   if (acquired) {
      bool released = false;
      for (size_t i=0; i<1000; i++) {
         if ((RELEASE_LOCK (&ccq->mutex)) == true) {
            released = true;
            break;
         }
         // osal_thread_sleep (1);
      }
      if (!released) {
         ret = (size_t)-1;
      }
   }
   return ret;
}

