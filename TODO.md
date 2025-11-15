# TODO Items

## Event bus module

Must change the struct event_t to register multiple handlers for a single
event, with only a single free.

The goal is to enforce deterministic execution of handlers, in the order they
were registered in.


```
struct handler_ll_t {
   osal_evt_handler_func_t *handler_fptr;
   struct handler_ll_t *next;
}

struct event_t {
   uint64_t evt_id;
   struct handler_ll_t *handler;
   osal_evt_free_func_t *free_fptr;
}

```
