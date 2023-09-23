
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
    * messages true is returned, and *dst nq_time NULL/0.
    *
    * The message is placed in dst, the time that the message
    * was added to the queue is placed in 'nq_time'. See
    * osal_timer_since_start() for more information on the
    * time value that is returned.
    *
    * If nq_time is NULL, it is ignored. The parameter dst
    * must point to a valid pointer.
    *
    * Returns false if any errors are encountered, true if
    * no errors were encountered. A return value of true does
    * not indicate that a message was retrieved (the queue
    * may be empty).
    *
    * On a return of true, *dst will be NULL and nq_time
    * will be zero if there were no messages available.
    *
    * on a return of true, *dst will point to the message
    * retrieved from the queue and nq_time will be
    * populated with the time that the message entered the
    * queue.
    *
    * On a return of false, both *dst and nq_time are
    * invalid.
    *
    */
   bool osal_ccq_dq (osal_ccq_t *ccq, void **dst, uint64_t *nq_time);

#ifdef __cplusplus
};
#endif


#endif


