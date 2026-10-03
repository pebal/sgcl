[sgcl](../../README.md) › [core](../README.md) › [thread](README.md)

# sgcl::thread::join

```cpp
void join();
```

Waits for the thread to finish its function. The closure in the managed node is destroyed on the thread when the
function returns, before `join` returns: what it held by value is released by then and collected later. After the
call the object stands for no thread.

## Parameters

None.

## Return value

None.

## Complexity

The time the thread takes to finish.

## Exceptions

`std::system_error`, as `std::thread::join` throws it: when the object stands for no thread, and when a thread
joins itself.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Result {
    int value = 0;
};

int main() {
    tracked_ptr result = make_tracked<Result>();
    thread worker([result] { result->value = 42; });
    worker.join();  // the write is visible after the join
    println("{} {}", result->value, worker.joinable());
}
```

Output:

```text
42 false
```

## See also

- [detach](detach.md): lets the thread run on independently
- [joinable](joinable.md): checks whether the object stands for a thread
- [sgcl::thread](README.md)
