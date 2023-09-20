# libosal
A lightweight and minimal OS abstraction library.

I don't recommend anyone use this yet.

I'm creating this library purely because I want a central place to put
all my abstractions for the OS (\*nix and Windows) in the same place.

I also want this to be reasonably performant, so each function in each
module is as lightweight as possible. This means that they are merely
simple wrappers.

For threads, for example, pthreads on Windows performed a lot worse than
the Linux test. Win32 `_beginthreadex()` and Win32 `CreateMutex()` are
and Win32 `Sleep()` all have a much higher overhead than the equivalents
on Linux, but are much better than the POSIX equivalents on Windows.


