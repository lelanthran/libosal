#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include "osal_ccq.h"
#include "osal_thread.h"
#include "osal_timer.h"

struct message_t {
   void *message;
   uint64_t nq_time;
};

struct osal_ccq_t {
   struct message_t *array;
   size_t array_len;
   size_t index_insert;
   size_t index_retrieve;
   osal_mutex_t mutex;
};


osal_ccq_t *osal_ccq_new (size_t nelements)
{
   bool error = true;
   osal_ccq_t *ret = calloc (1, sizeof *ret);
   if (!ret) {
      goto cleanup;
   }

   if (!(osal_mutex_new (&ret->mutex))) {
      goto cleanup;
   }

   if (!(ret->array = malloc (sizeof *ret->array * nelements))) {
      goto cleanup;
   }

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

   osal_mutex_del (&ccq->mutex);

   free (ccq->array);

   free (ccq);
}

bool osal_ccq_nq (osal_ccq_t *ccq, void *message)
{
   bool ret = false;
   uint64_t now = osal_timer_since_start();
   if (!(osal_mutex_acquire(&ccq->mutex))) {
      goto cleanup;
   }

   /* **************************************************************
    * Tricky!
    */

   // If the insertion point matches the retrieval point, the queue
   // is full and so we have to bail.
   if (ccq->index_insert == ccq->index_retrieve) {
      goto cleanup;
   }

   // Insert the message (with the time) at the insertion point. We
   // need to do this because the retrieval point might be unset
   // ((size_t)-1), and it *can* be that even when the insertion point
   // is non-zero (queue empties faster than filling).
   ccq->array[ccq->index_insert].message = message;
   ccq->array[ccq->index_insert].nq_time = now;

   // If the retrieval point is unset ((size_t)-1), then we must
   // set it to the element we just inserted, which must be, by
   if (ccq->index_retrieve == (size_t)-1) {
      ccq->index_retrieve = ccq->index_insert;
   }

   // Increment the insertion point and wraparound when we exceed
   // the arrays capacity.
   if (++ccq->index_insert >= ccq->array_len) {
      ccq->index_insert = 0;
   }

   // We are done. Now the retrieval will take of of some details.
   ret = true;
cleanup:

   osal_mutex_release(&ccq->mutex);
   return ret;
}

bool osal_ccq_dq (osal_ccq_t *ccq, void **dst, uint64_t *nq_time)
{
   bool ret = false;

   if (!(osal_mutex_acquire(&ccq->mutex))) {
      goto cleanup;
   }

   /* **************************************************************
    * More trickness!
    */

   // If the queue is already empty, bail.
   if (ccq->index_retrieve == (size_t)-1) {
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
   osal_mutex_release(&ccq->mutex);
   return ret;
}

