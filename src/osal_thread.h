
#ifndef H_OSAL_THREAD
#define H_OSAL_THREAD

typedef uint64_t osal_thread_t;
typedef uint64_t osal_mutex_t;
typedef void (osal_thread_func_t) (void *);

#ifdef __cplusplus
extern "C" {
#endif

   bool osal_thread_new (osal_thread_t *thandle, osal_thread_func_t *fptr, void *param);
   bool osal_thread_wait (osal_thread_t *threads, size_t nthreads);
   void osal_thread_sleep (size_t micro_s);

   bool osal_mutex_init (osal_mutex_t *mutex);
   bool osal_mutex_acquire (osal_mutex_t *mutex);
   void osal_mutex_release (osal_mutex_t *mutex);


#ifdef __cplusplus
};
#endif


#endif


