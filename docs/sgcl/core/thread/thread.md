[sgcl](../../README.md) › [core](../README.md) › [thread](../thread.md)

# sgcl::thread::thread

```cpp
/*(1)*/ thread() noexcept = default;
/*(2)*/ template<class F, class... Args>
        explicit thread(F&& f, Args&&... args);
/*(3)*/ thread(thread&& o) noexcept = default;
/*(4)*/ thread(const thread&) = delete;
```

Constructs a thread object.

1. An object that stands for no thread: not joinable.
2. Starts a thread that calls `f(args...)`. The callable and the arguments are copied (decayed) into a managed node
   of their own, the node is made a root, and the new thread calls the callable once, with the arguments as
   rvalues, as `std::thread` calls it; when the call returns, the thread destroys the closure in the node and the
   root goes with it. Takes part only when `F` is not `thread` and the decayed `F` is invocable with the decayed
   `Args`.
3. Takes the thread of `o` over; `o` stands for no thread after.
4. A thread object is not copyable.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the callable the new thread calls |
| `args` | the arguments of the call |
| `o` | the thread object taken over |

## Complexity

- (1), (3) Constant.
- (2) The copy of the callable and the arguments, one managed allocation, one root cell, and the start of a
  thread.

## Exceptions

- (1), (3) None.
- (2) What the copy or the move of the callable and of the arguments throws, and `std::system_error` when the
  thread cannot be started, as `std::thread` throws it.

An exception the function lets out on the new thread is not passed to the caller: it ends the program with
`std::terminate`, as with `std::thread`.

## Notes

A `tracked_ptr` captured by value or passed as an argument is in the managed node, traced from the node, which is
a root until the call returns: the new thread may outlive the frame that started it, and the object with it. A
`std::thread` would put the same pointer in unmanaged memory, where the collector does not see it.

The node is one managed object: a closure whose node does not fit in a page of the managed heap
([config](../config.md): `page_size`) does not compile.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Counter {
    int value = 0;
};

void add(const tracked_ptr<Counter>& counter, int n) {
    counter->value += n;
}

int main() {
    thread none;
    println("{}", none.joinable());

    tracked_ptr counter = make_tracked<Counter>();
    thread worker(add, counter, 5);  // the pointer is copied into the managed node
    worker.join();
    println("{}", counter->value);

    thread lambda([counter] { counter->value *= 10; });
    thread moved = std::move(lambda);
    println("{} {}", lambda.joinable(), moved.joinable());
    moved.join();
    println("{}", counter->value);
}
```

Output:

```text
false
5
false true
50
```

## See also

- [join](join.md): waits for the thread to finish
- [operator=](operator_assign.md): moves a thread object
- [function](../function.md): the same closure in a managed node, called many times
- [sgcl::thread](../thread.md)
