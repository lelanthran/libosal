
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "osal_evt.h"
#include "osal_timer.h"

static char *lstrdup (const char *src)
{
   if (!src)
      return NULL;
   char *ret = malloc (strlen (src) + 1);
   if (!ret)
      return NULL;

   return strcpy (ret, src);
}


static size_t handled = 0;
void event_handler (uint64_t evt, void *payload, uint64_t nq_time)
{
   static uint64_t prev = 0;
   uint64_t tmp = nq_time - prev;
   double delta = osal_timer_convert_ns_to_ms (tmp);
   if (!prev) {
      delta = 0.0;
   }
   prev = nq_time;

   handled++;

   if (evt >= 1) {
      printf ("Event %" PRIu64 ": [%s]: %fms\n", evt, (const char *)payload, delta);
      free (payload);
      return;
   }
   printf ("Unknown event %" PRIu64 "\n", evt);
}

int main (void)
{
   int ret = EXIT_FAILURE;

   // Start 30 threads with a 1M queue length
   if (!(osal_evt_startup (30, 1024 * 1024))) {
      printf ("Failed to initialise event module\n");
      goto cleanup;
   }

   for (uint8_t i=1; i<=10; i++) {
      bool registered = osal_evt_register (i, event_handler);
      printf ("Registered %" PRIi8 ": %s\n", i, registered ? "true" : "false");
   }

   for (uint8_t i=1; i<25; i++) {
      char tmp[] = "Message 1: 0xXXXX--";
      snprintf (tmp, sizeof tmp, "Message 1: 0x%04" PRIx8, i);
      char *payload = lstrdup (tmp);
      bool generated = osal_evt_generate (i, payload);
      printf ("generated [%s]: %s\n", payload, generated ? "true" : "false");
      if (!generated) {
         free (payload);
      }
   }

   for (uint8_t i=2; i<8; i++) {
      bool deregistered = osal_evt_deregister (i, event_handler);
      printf ("Deregistered %" PRIi8 ":%s\n", i, deregistered ? "true" : "false");
   }

   for (uint32_t i=1; i<25000; i++) {
      char tmp[] = "Message 1: 0xXXXXXXXX--";
      snprintf (tmp, sizeof tmp, "Message 2: 0x%08" PRIx16, (i % 10));
      char *payload = lstrdup (tmp);
      bool generated = osal_evt_generate ((i % 10), payload);
      printf ("generated [%s]: %s\n", payload, generated ? "true" : "false");
      if (!generated) {
         free (payload);
      }
   }

   ret = EXIT_SUCCESS;

cleanup:
   osal_evt_shutdown ();
   printf ("Handled %zu events\n", handled);
   return ret;
}

