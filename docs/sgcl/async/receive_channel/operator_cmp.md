[sgcl](../../README.md) › [async](../README.md) › [receive_channel](README.md)

# sgcl::async::operator==, operator!= (sgcl::async::receive_channel)

```cpp
friend bool operator==(const receive_channel& a, const receive_channel& b) noexcept;
```

Checks whether `a` and `b` are handles of the same channel: the same state. A `channel<T>` on either side converts
to its receiving end first, so a channel and its `receive_channel` compare equal. A hidden friend, found by the
arguments' type alone; `a != b` is rewritten to it by the compiler. The same for `receive_channel<void>`.

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
    async::channel<int> jobs(4), other(4);
    async::receive_channel<int> in = jobs;
    println("{} {}", in == jobs, in != other);
}
```

Output:

```text
true true
```

## See also

- [(constructor)](receive_channel.md): the receiving end is the same channel
- [sgcl::async::receive_channel\<T\>](README.md)
