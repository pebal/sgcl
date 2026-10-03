[sgcl](../../README.md) › [core](../README.md) › [thread](../thread.md)

# sgcl::thread::native_handle

```cpp
native_handle_type native_handle() noexcept;
```

The handle of the thread on the platform, `std::thread::native_handle()`: a `pthread_t` on Linux and macOS, a
`HANDLE` on Windows. It is for the calls of the platform the class has no member for, a thread's name or its
priority.

## Parameters

None.

## Return value

The platform's handle of the thread.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <pthread.h>

using namespace sgcl;

int main() {
    thread worker([] { this_thread::sleep_for(std::chrono::milliseconds(10)); });
    pthread_t handle = worker.native_handle();
    println("{}", pthread_equal(handle, pthread_self()) != 0);
    worker.join();
}
```

Output:

```text
false
```

## See also

- [get_id](get_id.md): the id of the thread
- [sgcl::thread](../thread.md)
