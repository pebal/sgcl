[sgcl](../../README.md) › [async](../README.md) › [mutex](../mutex.md) › [guard](../mutex-guard.md)

# sgcl::async::mutex::guard::owner

```cpp
optional<mutex> owner() const noexcept;
```

Returns a handle of the mutex the guard holds, which stays locked and held by the guard: what a
[condition_variable](../condition_variable.md) lets go of and takes back around its wait. A guard moved from or
released holds nothing, and gives nothing: there is no mutex without a state.

## Parameters

None.

## Return value

A handle of the mutex the guard holds, equal to every other handle of it; `nullopt` for a guard that holds nothing.
`optional` is the alias of `std::optional` ([aliases](../../core/aliases.md)).

## Complexity

Constant: one tracked word made from the guard's.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::mutex a;
    async::mutex b;
    auto guard = a.scoped_lock().wait();
    println("{} {}", guard.owner() == a, guard.owner() == b);
    auto moved = std::move(guard);
    println("{}", guard.owner().has_value());  // moved from: holds nothing
}
```

Output:

```text
true false
false
```

## See also

- [release](release.md): the mutex given up, still locked
- [operator==](../mutex/operator_cmp.md): whether two handles are the same mutex
- [sgcl::async::mutex::guard](../mutex-guard.md)
