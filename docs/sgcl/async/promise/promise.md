[sgcl](../../README.md) › [async](../README.md) › [promise](../promise.md)

# sgcl::async::promise\<T\>::promise

```cpp
/*(1)*/ promise() noexcept;
/*(2)*/ promise(const promise& other) noexcept;
/*(3)*/ promise(promise&& other) noexcept;
```

Makes a promise, or another handle of an existing one.

1. A promise not set: its state on the managed heap, with the channel the waiters wait on.
2. A handle of the promise `other` is: the copies share the state, and `==` gives `true` for them. The side that sets
   and the sides that wait each hold one.
3. The same as (2): a move of the handle's word is a copy, and `other` still refers to the promise.

The constructors of `promise<void>` are the same.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle of the promise to refer to |

## Complexity

- (1) Constant: one managed object.
- (2–3) Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::promise<int> answer;
    async::promise<int> same = answer;
    println("{} {}", answer.done(), same == answer);

    same.set_value(42);
    println("{} {}", answer.done(), answer.result());

    async::promise<> finished;  // a completion without a value
    println("{}", finished.done());
}
```

Output:

```text
false true
true 42
false
```

## See also

- [operator=](operator_assign.md): makes the handle one of another promise
- [set_value](set_value.md): sets the promise
- [sgcl::async::promise\<T\>](../promise.md)
