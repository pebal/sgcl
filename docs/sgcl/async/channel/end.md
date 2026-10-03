[sgcl](../../README.md) › [async](../README.md) › [channel](../channel.md)

# sgcl::async::channel\<T\>::end

```cpp
iterator end() const noexcept;
```

The end iterator of a range-for over the channel: the one an iterator from [begin](begin.md) becomes once the
channel is closed and drained. It receives nothing and holds no element. `channel<void>` has no `end`.

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

- [begin](begin.md): receives the first element
- [sgcl::async::channel\<T\>](../channel.md)
