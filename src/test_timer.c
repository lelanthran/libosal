
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

   printf ("Testing osal_timer functionality\n");
   osal_timer_init ();

   uint64_t resolution = osal_timer_resolution ();
   printf ("Resolution in ns: %" PRIu64 "\n", resolution);

   printf ("Starting OS timer cost test. The display will not be updated\n");
   osal_timer_since_start ();
   uint64_t cost = osal_timer_since_start ();
   for (uint64_t i=0; i < 0x0fffffff; i++) {
      mark = osal_timer_mark_ns ();
   }
   mark = osal_timer_since_start () - cost;
   printf ("OS timer cost test: %.2fs\n", osal_timer_convert_ns_to_s (mark));
   printf ("Each timer call cost %.5fns\n", (double)0x0fffffff / (double)mark);

   printf ("Starting OS timer duplicate test. The display will not be updated\n");
   uint64_t prev_mark = osal_timer_since_start ();
   uint64_t duplicates = 0;
   uint64_t counter = 0;
   uint64_t resets = 0;
   for (uint64_t i=0; i < 0x0fffffff; i++) {
      mark = osal_timer_mark_ns ();
      counter++;
      if (mark == prev_mark) {
         duplicates++;
      } else {
         prev_mark = mark;
         resets++;
      }
   }
   printf ("Duplicates test: %" PRIu64 " duplicates found in %" PRIu64 " calls\n"
           "(rate of duplicates: %.3f)\n"
           "(rate of resets : %.3f)\n"
           "(count of resets : %" PRIu64 ")\n",
           duplicates, counter,
           (double)((double)duplicates/(double)counter),
           (double)((double)resets/(double)counter),
           resets);

   printf ("starting at: %" PRIu64 "\n ", osal_timer_since_start ());
   osal_timer_mark_ns ();
   for (size_t i=0; i<1024; i++) {
      printf ("\b%c", paddle[i % (sizeof paddle)]);
   }

   mark = osal_timer_mark_ns ();
   printf ("\nLoop duration:  %" PRIu64 "ns (%.2fs)\n",
            mark, osal_timer_convert_ns_to_s (mark));
   printf ("ending at: %" PRIu64 " \n", osal_timer_since_start ());


   printf ("Starting expired test (15 seconds)\n ");
   osal_timer_t *t1 = osal_timer_set (osal_timer_convert_s_to_ns (15));
   printf ("Mark at: %" PRIu64 " \n", osal_timer_since_start ());
   osal_timer_mark_ns ();
   while (!osal_timer_expired (t1)) {
      //printf ("\b%c", paddle[i++ % (sizeof paddle)]);
      putchar ('.');
      fflush (stdout);
      spinwait (osal_timer_convert_ms_to_ns (250));
   }
   mark = osal_timer_mark_ns ();
   printf ("\nLoop duration:  %" PRIu64 "ns (%.2fs)\n", mark, osal_timer_convert_ns_to_s (mark));

   printf ("Testing resets (reset timer for 3 seconds)\n");
   printf ("Mark at: %" PRIu64 " \n", osal_timer_since_start ());

   osal_timer_reset (t1, osal_timer_convert_s_to_ns (3));

   printf ("Mark at: %" PRIu64 " \n", osal_timer_since_start ());
   osal_timer_mark_ns ();
   while (!osal_timer_expired (t1)) {
      // printf ("\b%c", paddle[i++ % (sizeof paddle)]);
      putchar ('.');
      fflush (stdout);
      spinwait (osal_timer_convert_ms_to_ns (250));
   }
   mark = osal_timer_mark_ns ();
   printf ("\nLoop duration:  %" PRIu64 "ns (%.2fs)\n", mark, osal_timer_convert_ns_to_s (mark));
   printf ("Mark at: %" PRIu64 " \n", osal_timer_since_start ());

   osal_timer_del (t1);
   return EXIT_SUCCESS;
}
