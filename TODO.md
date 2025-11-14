# TODO Items

## Need atomic increment/decrement

Need an atomic increment/decrement so that threads can update shared counters. The atomic increment
and decrements are also important for semaphore usage.

See the sectiona titled [Atomic functions](Atomic functions) below. For the information that has
already been assembled for a common set of atomic functions between Windows and glibc.

## Atomic functions

1. Must replace `#ifdef POSIX` with `#ifdef GLIBC` or whichever way works to determine glibc.
2. Ditto for Visual Studio

### Overview
Atomic functions necessary; note that if I relax the restriction on minimum C standard, then C11 can
be used which have these functions in the standard. Maybe it might be possible to provide a C11 set
of functions and polyfill those interfaces if C99 or earlier is detected.

| Operation                 | Glibc                           | Visual Studio
|---------------------------|---------------------------------|-------------------------------
| Atomic Load               | `__atomic_load()`               | `InterlockedCompareExchange()`
| Atomic Store              | `__atomic_store()`              | `InterlockedExchange()`
| Atomic Compare & Exchange | `__atomic_compare_exchange_n()` | `InterlockedCompareExchange()`
| Atomic Increment          | `__atomic_add_fetch()`          | `InterlockedIncrement()`
| Atomic Decrement          | `__atomic_sub_fetch()`          | `InterlockedDecrement()`

### Glibc functions

```
#ifdef __GLIBC__
    // This code path is executing against glibc.
    // The atomic functions (__atomic_store, etc.) are available via GCC.
#endif
// --- Atomic Store ---
// Atomically writes 'val' to the object pointed to by 'ptr'.
void __atomic_store(
    int64_t *ptr,
    int64_t val,
    int memorder
);

// --- Atomic Compare and Exchange (Strong) ---
// Compares *ptr with *expected. If equal, sets *ptr = desired and returns true.
// If unequal, sets *expected = *ptr and returns false.
// We use 'false' for the 'weak' parameter to ensure a strong CAS operation.
_Bool __atomic_compare_exchange_n(
    int64_t *ptr,
    int64_t *expected,
    int64_t desired,
    _Bool weak,
    int success_memorder,
    int failure_memorder
);

// --- Atomic Fetch and Add (Equivalent to Increment/Decrement) ---
// Atomically adds 'val' (use 1 for increment) to *ptr and returns the NEW value.
int64_t __atomic_add_fetch(
    int64_t *ptr,
    int64_t val,
    int memorder
);

// Atomically subtracts 'val' (use 1 for decrement) from *ptr and returns the NEW value.
int64_t __atomic_sub_fetch(
    int64_t *ptr,
    int64_t val,
    int memorder
);

// Note: For increment/decrement where you need the OLD value, use:
// int64_t __atomic_fetch_add(int64_t *ptr, int64_t val, int memorder);
// int64_t __atomic_fetch_sub(int64_t *ptr, int64_t val, int memorder);
```

### Visual Studio functions

```
#ifdef PLATFORM_Windows
    // This code path is executing against visual studio.
#endif
// --- Atomic Store ---
// Atomically writes 'Exchange' to 'Destination'. Returns the ORIGINAL value.
// Note: This function's return type is often the size of the operation (LONG64).
LONG64 InterlockedExchange64(
    volatile LONG64 *Destination,
    LONG64 Exchange
);

// --- Atomic Compare and Exchange ---
// Compares *Destination with Comparand. If equal, sets *Destination = Exchange.
// Returns the ORIGINAL value of *Destination before the operation.
LONG64 InterlockedCompareExchange64(
    volatile LONG64 *Destination,
    LONG64 Exchange,
    LONG64 Comparand
);

// --- Atomic Increment ---
// Atomically adds 1 to *Addend. Returns the NEW (resulting) value.
LONG64 InterlockedIncrement64(
    volatile LONG64 *Addend
);

// --- Atomic Decrement ---
// Atomically subtracts 1 from *Addend. Returns the NEW (resulting) value.
LONG64 InterlockedDecrement64(
    volatile LONG64 *Addend
);
```


### C11 Atomic functions

```
// Note: For convenience, this uses atomic_int_least64_t as the atomic object type.
// All functions use the explicit memory order parameter.

// 1. Atomic Store
// Atomically stores 'desired' into *obj.
void atomic_store_explicit(
    volatile atomic_int_least64_t *obj,
    int64_t desired,
    memory_order order
);


// 2. Atomic Load
// Atomically loads the value from *obj and returns it.
int64_t atomic_load_explicit(
    const volatile atomic_int_least64_t *obj,
    memory_order order
);


// 3. Atomic Compare and Exchange (Strong)
// Compares *obj with *expected. If equal, sets *obj = desired and returns true.
// If unequal, sets *expected = *obj and returns false.
_Bool atomic_compare_exchange_strong_explicit(
    volatile atomic_int_least64_t *obj,
    int64_t *expected,
    int64_t desired,
    memory_order success_order,
    memory_order failure_order
);


// 4. Atomic Increment (Fetch and Add)
// Atomically adds 'arg' (use 1 for increment) to *obj and returns the OLD value.
// Note: To get the NEW value, you can use the non-standard extension 'atomic_fetch_add_explicit'
// available in GCC/Clang, or simply add 'arg' to the returned value.
int64_t atomic_fetch_add_explicit(
    volatile atomic_int_least64_t *obj,
    int64_t arg, // Use 1 for increment
    memory_order order
);


// 5. Atomic Decrement (Fetch and Subtract)
// Atomically subtracts 'arg' (use 1 for decrement) from *obj and returns the OLD value.
int64_t atomic_fetch_sub_explicit(
    volatile atomic_int_least64_t *obj,
    int64_t arg, // Use 1 for decrement
    memory_order order
);
```



## Semaphores

Need semaphores for proper `ccq_t`: currently losing queue items and not sure
if the spinlocking is an issue.

With semaphores, all doubt is removed.

# Whole new redesign on the interface
Maybe a whole new redesign is needed for the interface. Currently doing the
lazy thing and typedefing the underlying semaphore/thread/mutex types.

However, this is not really good; cannot use 2-phase commit to destroy/delete
a mutex or semaphore. This results in eventually deleting a mutex which
*might* be locked by a thread.  No point in having a safe wrapper if it is not
safe.

