
#ifndef H_OSAL_CCQ
#define H_OSAL_CCQ

typedef struct osal_ccq_t osal_ccq_t;

#ifdef __cplusplus
extern "C" {
#endif

   void osal_ccq_dump (osal_ccq_t *ccq);

   /* Create a bounded queue of nelements. Returns NULL
    * on error or a pointer to an object of type osal
    * ccq_t on success;
    */
   osal_ccq_t *osal_ccq_new (size_t nelements);

   /* Delete an object of type osal_ccq_t, which is returned
    * from a successful call to osal_ccq_new().
    */
   void osal_ccq_del (osal_ccq_t *ccq);

   /* Place a message onto the queue, returns true on success and
    * false on error. The only error possible is failure to
    * lock.
    */
   bool osal_ccq_nq (osal_ccq_t *ccq, void *message);

   /* Retrieves a message from the queue. Returns true on
    * success and false on any error. When there are no
    * messages true is returned, and {*dst, nq_time_us} is
    * set to { NULL, 0}.
    *
    * The message is placed in dst, the time that the message
    * was added to the queue is placed in 'nq_time_us'. See
    * osal_timer_since_start() for more information on the
    * time value that is returned.
    *
    * If nq_time_us is NULL, it is ignored. The parameter dst
    * must point to a valid pointer.
    *
    * Returns false if any errors are encountered, true if
    * no errors were encountered. A return value of true does
    * not indicate that a message was retrieved (the queue
    * may be empty).
    *
    * When there are no messages available, this function
    * returns `true` and sets the message `*dst` to NULL and
    * the `nq_time_us` to zero.
    *
    * When a message is returned, this function returns true,
    * the `*dst` will container a pointer to the message and
    * `nq_time_us` will contain the duration that the message
    * spent in the queue.
    *
    * On a return of false, both `*dst` and `nq_time_us` are
    * invalid and should not be used by the caller.
    *
    */
   bool osal_ccq_dq_try (osal_ccq_t *ccq, void **dst, uint64_t *nq_time_us);

   /* Wrapper around osal_ccq_dq; this function will retry the dq operation
    * `retry` times, waiting (`interval_ms` * the retry attempt milliseconds)
    * between retries.
    */
   bool osal_ccq_dq_retry (osal_ccq_t *ccq, void **dst, uint64_t *nq_time_us,
                           size_t retry, size_t interval_ms);

   /* Returns a count of elements in the queue. Note that by the time this
    * function returns, the number of elements in the queue may have changed.
    * This function is used exclusively for logging and diagnostics.
    */
   size_t osal_ccq_count (osal_ccq_t *ccq);

#ifdef __cplusplus
};
#endif


#endif


