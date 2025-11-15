
/* **************************************************************************
 * Sometimes you need to write spaghetti-code; a conditional within a loop
 * inside a subroutine of a function deep within a module in some library
 * needs to alert a different function in a different library that _SOMETHING
 * HAPPENED!!!_
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
 * returned to the caller for display, but a lookup failure should be handled
 * at the top level so a monitor program can alert the user.
 *
 * In an IoT application, a sensor value must be sent to different (often
 * sibling) subsystems, with some handling the value when it is within the
 * expected range and others handling different thresholds being exceeded.
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
 * can be handled directly in the handler supplied to `osal_register_handler()`.
 */

#ifndef H_OSAL_EVT
#define H_OSAL_EVT

#define OSAL_EVT_INVALID      ((uint64_t)-1)

typedef void (osal_evt_handler_func_t) (uint64_t evt, void *payload, uint64_t nq_time);
typedef void (osal_evt_free_func_t) (void *);

#ifdef __cplusplus
extern "C" {
#endif

   // Start and stop the event bus. When starting it the caller must specify
   // how many threads must service the event bus and the maximum queue
   // length to use (once the queue is full the `osal_evt_generate()` function
   // returns false and the caller must handle the failure appropriately).
   //
   // As the global composite structures are not protected with locks (only
   // the elements/fields within those structures are protected with locks),
   // be very careful to ensure that no code is calling `osal_evt_generate()` or
   // `osal_register_handler()` when `osal_evt_startup()` is called.
   //
   // The shutdown function `osal_evt_shutdown()` is safe to call at any time,
   // even with a full queue, as the shutdown process waits for the queue
   // to empty and waits for all outstanding events to be handled before
   // returning.
   bool osal_evt_startup (size_t nthreads, size_t qlength);
   void osal_evt_shutdown (void);

   // Generated events have a payload that must eventually be freed. This
   // function registers a free function to be called after all the handlers
   // are run. Note that this function returns an error if the `evt` is not
   // yet registered with `osal_register_handler()`.
   bool osal_evt_register_free (uint64_t evt, osal_evt_free_func_t *free_fptr);

   // Register a new event using the specified function as the handler.
   // Multiple handlers can be registered for the same event but the
   // order of execution is indeterminate.
   //
   // The handle_fptr function is passed the payload, and when there are
   // no more handlers, the payload is passed to free_fptr for disposal.
   // If free_fptr is NULL the payload is ignored.
   //
   // Note that OSAL_EVT_INVALID is reserved.
   bool osal_register_handler (uint64_t evt, osal_evt_handler_func_t *handler_fptr);

   // Deregister all functions for the specified event if the handler
   // function matches the specified handler_fptr.
   bool osal_evt_deregister (uint64_t evt, osal_evt_handler_func_t *handler_fptr);

   // Generate a new event. All handlers registered for `evt` will be run
   // in an indeterminate order. When there are no more handlers to run, the
   // payload is disposed off using the free_fptr function that was registered.
   //
   //    **NOTE**: If `false` is returned, the caller *must* free payload,
   // because the event will not be delivered and the handler will not get
   // the opportunity to free the payload.
   //
   //    If `true` is returned then the event has been queued and will
   // eventually be handled by the event handler, which will dispose of
   // the payload.
   bool osal_evt_generate (uint64_t evt, void *payload);

   // Return the number of events in the queue.
   size_t osal_evt_queue_length (void);

#ifdef __cplusplus
};
#endif


#endif


