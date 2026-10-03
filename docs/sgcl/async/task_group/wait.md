[sgcl](../../README.md) › [async](../README.md) › [task_group](../task_group.md)

# sgcl::async::task_group::wait, operator co_await

```cpp
void wait();                          // (1)
auto operator co_await() noexcept;    // (2)
```

Waits for every child to finish, then rethrows the first exception a child threw, if any. A group is waited for as a
task is ([README: Waiting operations](../README.md#waiting-operations)):

1. `g.wait()` on a thread: blocks the thread. Not from a task on a worker, which it would block with every task the
   worker runs; debug builds assert.
2. `co_await g` in a task: the task suspends, holding no thread, and is resumed by the end of the last child. The
   task holds the group's state for the wait, so the group object itself may go meanwhile.

- (1–2) The wait returns only when every child has finished, exception or not, so nothing of the scope runs on past
  it. Every wait rethrows the first exception again, as `errgroup.Wait` returns its error again; the others are
  dropped. A stop is not an error: a group whose children left on a stop waits without throwing.

## Parameters

None.

## Return value

1. None.
2. An [operation](../operation.md) that `co_await` carries out; the `co_await` gives nothing.

## Complexity

Constant when every child has finished. Otherwise a waiter on the group's count until its last child ends.

## Exceptions

- (1) The first exception a child threw. The wait itself is a receive of a channel, not `noexcept` as a receive is
  not: a receive may wake a waiting sender, and the wake may start the scheduler's workers (`std::system_error`
  when a thread cannot be started); nobody sends on the channel of the group's count.
- (2) None from the call; the `co_await` rethrows the first exception a child threw, after the same receive.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string>

using namespace sgcl;

async::task<> check(int n) {
    if (n % 2) {
        throw runtime_error(std::to_string(n) + " is odd");
    }
    co_return;
}

async::task<string> validate(vector<int> values) {
    async::task_group g;
    for (int n : values) {
        g.go(check(n));
    }
    try {
        co_await g;  // a task: no thread held
    } catch (const runtime_error& e) {
        co_return string(e.what());
    }
    co_return string("valid");
}

int main() {
    println("{}", async::spawn(validate({2, 4, 6})).wait());
    println("{}", async::spawn(validate({2, 3, 4})).wait());

    async::task_group g;
    g.go(check(5));
    for (int i : range(2)) {
        try {
            g.wait();  // a thread
        } catch (const runtime_error& e) {
            println("{}", e.what());  // the same exception, every time
        }
    }
}
```

Output:

```text
valid
3 is odd
5 is odd
5 is odd
```

## See also

- [on_done](on_done.md): the wait as a case of a select
- [go](go.md): starts the children
- [count](count.md): the children not yet finished
- [sgcl::async::task_group](../task_group.md)
