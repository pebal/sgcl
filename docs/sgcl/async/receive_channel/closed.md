[sgcl](../../README.md) › [async](../README.md) › [receive_channel](../receive_channel.md)

# sgcl::async::receive_channel\<T\>::closed

```cpp
bool closed() const noexcept;
```

Checks whether the channel is closed, by whoever holds its sending end. A closed channel may still hold elements,
which receives still give. The same for `receive_channel<void>`: the receiving end of a
[stop_token](../stop_token/channel.md)'s channel is closed once the stop is requested.

## Parameters

None.

## Return value

`true` once the channel was closed, through any of its handles; `false` before.

## Complexity

Constant: one atomic load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::stop_source source;
    async::receive_channel<void> stop = source.token().channel();
    println("{}", stop.closed());
    source.request_stop();
    println("{}", stop.closed());
}
```

Output:

```text
false
true
```

## See also

- [try_receive](try_receive.md): an empty channel and a closed one look the same there
- [sgcl::async::receive_channel\<T\>](../receive_channel.md)
