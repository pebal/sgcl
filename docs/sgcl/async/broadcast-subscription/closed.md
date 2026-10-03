[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast/README.md) › [subscription](README.md)

# sgcl::async::broadcast\<T\>::subscription::closed

```cpp
bool closed() const noexcept;
```

Checks whether the broadcast of this subscription is closed. A subscription of a closed broadcast may still have
values to receive; `nullopt` from a receive is the end. The subscription must not be empty.

## Parameters

None.

## Return value

`true` once the broadcast was closed, `false` before.

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
    async::broadcast<int> events(4);
    auto s = events.subscribe();
    events.send(1);
    events.close();

    while (true) {
        auto v = s.try_receive();
        if (!v && s.closed()) {
            break;  // nothing more will come
        }
        println("{}", v);
    }
}
```

Output:

```text
1
```

## See also

- [close](../broadcast/close.md): closes the broadcast
- [try_receive](try_receive.md): `nullopt` alone does not tell the close
- [sgcl::async::broadcast\<T\>::subscription](README.md)
