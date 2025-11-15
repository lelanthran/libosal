
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "osal_thread.h"
#include "osal_timer.h"

/* **********************************************************************
 * On Windows this takes about ten minutes to execute, compiled Release
 * On Linux (same machine, as a guest VM), this executes in about 20s.
 *
 * I suspect the reason for the difference is the printf output.
 */
static osal_mutex_t mutex;
static volatile size_t counter;
static const size_t addloop = 1000 * 10;

void thread_func (void *param)
{
   size_t self = (size_t)((uintptr_t)param);
   for (size_t i=0; i<addloop; i++) {
      while (!(osal_mutex_acquire_try (&mutex))) {
         // printf ("Failed to acquire mutex [%zu]:%zu\n", self, i);
         osal_thread_sleep_ms (1); // milliseconds, slows things down.
      }
      counter++;
      while (!(osal_mutex_release (&mutex))) {
         printf ("Failed to release mutex [%zu]:%zu\n", self, i);
         osal_thread_sleep_ms (1);
      }
   }
}

int main (void)
{
   int ret = EXIT_FAILURE;
   size_t completed = 0;
   static osal_thread_t threads[500];
   size_t nthreads = sizeof threads/sizeof threads[0];

   osal_timer_init ();
   osal_timer_mark_ns ();
   memset (threads, 0, sizeof threads);

   if (!(osal_mutex_new (&mutex))) {
      printf ("Error initialising mutex\n");
   }


   for (size_t i=0; i<sizeof threads/sizeof threads[0]; i++) {
      // Thread created running
      if (!(osal_thread_new (&threads[i], thread_func, (void *)i))) {
         printf ("Failed to create thread [%zu]\n", i);
         goto cleanup;
      }
      printf ("Created thread :%zu:%zu\n", (size_t)(threads[i]), i);
   }

   completed = 0;
   printf ("Start thread wait: %fs\n", osal_timer_convert_ns_to_s (osal_timer_mark_ns ()));
   while (completed < nthreads) {
      completed = osal_thread_wait_retry (&threads[completed], nthreads, 10, 1000);
   }
   printf ("End thread wait: %fs\n", osal_timer_convert_ns_to_s (osal_timer_mark_ns ()));

   size_t expected = addloop * (sizeof threads / sizeof threads[0]);
   printf ("Counter: %zu\n", counter);
   if (counter != expected) {
      printf ("Failed: expected %zu, got %zu\n", expected, counter);
   } else {
      printf ("Passed: expected %zu, got %zu\n", expected, counter);
   }

   printf ("Loops executed in %fs\n", osal_timer_convert_ns_to_s (osal_timer_mark_ns ()));

   ret = EXIT_SUCCESS;
cleanup:
   while (completed != nthreads) {
      completed = osal_thread_wait_retry (&threads[completed], nthreads - completed, 10, 1000);
   }

   osal_mutex_del (&mutex);

   printf ("\n");
   return ret;
}

