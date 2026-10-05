[sgcl](../../README.md) › [core](../README.md)

# sgcl::move_only_function\<R(Args...)\>

```cpp
#include "sgcl/core/function.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Signature>
    class move_only_function;   // undefined

    template<class R, class... Args>
    class move_only_function<R(Args...)>;
    template<class R, class... Args>
    class move_only_function<R(Args...) const>;
    template<class R, class... Args>
    class move_only_function<R(Args...) noexcept>;
    template<class R, class... Args>
    class move_only_function<R(Args...) const noexcept>;
}
```

**Requires [rooted](../rooted/README.md) outside a stack or a managed object.**

`sgcl::move_only_function<Signature>` is `std::move_only_function` (C++23) over the storage of
[function](../function/README.md): a pointer word in the word, a small callable that cannot hold a pointer in the buffer of 16
bytes, any other in a managed node of its own, traced, so that a closure capturing a `tracked_ptr` keeps its object
and a closure capturing the object that holds the `move_only_function` is a cycle collected like any other. The
callable need not be copyable: a lambda capturing a `unique_ptr` or a `vector` taken over goes in as it is.

The interface is that of `std::move_only_function`: the constructors (a null function pointer, a null member
pointer, an empty `move_only_function` of any signature or an empty function of either library make an empty one),
`in_place_type`, the move assignment and the assignment of a callable or `nullptr`, `swap`, `operator bool`, the
call and `==` with `nullptr`. The signature's `const` and `noexcept` are honoured: `R(Args...) const` is callable
through a `const move_only_function&` and calls the callable as `const`, `R(Args...) noexcept` makes the call
`noexcept` and takes only a callable whose call cannot throw. The reference qualifiers `&` and `&&` are not
supported. Calling an empty one is undefined, as with `std`; debug builds assert. There is no `target_type` and no
`target`, as in `std`. A `move_only_function` is 32 bytes.

## Rules

- The callable of a `move_only_function` follows the rules of its captures where the `move_only_function` lives.
- A closure in a node is destroyed by an assignment, the assignment of `nullptr` or the destructor, at once, on the
  calling thread; the node is left to the collector.
- Non-copyable: a callable that may not be copied has one owner. A move hands the callable over and leaves the
  source empty.
- Thread safety is that of `std::move_only_function`: threads share one with the program's own synchronization
  ([The rules](../README.md#the-rules), 6).

## Template parameters

| Parameter | Description |
|---|---|
| `R` | The result type of the call; `void` discards the callable's result. |
| `Args` | The parameter types of the call. |

## Member types

| Type | Definition |
|---|---|
| `result_type` | `R` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](move_only_function.md) | constructs a `move_only_function`, empty or holding a callable |
| `(destructor)` | destroys the callable, if any; a node is left to the collector |
| [operator=](operator_assign.md) | assigns another `move_only_function`, a callable or `nullptr` |
| [swap](swap.md) | swaps the callables of two `move_only_function` objects |
| [operator bool](operator_bool.md) | checks whether there is a callable |
| [operator()](operator_call.md) | calls the callable |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | compares with `nullptr` |
| [swap](swap2.md) | swaps the callables of two `move_only_function` objects |

## Complexity

Every operation is constant. A callable in the buffer costs no allocation; one in a node costs one managed
allocation per construction. A call is one indirect call, plus the step through the node's pointer for a callable
in a node.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <memory>

using namespace sgcl;

struct Job {
    string name;
    move_only_function<void()> run;  // a member of a managed object
};

int main() {
    auto buffer = std::make_unique<int>(41);
    tracked_ptr job = make_tracked<Job>("job", [b = std::move(buffer)] { println("{}", *b + 1); });
    job->run();

    move_only_function<int(int) const noexcept> square = [](int x) noexcept { return x * x; };
    println("{}", square(7));
}
```

Output:

```text
42
49
```

## See also

- [function](../function/README.md): the copyable one, with `target`
- [any](../any/README.md): the same storage for a value
- [unique_ptr](../unique_ptr/README.md): what a move-only closure typically captures
- [README: The rules](../README.md#the-rules)
