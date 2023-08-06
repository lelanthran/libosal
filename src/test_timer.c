
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <stdbool.h>
#include <stdint.h>

#include "osal_timer.h"

void spinwait (uint64_t us)
{
   osal_timer_t *tmp = NULL;

   tmp = osal_timer_set (us);
   while (!osal_timer_expired (tmp))
      ;

   if (tmp) {
      osal_timer_del (tmp);
   }
}

int main (void)
{
   const char paddle[] = "-\\|/";
   uint64_t mark = 0;

   printf ("osal v%s: Testing osal_timer functionality\n", osal_version);
   osal_timer_init ();

   printf ("starting at: %" PRIu64 "\n ", osal_timer_since_start ());
   osal_timer_mark_us ();
   for (size_t i=0; i<1024; i++) {
      printf ("\b%c", paddle[i % (sizeof paddle)]);
   }

   mark = osal_timer_mark_us ();
   printf ("\nLoop duration:  %" PRIu64 "uS (%.2fs)\n", mark, osal_timer_convert_us_to_s (mark));
   printf ("ending at: %" PRIu64 " \n", osal_timer_since_start ());


   printf ("Starting expired test (15 seconds)\n ");
   osal_timer_t *t1 = osal_timer_set (osal_timer_convert_s_to_us (15));
   printf ("Mark at: %" PRIu64 " \n", osal_timer_since_start ());
   osal_timer_mark_us ();
   while (!osal_timer_expired (t1)) {
      //printf ("\b%c", paddle[i++ % (sizeof paddle)]);
      putchar ('.');
      fflush (stdout);
      spinwait (osal_timer_convert_ms_to_us (250));
   }
   mark = osal_timer_mark_us ();
   printf ("\nLoop duration:  %" PRIu64 "uS (%.2fs)\n", mark, osal_timer_convert_us_to_s (mark));

   printf ("Testing resets (reset timer for 3 seconds)\n");
   printf ("Mark at: %" PRIu64 " \n", osal_timer_since_start ());

   osal_timer_reset (t1, osal_timer_convert_s_to_us (3));

   printf ("Mark at: %" PRIu64 " \n", osal_timer_since_start ());
   osal_timer_mark_us ();
   while (!osal_timer_expired (t1)) {
      // printf ("\b%c", paddle[i++ % (sizeof paddle)]);
      putchar ('.');
      fflush (stdout);
      spinwait (osal_timer_convert_ms_to_us (250));
   }
   mark = osal_timer_mark_us ();
   printf ("\nLoop duration:  %" PRIu64 "uS (%.2fs)\n", mark, osal_timer_convert_us_to_s (mark));
   printf ("Mark at: %" PRIu64 " \n", osal_timer_since_start ());

   osal_timer_del (t1);
   return EXIT_SUCCESS;
}
