[sgcl](../../README.md) › [async](../README.md) › [task_local](README.md)

# sgcl::async::task_local\<T\>::get_or

```cpp
T get_or(T fallback) const
    noexcept(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_move_constructible_v<T>);
```

Returns a copy of the value of the task the calling thread runs, as [get](get.md) does, or `fallback` when there is
none: the task never set it and inherited none, or the thread runs no task.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | the value returned when there is none |

## Return value

The value, or `fallback`.

## Complexity

Linear in the number of sets in the task's chain, newest first; constant for a handful of keys.

## Exceptions

What the copy or the move constructor of `T` throws; none when they are noexcept.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task_local<string> user;

void log(const char* what) {
    println("[{}] {}", user.get_or("nobody"), what);
}

async::task<> session() {
    log("connected");
    co_await user.set("ann");
    log("logged in");
}

int main() {
    async::spawn(session()).wait();
    log("from main");
}
```

Output:

```text
[nobody] connected
[ann] logged in
[nobody] from main
```

## See also

- [get](get.md): the value, or nothing
- [sgcl::async::task_local\<T\>](README.md)
