[sgcl](../../README.md) › [async](../README.md) › [generator](README.md)

# sgcl::async::generator\<T\>::operator=

```cpp
generator& operator=(generator&&) noexcept = default;
```

Destroys the coroutine held, if any, running the destructors of its locals and promise wherever it was suspended,
then takes the coroutine of the other generator over; the other generator is empty after. A self-assignment does nothing. Unlike the
assignment of a [task](../task/README.md), it lets nothing run on: a generator runs only when its consumer asks for a value,
and the coroutine held is destroyed.

## Parameters

| Parameter | Description |
|---|---|
| `generator&&` | the generator whose coroutine is taken over |

## Return value

`*this`.

## Complexity

Constant, plus the destructors of the old coroutine's locals and promise.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Noisy {
    ~Noisy() {
        println("the first generator's local destroyed");
    }
};

async::generator<int> numbers(int from) {
    Noisy local;
    for (int i : range(from, from + 100)) {
        co_yield i;
    }
}

async::generator<int> letters() {
    co_yield 'a';
    co_yield 'b';
}

async::task<> consume() {
    auto g = numbers(10);
    println("{}", *co_await g.next());
    g = letters();  // the other 99 numbers never happen
    while (auto v = co_await g.next()) {
        println("{}", char(*v));
    }
}

int main() {
    consume().wait();
}
```

Output:

```text
10
the first generator's local destroyed
a
b
```

## See also

- [(constructor)](generator.md): the move constructor
- [frame_ptr::operator=](../../core/frame_ptr/operator_assign.md): what it calls
- [sgcl::async::generator\<T\>](README.md)
