[sgcl](../../README.md) › [async](../README.md) › [channel](README.md)

# sgcl::async::channel\<T\>::closed

```cpp
bool closed() const noexcept;
```

Checks whether the channel is closed. A closed channel may still hold elements, which receives still give. The same
for `channel<void>`.

## Parameters

None.

## Return value

`true` once [close](close.md) was called on the channel, through any of its handles; `false` before.

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
    async::channel<int> numbers(2);
    async::channel<int> same = numbers;
    numbers.send(1).wait();
    println("{}", same.closed());

    numbers.close();
    println("{} {}", same.closed(), same.size());
    println("{}", same.try_receive());
}
```

Output:

```text
false
true 1
1
```

## See also

- [close](close.md): closes the channel
- [empty](empty.md): whether anything is left to receive
- [sgcl::async::channel\<T\>](README.md)
