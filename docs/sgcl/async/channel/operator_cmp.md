[sgcl](../../README.md) › [async](../README.md) › [channel](README.md)

# sgcl::async::operator==, operator!= (sgcl::async::channel)

```cpp
friend bool operator==(const channel& a, const channel& b) noexcept;
```

Checks whether `a` and `b` are handles of the same channel: the same state, made by one constructor and shared by
copying. Two channels made apart are never equal, whatever they hold. A hidden friend, found by the arguments' type
alone; `a != b` is rewritten to it by the compiler. The same for `channel<void>`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when `a` and `b` refer to the same channel, `false` otherwise.

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
    async::channel<int> jobs(4);
    async::channel<int> copy = jobs;
    async::channel<int> other(4);
    println("{} {}", jobs == copy, jobs != other);
}
```

Output:

```text
true true
```

## See also

- [(constructor)](channel.md): a copy is the same channel
- [sgcl::async::channel\<T\>](README.md)
