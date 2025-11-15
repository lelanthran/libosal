
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
// #include <inttypes.h>
#include <stdio.h>

#include "osal_ccq.h"
#include "osal_evt.h"
#include "osal_thread.h"

static volatile uint64_t g_fastlock;

static bool lock_acquire (size_t timeout_ms)
{
   return osal_fastlock_acquire_retry (&g_fastlock, "", 5, timeout_ms);
}

static bool lock_release (void)
{
   osal_fastlock_release (&g_fastlock, "");
   return true;
}



// Data structure to store multiple handlers per event id. This cannot
// be a linear data structure, because handlers are sometimes removed.
//
// A better implementation, in terms of performance, is to have a flag
// field that has a bit for enabled/disabled and use a linear array.
//
// The problem with that is that the array would be reallocated each
// time a handler is added, which would invalidated pointers to the
// existing handler functions that are already working in the context
// of the event_loop.
//
// For now, a linked list is the only viable structure.
struct handler_ll_t {
   osal_evt_handler_func_t *handler_fptr;
   struct handler_ll_t *next;
};

static struct handler_ll_t *handler_append (struct handler_ll_t *first,
                                            osal_evt_handler_func_t *handler_fptr)
{
   struct handler_ll_t *new = malloc (sizeof *new);
   if (!new) {
      return NULL;
   }
   struct handler_ll_t *tmp = first;
   while (tmp && tmp->next) {
      tmp = tmp->next;
   }
   if (tmp) {
      tmp->next = new;
   }
   if (!first)
      first = new;

   new->handler_fptr = handler_fptr;
   new->next = NULL;
   return first;
}

static struct handler_ll_t *handler_copy (const struct handler_ll_t *src)
{
   struct handler_ll_t *ret = NULL;

   if (!src ||  !(ret = malloc (sizeof *ret)))
      return NULL;

   ret->handler_fptr = src->handler_fptr;
   ret->next = handler_copy (src->next);
   return ret;
}


static void handler_remove (struct handler_ll_t *first,
                            osal_evt_handler_func_t *handler_fptr)
{
   if (!first)
      return;

   struct handler_ll_t *tmp = first;
   while (tmp && tmp->next && tmp->next->handler_fptr != handler_fptr)
      tmp = tmp->next;

   if (!tmp || !tmp->next)
      return;

   struct handler_ll_t *next = tmp->next;
   struct handler_ll_t *nextnext = tmp->next->next;
   free (next);
   tmp->next = nextnext;
}

static void handler_free (struct handler_ll_t *first)
{
   if (!first)
      return;
   struct handler_ll_t *next = first->next;
   free (first);
   handler_free (next);
}

struct handler_t {
   uint64_t              evt;
   struct handler_ll_t  *handlers;
   osal_evt_free_func_t *free_fptr;
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
   g_handlers[g_nhandlers].handlers = NULL;
   g_handlers[g_nhandlers].free_fptr = NULL;

   g_nhandlers = newsize;
   return true;
}

/* **************************************************************************
 * Add the specified handler for the specified event, expanding the array if
 * necessary.
 */
static bool handlers_add (uint64_t evt, osal_evt_handler_func_t *handler_fptr)
{
   bool error = true;

   if (!(lock_acquire (25)))
      return false;

   for (size_t i=0; i<g_nhandlers; i++) {
      if (g_handlers[i].evt == evt) {
         g_handlers[i].handlers = handler_append (g_handlers[i].handlers, handler_fptr);
         error = false;
         goto cleanup;
      }
   }

   if (!(handlers_inc ()))
      goto cleanup;

   g_handlers[g_nhandlers - 1].evt = evt;
   g_handlers[g_nhandlers - 1].handlers = handler_append (NULL, handler_fptr);
   g_handlers[g_nhandlers - 1].free_fptr = NULL;

   error = false;
cleanup:
   lock_release ();
   return !error;
}

/* **************************************************************************
 * Remove all handler entries that match evt and handler_fptr.
 */
static bool handlers_remove (uint64_t evt, osal_evt_handler_func_t *handler_fptr)
{
   if (!(lock_acquire (500)))
      return false;

   for (size_t i=0; i<g_nhandlers; i++) {
      if (g_handlers[i].evt == evt) {
         handler_remove (g_handlers[i].handlers, handler_fptr);
      }
   }
   return lock_release ();
}




static osal_ccq_t *g_ccq;
static osal_thread_t *g_threads;
static size_t g_nthreads;
static volatile uint64_t g_complete;



/* **************************************************************************
 * The main event loop function. This waits for pointers to a event_t struct
 * on g_ccq and executes the function with the caller-provided parameter.
 */
struct event_t {
   uint64_t                 evt_id;
   struct handler_ll_t     *handlers;
   osal_evt_free_func_t    *free_fptr;
   void                    *payload;
};

static void event_loop (void *param)
{
   (void)param; // We do not use the parameter
   struct event_t *evt = NULL;
   uint64_t nq_time_us = 0;

   while (1) {
      uint64_t complete = osal_atomic_load (&g_complete);
      if (complete)
         break;

      if (!(osal_ccq_dq_retry (g_ccq, (void **)&evt, &nq_time_us, 5, 10))) {
         continue;
      }
      if (!evt)
         continue;

      struct handler_ll_t *first = evt->handlers;
      while (first) {
         first->handler_fptr (evt->evt_id, evt->payload, nq_time_us);
         first = first->next;
      }
      if (evt->free_fptr) {
         evt->free_fptr (evt->payload);
      }
      handler_free (evt->handlers);
      free (evt);
   }

   while (1) {
      if (!(osal_ccq_dq_retry (g_ccq, (void **)&evt, &nq_time_us, 5, 10))) {
         break;
      }
      if (!evt && !nq_time_us)
         break;

      if (!evt)
         continue;

      struct handler_ll_t *first = evt->handlers;
      while (first) {
         first->handler_fptr (evt->evt_id, evt->payload, nq_time_us);
         first = first->next;
      }
      if (evt->free_fptr) {
         evt->free_fptr (evt->payload);
      }
      handler_free (evt->handlers);
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
   while (!(lock_acquire (5000)))
      ;

   osal_atomic_store (&g_complete, 1);

   size_t completed = 0;

   while ((completed = osal_thread_wait_retry (g_threads, g_nthreads, 10, 1000)) != g_nthreads) {
      // TODO: How do we handle this without passing it to a caller?
   }

   for (size_t i=0; i<g_nhandlers; i++) {
      handler_free (g_handlers[i].handlers);
   }

   free (g_handlers);
   g_handlers = NULL;
   g_nhandlers = 0;

   free (g_threads);
   g_threads = NULL;
   g_nthreads = 0;

   osal_ccq_del (g_ccq);
   g_ccq = NULL;
   lock_release ();
}

bool osal_evt_register_free (uint64_t evt, osal_evt_free_func_t *free_fptr)
{
   bool ret = false;
   uint64_t complete = osal_atomic_load (&g_complete);
   if (complete)
      return false;

   lock_acquire (500);

   for (size_t i=0; i<g_nhandlers; i++) {
      if (g_handlers[i].evt != evt)
         continue;

      g_handlers[i].free_fptr = free_fptr;
      ret = true;
      break;
   }

   lock_release ();
   return ret;
}

bool osal_register_handler (uint64_t evt, osal_evt_handler_func_t *handler_fptr)
{
   uint64_t complete = osal_atomic_load (&g_complete);
   if (complete)
      return false;
   return handlers_add (evt, handler_fptr);
}

bool osal_evt_deregister (uint64_t evt, osal_evt_handler_func_t *fptr)
{
   uint64_t complete = osal_atomic_load (&g_complete);
   if (complete)
      return false;
   return handlers_remove (evt, fptr);
}

bool osal_evt_generate (uint64_t evt, void *payload)
{
   bool ret = false;
   uint64_t complete = osal_atomic_load (&g_complete);
   if (complete)
      return false;

   while (!(lock_acquire (50)))
      ;
   for (size_t i=0; i < g_nhandlers; i++) {
      if (g_handlers[i].evt != evt)
         continue;

      struct event_t *evt = malloc (sizeof *evt);
      if (!evt) {
         goto cleanup;
      }
      evt->evt_id = g_handlers[i].evt;
      evt->handlers = handler_copy (g_handlers[i].handlers);
      evt->free_fptr = g_handlers[i].free_fptr;
      evt->payload = payload;
      if (!(osal_ccq_nq (g_ccq, evt))) {
         handler_free (evt->handlers);
         free (evt);
         goto cleanup;
      }
      ret = true;
      break;
   }

cleanup:
   lock_release ();
   return ret;
}

size_t osal_evt_queue_length (void)
{
   return osal_ccq_count (g_ccq);
}



