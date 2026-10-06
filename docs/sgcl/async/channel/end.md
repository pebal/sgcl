[sgcl](../../README.md) › [async](../README.md) › [channel](README.md)

# sgcl::async::channel\<T\>::end

```cpp
std::default_sentinel_t end() const noexcept;
```

The end of a range-for over the channel: `std::default_sentinel`, which an iterator from [begin](begin.md) compares
equal to once the channel is closed and drained. The comparison is a look: an iterator that holds no element yet
receives one, waiting for it, to tell whether the channel has ended. `end` itself receives nothing.
`channel<void>` has no `end`.

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
    async::channel<int> numbers(4);
    numbers.send(10).wait();
    numbers.send(20).wait();
    numbers.close();

    int total = 0;
    for (auto it = numbers.begin(); it != numbers.end(); ++it) {
        total += *it;
    }
    println("{}", total);
    println("{}", numbers.begin() == numbers.end());  // closed and drained
}
```

Output:

```text
30
true
```

## See also

- [begin](begin.md): the iterator, which receives at the first look
- [sgcl::async::channel\<T\>](README.md)
