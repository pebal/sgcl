[sgcl](../../README.md) › [async](../README.md) › [receive_channel](README.md)

# sgcl::async::receive_channel\<T\>::end

```cpp
std::default_sentinel_t end() const noexcept;
```

The end of a range-for over the channel: `std::default_sentinel`, which an iterator from [begin](begin.md) compares
equal to once the channel is closed and drained; the comparison receives an element when the iterator holds none,
`end` itself receives nothing. `receive_channel<void>` has no `end`.

## Parameters

None.

## Return value

`std::default_sentinel`.

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
