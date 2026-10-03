[sgcl](../README.md) › [core](README.md) › [generator](generator.md)

# sgcl::generator\<T\>::promise_type

```cpp
#include "sgcl/core/generator.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class generator {
    public:
        struct promise_type;   // : managed_frame
    };
}
```

The promise of a generator's coroutine: what the compiler finds through the return type `generator<T>` of a
coroutine function, constructs in the frame and calls at the coroutine's points of suspension. A program does not
call it; it writes `co_yield v` and `co_return;`, and the promise keeps the value and the error for `next()` and
`value()`. It derives from [managed_frame](managed_frame.md), so the frame, the promise in it included, is on the
managed heap and its words are roots while the generator holds it.

## Rules

- Lazy: `initial_suspend` suspends, so nothing runs until the first `next()` (or `begin()`).
- `co_yield v` moves `v` into `value` and suspends; the coroutine ends with `co_return;` or by falling off its end,
  and may not `co_return` a value.
- An exception that leaves the coroutine is kept in `error`, and `next()` rethrows it.

## Member objects

| Member | Description |
|---|---|
| `value` | an `optional<T>`: the value of the last `co_yield`, empty before the first |
| `error` | a `std::exception_ptr`: the exception the coroutine let out, null otherwise |

## Member functions

Each is called by the compiler, none by the program; all but `yield_value` are `noexcept`, and `yield_value` is
`noexcept` when the move of `T` is.

| Function | Description |
|---|---|
| `get_return_object` | the generator that holds the frame, made from the coroutine's handle |
| `initial_suspend` | `std::suspend_always`: the coroutine is lazy |
| `final_suspend` | `std::suspend_always`: the coroutine stops at its end, so that the generator sees it done and destroys the frame |
| `yield_value` | takes the value by value, `value.emplace(std::move(v))`, then suspends |
| `return_void` | `co_return;` and the end of the body |
| `unhandled_exception` | `error = std::current_exception()` |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

generator<int> squares(int n) {
    for (int i : range(1, n + 1)) {
        co_yield i * i;  // into the promise's value, then suspended until the next next()
    }
}

int main() {
    using Promise = generator<int>::promise_type;
    println("{}", std::is_base_of_v<managed_frame, Promise>);

    generator<int> g = squares(3);
    while (g.next()) {
        println("{}", g.value());
    }
}
```

Output:

```text
true
1
4
9
```

## See also

- [generator](generator.md): the coroutine type
- [managed_frame](managed_frame.md): where the frame comes from
