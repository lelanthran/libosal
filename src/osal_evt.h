
/* **************************************************************************
 * Sometimes you need to write spaghetti-code; a conditional within a loop
 * inside a subroutine of a function deep within a module in some library
 * needs to alert a different function in a different library that _SOMETHING
 * * HAPPENED!!!_
 *
 * At the time of compilation it may not even be known to a specific function
 * which other function in which other compilation unit needs to handle a
 * specific condition. In these cases the options are to pass around tramp
 * data or construct a spaghetti-like system of imports and includes to call
 * the correct destination function.
 *
 * In a game, a bullet collision should shake a character sprite but a meteor
 * collision should shake the whole screen.
 *
 * In a chatbox, a good HTTP response can be handled locally and the results
 * returned, but a lookup failure should be handled at the top level so a
 * monitor program can alert the user.
 *
 * In a calendar application, a notification failure has to be handled both at
 * the point of failure _AND_ by notifying the user OOB of the usual comms
 * channel.
 *
 * The point is, if you're _GOING_ to have spaghetti code, it may as well be
 * in a spaghetti-framework. This is what an event bus is intended for.
 *
 * This module implements a very simple in-process RAM-only event bus.
 *
 * A user-specified number of threads processes all events, so event handlers
 * should be as lightweight and as quick as possible. One good pattern is to
 * simply re-queue selected events on a different queue which is consumed by a
 * different thread, and process quick events within the registered handler.
 *
 * For example, time-consuming event handlers could be re-queued on a thread
 * to handle time-consuming events while quick events (update a variable, etc)
 * can be handled directly in the handler supplied to `osal_evt_register()`.
 */

#ifndef H_EVT
#define H_EVT

#define OSAL_EVT_INVALID      ((uint64_t)-1)

typedef void (osal_evt_handler_func_t) (uint64_t evt, void *payload, uint64_t nq_time);

#ifdef __cplusplus
extern "C" {
#endif

   // Start and stop the event bus. When starting it the caller must specify
   // how many threads must service the event bus and the maximum queue
   // length to use (once the queue is full the `_evt_generate()` function
   // returns false and the caller must handle the failure appropriately).
   //
   // As the global composite structures are not protected with locks (only
   // the elements/fields within those structures are protected with locks),
   // be very careful to ensure that no code is calling `_evt_generate()` or
   // `_evt_register()` when `_evt_startup()` is called.
   //
   // The shutdown function `_evt_shutdown()` is safe to call at any time,
   // even with a full queue, as the shutdown process waits for the queue
   // to empty and wait for all outstanding events to be handled before
   // returning.
   bool osal_evt_startup (size_t nthreads, size_t qlength);
   void osal_evt_shutdown (void);

   // Register a new event using the specified function as the handler.
   // Multiple handlers can be registered for the same event but the
   // order of execution is indeterminate.
   bool osal_evt_register (uint64_t evt, osal_evt_handler_func_t *fptr);

   // Deregister all functions for the specified event if the function
   // matches the specified fptr.
   bool osal_evt_deregister (uint64_t evt, osal_evt_handler_func_t *fptr);

   // Generate a new event. All handlers registered for `evt` will be run
   // in an indeterminate order.
   //
   //    **NOTE**: If `false` is returned, the caller *must* free payload,
   // because the event will not be delivered and the handler will not get
   // the opportunity to free the payload.
   //
   //    If `true` is returned then the event has been queued and will
   // eventually be handled by the event handler.
   bool osal_evt_generate (uint64_t evt, void *payload);

#ifdef __cplusplus
};
#endif


#endif


