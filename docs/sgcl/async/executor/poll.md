[sgcl](../../README.md) › [async](../README.md) › [executor](README.md)

# sgcl::async::executor::poll

```cpp
size_t poll() noexcept;
```

Runs, on the calling thread, the frames that were queued when it was called, each to its next suspension, and
returns. A frame queued meanwhile, a task that yielded included, waits for the next call, so a loop that calls
`poll()` once per frame of its own is never held by a task that keeps yielding. With nothing queued it does nothing
and does not wait.

## Parameters

None.

## Return value

The number of frames resumed.

## Complexity

Linear in the number of frames queued at the call.

## Exceptions

None: what a task throws stays in the task, for whoever waits for it.

## Notes

For a loop that is not the library's: a `CFRunLoop`, a `GMainLoop`, a game's frame, which calls `poll()` when it
has a moment. The executor is run by one thread at a time: a `poll` during a [run](run.md) is asserted on in debug
builds.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> animate(string name, int steps) {
    for (int i : range(steps)) {
        println("  {} step {}", name, i);
        co_await async::yield();  // to the next frame
    }
}

int main() {
    async::executor ui;
    ui.go(animate("fade", 2));
    ui.go(animate("slide", 3));
    for (int frame : range(5)) {
        println("frame {}", frame);
        println("  {} resumed", ui.poll());
    }
}
```

Output:

```text
frame 0
  fade step 0
  slide step 0
  2 resumed
frame 1
  fade step 1
  slide step 1
  2 resumed
frame 2
  slide step 2
  2 resumed
frame 3
  1 resumed
frame 4
  0 resumed
```

## See also

- [run](run.md): the loop of the executor's own
- [running](running.md): whether a run or a poll is in progress
- [sgcl::async::executor](README.md)
