
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
// #include <inttypes.h>
// #include <stdio.h>

#include "osal_ccq.h"
#include "osal_evt.h"
#include "osal_thread.h"

static uint64_t g_futex;

static bool lock_acquire (size_t timeout_ms)
{
   for (size_t i=0; i<timeout_ms; i++) {
      if (osal_futex_acquire (&g_futex, NULL))
         return true;
      osal_thread_sleep (1);
   }
   return false;
}

static bool lock_release (size_t timeout_ms)
{
   for (size_t i=0; i<timeout_ms; i++) {
      if (osal_futex_release (&g_futex, NULL))
         return true;
      osal_thread_sleep (1);
   }
   return false;
}





struct handler_t {
   uint64_t                 evt;
   osal_evt_handler_func_t *fptr;
};

static struct handler_t *g_handlers;
static size_t g_nhandlers;

/* **************************************************************************
 * Increase the length of the array storing the event handlers
 */
static bool handlers_inc (void)
{
   // This function expects to be run while the futex is already acquired.
   size_t newsize = g_nhandlers + 1;
   struct handler_t *tmp = realloc (g_handlers, newsize * (sizeof *tmp));
   if (!tmp)
      return false;

   g_handlers = tmp;

   g_handlers[g_nhandlers].evt = OSAL_EVT_INVALID;
   g_handlers[g_nhandlers].fptr = NULL;

   g_nhandlers = newsize;
   return true;
}

/* **************************************************************************
 * Add the specified handler for the specified event, expanding the array if
 * necessary.
 */
static bool handlers_add (uint64_t evt, osal_evt_handler_func_t *fptr)
{
   bool error = true;

   if (!(lock_acquire (25)))
      return false;

   for (size_t i=0; i<g_nhandlers; i++) {
      if (!g_handlers[i].fptr && g_handlers[i].evt == OSAL_EVT_INVALID) {
         g_handlers[i].evt = evt;
         g_handlers[i].fptr = fptr;
         error = false;
         goto cleanup;
      }
   }

   if (!(handlers_inc ()))
      goto cleanup;

   g_handlers[g_nhandlers - 1].evt = evt;
   g_handlers[g_nhandlers - 1].fptr = fptr;

   error = false;
cleanup:
   lock_release (500);
   return !error;
}

/* **************************************************************************
 * Remove all handler entries that match evt and fptr.
 */
static bool handlers_remove (uint64_t evt, osal_evt_handler_func_t *fptr)
{
   if (!(lock_acquire (500)))
      return false;

   for (size_t i=0; i<g_nhandlers; i++) {
      if (g_handlers[i].evt == evt && g_handlers[i].fptr == fptr) {
         g_handlers[i].evt = OSAL_EVT_INVALID;
         g_handlers[i].fptr = NULL;
      }
   }
   return lock_release (1000);
}

/* **************************************************************************
 * Find all the events that match the specified event.
 */
static struct handler_t *handlers_evt_find (uint64_t evt)
{
   struct handler_t *ret = calloc (g_nhandlers + 1, (sizeof *ret));
   if (!ret)
      return NULL;

   if (!(lock_acquire (25))) {
      free (ret);
      return NULL;
   }

   size_t index = 0;
   for (size_t i=0; i<g_nhandlers; i++) {
      if (g_handlers[i].evt == evt) {
         ret[index].evt = g_handlers[i].evt;
         ret[index].fptr = g_handlers[i].fptr;
         index++;
      }
   }

   lock_release (500);
   return ret;
}




static osal_ccq_t *g_ccq;
static osal_thread_t *g_threads;
static size_t g_nthreads;
static volatile uint64_t g_complete;



/* **************************************************************************
 * The main event loop function. This waits for pointers to a event_t struct
 * on g_ccq and executes the function with the caller-provided parameter.
 *
 * A receipt of NULL causes the event loop to first free all remaining
 * items in the queue g_ccq and then end.
 */
struct event_t {
   uint64_t                 evt_id;
   osal_evt_handler_func_t *fptr;
   void                    *payload;
};

static bool dq_retry (osal_ccq_t *ccq, size_t n, void **dst, uint64_t *nq_time)
{
   for (size_t i=0; i<n; i++) {
      if ((osal_ccq_dq (ccq, dst, nq_time)) && *dst && *nq_time)
         return true;
      osal_thread_sleep (1);
   }
   return false;
}

static void event_loop (void *param)
{
   (void)param; // We do not use the parameter
   struct event_t *evt = NULL;
   uint64_t nq_time = 0;

   while (1) {
      uint64_t complete = osal_atomic_load (&g_complete);
      if (complete)
         break;

      if (!(dq_retry (g_ccq, 50, (void **)&evt, &nq_time))) {
         continue;
      }

      evt->fptr (evt->evt_id, evt->payload, nq_time);
      free (evt);
   }

   while (1) {
      if (!(dq_retry (g_ccq, 100, (void **)&evt, &nq_time))) {
         break;
      }

      evt->fptr (evt->evt_id, evt->payload, nq_time);
      free (evt);
   }

}


bool osal_evt_startup (size_t nthreads, size_t qlength)
{
   bool error = true;

   // If we have already initialised, return true
   if (g_ccq || g_nthreads || g_threads)
      return true;

   if (!(g_ccq = osal_ccq_new (qlength))) {
      goto cleanup;
   }

   g_nthreads = nthreads;
   if (!(g_threads = calloc (g_nthreads, sizeof *g_threads)))
      goto cleanup;

   // Make them all invalid so that shutdown can proceed even if only half of
   // them were started.
   for (size_t i=0; i<g_nthreads; i++) {
      g_threads[i] = -1;
   }

   osal_atomic_store (&g_complete, 0);
   for (size_t i=0; i<g_nthreads; i++) {
      if (!(osal_thread_new (&g_threads[i], event_loop, NULL))) {
         goto cleanup;
      }
   }

   error = false;

cleanup:
   if (error) {
      osal_evt_shutdown ();
   }

   return !error;
}

void osal_evt_shutdown (void)
{
   if (!g_ccq || !g_threads) {
      g_nthreads = 0;
      return;
   }

   // First acquire the lock
   lock_acquire (5000);

   osal_atomic_store (&g_complete, 1);

   osal_thread_wait (g_threads, g_nthreads);

   for (size_t i=0; i<g_nthreads; i++) {
      osal_thread_del (&g_threads[i]);
   }

   free (g_handlers);
   g_handlers = NULL;
   g_nhandlers = 0;

   free (g_threads);
   g_threads = NULL;
   g_nthreads = 0;

   osal_ccq_del (g_ccq);
   g_ccq = NULL;
   lock_release (10000);
}

bool osal_evt_register (uint64_t evt, osal_evt_handler_func_t *fptr)
{
   return handlers_add (evt, fptr);
}

bool osal_evt_deregister (uint64_t evt, osal_evt_handler_func_t *fptr)
{
   return handlers_remove (evt, fptr);
}

bool osal_evt_generate (uint64_t evt, void *payload)
{
   struct handler_t *matches = handlers_evt_find (evt);
   if (!matches)
      return false;

   size_t nmatches = 0;
   for (size_t i=0; matches[i].evt != OSAL_EVT_INVALID && matches[i].fptr; i++) {
      struct event_t *evt = malloc (sizeof *evt);
      if (!evt) {
         free (matches);
         return false;
      }
      evt->evt_id = matches[i].evt;
      evt->fptr = matches[i].fptr;
      evt->payload = payload;
      if (!(osal_ccq_nq (g_ccq, evt))) {
         free (evt);
         free (matches);
         return false;
      }
      nmatches++;
   }
   free (matches);
   return nmatches > 0 ? true : false;
}

size_t osal_evt_queue_length (void)
{
   return osal_ccq_count (g_ccq);
}



