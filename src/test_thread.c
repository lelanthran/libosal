/* ********************************************************
 * Copyright ©2023 Lelanthran Manickum, All rights reserved
 * This project  is licensed under the GPLv3.  See the file
 * LICENSE for more information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "osal_thread.h"

/* **********************************************************************
 * On Windows this takes about ten minutes to execute, compiled Release
 * On Linux (same machine, as a guest VM), this executes in about 20s.
 *
 * I suspect the reason for the difference is the printf output.
 */
static osal_mutex_t mutex;
static size_t counter;
static const size_t addloop = 1000 * 10;

void thread_func (void *param)
{
   size_t self = (size_t)((uintptr_t)param);
   for (size_t i=0; i<addloop; i++) {
      while (!(osal_mutex_acquire (&mutex))) {
         printf ("Failed to acquire mutex [%zu]:%zu\n", self, i);
         osal_thread_sleep (1); // milliseconds, slows things down.
      }
      counter++;
      while (!(osal_mutex_release (&mutex))) {
         printf ("Failed to release mutex [%zu]:%zu\n", self, i);
         osal_thread_sleep (1);
      }

      printf ("%zu: %zu\n", self, i);
   }
}

int main (void)
{
   int ret = EXIT_FAILURE;
   static osal_thread_t threads[500];

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

   if (!(osal_thread_wait (threads, sizeof threads/sizeof threads[0]))) {
      printf ("One or more threads failed to signal\n");
   }

   size_t expected = addloop * (sizeof threads / sizeof threads[0]);
   printf ("Counter: %zu\n", counter);
   if (counter != expected) {
      printf ("Failed: expected %zu, got %zu\n", expected, counter);
   } else {
      printf ("Passed: expected %zu, got %zu\n", expected, counter);
   }



   ret = EXIT_SUCCESS;
cleanup:

   for (size_t i=0; i<sizeof threads/sizeof threads[0]; i++) {
      osal_thread_wait (&threads[i], 1);
      osal_thread_del (&threads[i]);
   }

   osal_mutex_del (&mutex);

   return ret;
}

