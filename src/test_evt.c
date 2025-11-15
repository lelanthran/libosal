
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "osal_evt.h"
#include "osal_timer.h"
#include "osal_thread.h"

static char *lstrdup (const char *src)
{
   if (!src)
      return NULL;
   char *ret = malloc (strlen (src) + 1);
   if (!ret)
      return NULL;

   return strcpy (ret, src);
}


static volatile uint64_t g_handled = 0;
static size_t g_generated = 0;

static void free_payload (void *payload)
{
   free (payload);
}

static void event_handler (uint64_t evt, void *payload, uint64_t nq_time)
{
   static uint64_t prev = 0;
   uint64_t tmp = nq_time - prev;
   double delta = osal_timer_convert_ns_to_ms (tmp);
   if (!prev) {
      delta = 0.0;
   }
   prev = nq_time;

   osal_atomic_add (&g_handled, 1);

   size_t tid = (size_t)osal_thread_self ();
   if (evt >= 1) {
      printf ("Thread: %zu: Event %" PRIu64 ": [%s]: %fms\n", tid, evt, (const char *)payload, delta);
      // free (payload); // Handled by register_free
      return;
   }
   printf ("Unknown event %" PRIu64 "\n", evt);
}

int main (void)
{
   int ret = EXIT_FAILURE;

   osal_timer_init ();

   osal_timer_mark_ns ();

   uint64_t sub_start = osal_timer_mark_ns ();
   // Start 30 threads with a 1M queue length
   if (!(osal_evt_startup (30, 1024 * 1024))) {
      printf ("Failed to initialise event module\n");
      goto cleanup;
   }
   uint64_t sub_end = osal_timer_mark_ns ();
   printf ("Timer: Event bus started in %fms\n", osal_timer_convert_ns_to_ms (sub_end - sub_start));

   for (uint8_t i=1; i<=10; i++) {
      bool reg_handler = osal_register_handler (i, event_handler);
      bool reg_free = osal_evt_register_free (i, free_payload);
      printf ("Registered handler %" PRIi8 ": %s\n", i, reg_handler ? "true" : "false");
      printf ("Registered freefunc %" PRIi8 ": %s\n", i, reg_free ? "true" : "false");
   }

   sub_start = osal_timer_mark_ns ();
   for (uint8_t i=1; i<25; i++) {
      char tmp[] = "Message 1: 0xXXXX--";
      snprintf (tmp, sizeof tmp, "Message 1: 0x%04" PRIx8, i);
      char *payload = lstrdup (tmp);
      bool generated = osal_evt_generate (i, payload);
      printf ("generated [%s]: %s\n", payload, generated ? "true" : "false");
      if (!generated) {
         free (payload);
      } else {
         g_generated++;
      }
   }
   sub_end = osal_timer_mark_ns ();
   printf ("Timer: Generated %zu handlers in %fms\n", g_generated, osal_timer_convert_ns_to_ms (sub_end - sub_start));

   sub_start = osal_timer_mark_ns ();
   for (uint8_t i=2; i<8; i++) {
      bool deregistered = osal_evt_deregister (i, event_handler);
      printf ("Deregistered %" PRIi8 ":%s\n", i, deregistered ? "true" : "false");
   }
   sub_end = osal_timer_mark_ns ();
   printf ("Timer: Deregistered 8 handlers in %fms\n", osal_timer_convert_ns_to_ms (sub_end - sub_start));

   sub_start = osal_timer_mark_ns ();
   for (uint32_t i=1; i<(1024 * 1024); i++) {
      char tmp[] = "1234567890: Message 2: 0xXXXXXXXX--";
      snprintf (tmp, sizeof tmp, "%" PRIu32 ": Message 2: 0x%08" PRIx16, i, (i % 10));
      char *payload = lstrdup (tmp);
      bool generated = osal_evt_generate ((i % 10), payload);
      printf ("generated [%s]: %s\n", payload, generated ? "true" : "false");
      if (!generated) {
         free (payload);
      } else {
         g_generated++;
      }
   }
   sub_end = osal_timer_mark_ns ();
   printf ("Timer: Generated %zu events in %fms\n", g_generated, osal_timer_convert_ns_to_ms (sub_end - sub_start));

   size_t qlen = osal_evt_queue_length ();
   printf ("qcount = %zu\n", qlen);

   ret = EXIT_SUCCESS;

cleanup:
   sub_start = osal_timer_mark_ns ();
   osal_evt_shutdown ();
   sub_end = osal_timer_mark_ns ();
   printf ("Timer: Shutdown took %fms\n", osal_timer_convert_ns_to_ms (sub_end - sub_start));

   if (g_generated != g_handled) {
      printf ("Failed: Handled %zu of %zu events\n", g_handled, g_generated);
      ret = EXIT_FAILURE;
   } else {
      printf ("Passed: Handled %zu of %zu events\n", g_handled, g_generated);
   }

   return ret;
}

