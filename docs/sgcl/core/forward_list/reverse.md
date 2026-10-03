[sgcl](../../README.md) › [core](../README.md) › [forward_list](README.md)

# sgcl::forward_list\<T\>::reverse

```cpp
void reverse() noexcept;
```

Reverses the order of the elements in place, by turning every link back: no element is moved or copied, and
iterators and references stay valid, naming the same elements at their new places. `reverse` of
[mixin::sequence](../mixin/sequence/README.md), which needs a bidirectional range, is not a forward_list's.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of elements.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    forward_list<string> words = {"one", "two", "three"};
    string& first = words.front();
    words.reverse();
    println("{}, the old first {}", words, first);
}
```

Output:

```text
["three", "two", "one"], the old first one
```

## See also

- [sort](sort.md): sorts the nodes
- [sgcl::forward_list\<T\>](README.md)
