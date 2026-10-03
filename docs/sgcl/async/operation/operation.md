[sgcl](../../README.md) › [async](../README.md) › [operation](README.md)

# sgcl::async::operation\<F\>::operation

```cpp
operation(operation&& o) noexcept(std::is_nothrow_move_constructible_v<F>);    // (1)
operation(const operation&) = delete;                                          // (2)
```

Constructs an operation from another. An operation of a callable is made by the functions of the module that wait,
through a constructor of the library's own, private: the callable is called with tags of the library, so a program
gets its operations from those functions and does not make one itself.

1. Takes the callable of `o` over: the operation is carried out through the new object, and `o`, moved from, is not
   asserted on when it is dropped.
2. Not copyable: an operation is carried out once.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the operation to take over |

## Complexity

Constant: the move of the callable.

## Exceptions

What the move constructor of `F` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

// Carries out on this thread an operation it is given
template<class F>
auto carry_out(async::operation<F> op) {
    return std::move(op).wait();
}

int main() {
    async::channel<int> ch(1);
    auto send = ch.send(5);
    auto kept = std::move(send);  // the operation moves; nothing is sent yet
    println("{}", ch.size());
    println("{}", carry_out(std::move(kept)));
    println("{}", *carry_out(ch.receive()));
}
```

Output:

```text
0
true
5
```

## See also

- [wait, await_ready, await_suspend, await_resume](wait.md): carry the operation out
- [sgcl::async::operation\<F\>](README.md)
