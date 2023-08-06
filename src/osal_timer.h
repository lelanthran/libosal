
#ifndef H_OSAL_TIMER
#define H_OSAL_TIMER


typedef struct osal_timer_t osal_timer_t;

// Convenience macros to convert to/from microseconds
#define osal_timer_convert_us_to_s(x)\
   (double)((double)x / 1000000.0)
#define osal_timer_convert_us_to_ms(x)\
   (double)((double)x * 1000.0)
#define osal_timer_convert_s_to_us(x)\
   (uint64_t)((uint64_t)x * (uint64_t)1000000ULL)
#define osal_timer_convert_ms_to_us(x)\
   (uint64_t)((uint64_t)x * (uint64_t)1000ULL)

#ifdef __cplusplus
extern "C" {
#endif

   // Initialise all the timer structures and values.
   void osal_timer_init (void);

   // Returns the number of microseconds that have elapsed since this
   // function was last called. On the first invocation it returns
   // the number of microseconds since osal_timer_init() was called.
   uint64_t osal_timer_mark_us (void);

   // Returns the number of microseconds since the program init().
   uint64_t osal_timer_since_start (void);

   // Sets a timer for expiry in the future, in ns. The caller must
   // use osal_timer_expired() to determine if the timer has expired.
   // Use reset() for reusing an existing timer, or set() for
   // allocating a new timer
   osal_timer_t *osal_timer_set (uint64_t micros);
   bool osal_timer_reset (osal_timer_t *xt, uint64_t micros);

   // Check if a timer expired. Timer must have been set with
   // osal_timer_set() above.
   bool osal_timer_expired (osal_timer_t *xt);

   // Cancel and delete a timer.
   void osal_timer_del (osal_timer_t *xt);

#ifdef __cplusplus
};
#endif

#endif


