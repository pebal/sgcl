[sgcl](../../README.md) › [async](../README.md) › [promise](README.md)

# sgcl::async::operator==, operator!= (sgcl::async::promise)

```cpp
friend bool operator==(const promise& a, const promise& b) noexcept;
```

Checks whether `a` and `b` are handles of the same promise: the same state, made by one constructor and shared by
copying. Two promises made apart are never equal, whatever they hold. A hidden friend, found by the arguments' type
alone; `a != b` is rewritten to it by the compiler. The same for `promise<void>`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when `a` and `b` refer to the same promise, `false` otherwise.

## Complexity

Constant: a comparison of two words.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::promise<int> p;
    async::promise<int> copy = p;
    async::promise<int> other;
    println("{} {}", p == copy, p != other);
}
```

Output:

```text
true true
```

## See also

- [(constructor)](promise.md): a copy is the same promise
- [sgcl::async::promise\<T\>](README.md)
