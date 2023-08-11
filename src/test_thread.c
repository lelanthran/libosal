
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "osal_thread.h"

static osal_mutex_t mutex;
static size_t counter;

void thread_func (void *param)
{
   uint8_t self = (uint8_t)((uintptr_t)param);
   for (size_t i=0; i<100; i++) {
      while (!(osal_mutex_acquire (&mutex))) {
         printf ("Failed to acquire mutex\n");
         osal_thread_sleep (500); // milliseconds
      }
      counter++;
      osal_mutex_release (&mutex);
      printf ("%" PRIu8 ": %zu\n", self, i);
   }
}

int main (void)
{
   int ret = EXIT_FAILURE;
   osal_thread_t threads[5];

   memset (threads, 0, sizeof threads);

   if (!(osal_mutex_init (&mutex))) {
      printf ("Error initialising mutex\n");
   }


   for (size_t i=0; i<sizeof threads/sizeof threads[0]; i++) {
      // Thread created running
      if (!(osal_thread_new (&threads[i], thread_func, (void *)i))) {
         printf ("Failed to create thread [%zu]\n", i);
         goto cleanup;
      }
   }

   if (!(osal_thread_wait (threads, sizeof threads/sizeof threads[0]))) {
      printf ("One or more threads failed to signal\n");
   }

   printf ("Counter: %zu\n", counter);

   ret = EXIT_SUCCESS;
cleanup:

   for (size_t i=0; i<sizeof threads/sizeof threads[0]; i++) {
      osal_thread_wait (&threads[i], 1);
   }

   return ret;
}

