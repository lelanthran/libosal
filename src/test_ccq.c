
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "osal_thread.h"
#include "osal_timer.h"
#include "osal_ccq.h"

static char *lstrdup (const char *src)
{
   size_t nbytes = strlen (src) + 1;
   char *ret = malloc (nbytes);
   if (ret) {
      strcpy (ret, src);
   }
   return ret;
}

static void consumer (void *param)
{
   osal_ccq_t *queue = param;
   printf ("[consumer] Started\n");
   char *message = NULL;
   uint64_t nq_time = (uint64_t)-1;
   uint64_t prev_time = osal_timer_since_start();
   size_t expected = 0;
   size_t msg_number = (size_t)-1;

   while (true) {
      if ((osal_ccq_dq (queue, (void **)&message, &nq_time)) == true) {

         // End the thread if NULL is returned.
         if (message == NULL) {
            break;
         }

         uint64_t duration = nq_time - prev_time;
         printf ("[consumer]: rxed [%s], spent %.2fms in queue\n",
                  message, duration/1000.0);
         if ((sscanf (message, "%zu", &msg_number)) != 1) {
            fprintf (stderr, "[consumer] Missing message number [%s]\n", message);
            break;
         }
         if (msg_number != expected) {
            fprintf (stderr, "[consumer] Expected %zu, got %zu\n",
                  expected, msg_number);
            break;
         }

         prev_time = nq_time;
         free (message);
         message = NULL;

         expected++;
      }
   }

   printf ("[consumer] Completed\n");
   free (message);
}

static void producer (void *param)
{
   osal_ccq_t *queue = param;
   printf ("[producer]: Started\n");
   char message[50];
   for (size_t i=0; i<999; i++) {
      snprintf (message, sizeof message, "%zu message", i);
      char *msg = lstrdup (message);
      while (!(osal_ccq_nq (queue, msg)))
         ;
      printf ("[producer]: txed [%s]\n", message);
   }

   osal_ccq_nq (queue, NULL);
}


int main (void)
{
   int ret = EXIT_FAILURE;
   osal_thread_t threads[2] = {0, 0};

   osal_ccq_t *queue = NULL;

   queue = osal_ccq_new (10);
   if (!queue) {
      fprintf (stderr, "Failed to create a new queue\n");
      goto cleanup;
   }

   osal_timer_init();
   if (!(osal_thread_new(&threads[0], producer, queue))) {
      fprintf (stderr, "Failed to create producer thread\n");
      goto cleanup;
   }

   if (!(osal_thread_new(&threads[1], consumer, queue))) {
      fprintf (stderr, "Failed to create consumer thread\n");
      goto cleanup;
   }


   ret = EXIT_SUCCESS;
cleanup:
   osal_thread_wait(threads, 2);
   osal_ccq_del (queue);
   return ret;
}

