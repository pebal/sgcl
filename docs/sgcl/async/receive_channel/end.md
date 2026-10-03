[sgcl](../../README.md) › [async](../README.md) › [receive_channel](README.md)

# sgcl::async::receive_channel\<T\>::end

```cpp
iterator end() const noexcept;
```

The end iterator of a range-for over the channel: the one an iterator from [begin](begin.md) becomes once the
channel is closed and drained. It receives nothing and holds no element. `receive_channel<void>` has no `end`.

## Parameters

None.

## Return value

The end iterator.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<int> numbers(1);
    async::receive_channel<int> in = numbers;
    numbers.close();
    println("{}", in.begin() == in.end());  // closed and drained: nothing to hold
}
```

Output:

```text
true
```

## See also

- [begin](begin.md): the first element, for a range-for
- [sgcl::async::receive_channel\<T\>](README.md)
