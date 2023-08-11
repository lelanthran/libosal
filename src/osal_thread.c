
#include <stdint.h>
#include <stdbool.h>

#include <process.h>

#include "osal_thread.h"

#ifdef PLATFORM_Windows
typedef unsigned int thread_return_t;
#else
typedef void *thread_return_t;
#endif

struct trunner_param_t {
   osal_thread_func_t *fptr;
   void *param;
};

static thread_return_t trunner (void *param)
{
   struct trunner_param_t *tr = param;
   tr->fptr (tr->param);
   free (tr);
#ifdef PLATFORM_Windows
   return 0;
#else
   return NULL;
#endif
}

/* ***************************************************** */

#ifdef PLATFORM_Windows

bool osal_thread_new (osal_thread_t *thandle, osal_thread_func_t *fptr, void *param)
{
   struct trunner_param_t *tr = malloc (sizeof *tr);
   if (!tr) {
      return false;
   }

   tr->fptr = fptr;
   tr->param = param;

   *thandle = _beginthreadex (NULL, 0, trunner, tr, 0, NULL);

   return *thandle != 0;
}

bool osal_thread_wait (osal_thread_t *threads, size_t nthreads)
{
   bool rc = true;
   for (size_t i=0; i<nthreads; i++) {
      DWORD e = WaitForSingleObject (threads[i], INFINITE);
      rc = rc && e == WAIT_OBJECT_0;
   }
   return rc;
}

void osal_thread_sleep (size_t micro_s)
{

}

bool osal_mutex_init (osal_mutex_t *mutex)
{
   *mutex = CreateMutex (NULL, false, NULL);
   return *mutex != NULL;
}

bool osal_mutex_acquire (osal_mutex_t *mutex)
{
   DWORD rc = WaitForSingleObject (*mutex, 1);
   return rc == WAIT_OBJECT_0;
}

void osal_mutex_release (osal_mutex_t *mutex)
{
   ReleaseMutex (*mutex);
}

#endif

/* ***************************************************** */
#ifdef PLATFORM_POSIX
#endif


