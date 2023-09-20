
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
    * false on error.
    */
   bool osal_ccq_nq (osal_ccq_t *ccq, void *message);

   /* Retrieves a message from the queue. Returns true on
    * success and false if there are no messages. Errors are
    * not possible.
    *
    * The message is placed in dst, the time that the message
    * was added to the queue is placed in 'nq_time'. See
    * osal_timer_since_start() for more information on the
    * time value that is returned.
    *
    * If nq_time is NULL, it is ignored. The parameter dst
    * must point to a valid pointer.
    */
   bool osal_ccq_dq (osal_ccq_t *ccq, void **dst, uint64_t *nq_time);

#ifdef __cplusplus
};
#endif


#endif


