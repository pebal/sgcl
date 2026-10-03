[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::reverse

```cpp
void reverse() noexcept;
```

Reverses the order of the elements in place, by swapping the two links of every node: no element is moved or
copied, and iterators and references stay valid, naming the same elements at their new places. It hides the
`reverse` of [mixin::sequence](../mixin/sequence.md), which swaps the elements themselves.

## Parameters

None.

## Return value

None.

## Complexity

Linear in `size()`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list<string> words = {"one", "two", "three"};
    string& first = words.front();
    words.reverse();
    println("{}, the old first {}, now last: {}", words, first, &first == &words.back());
}
```

Output:

```text
["three", "two", "one"], the old first one, now last: true
```

## See also

- [sort](sort.md): sorts the nodes
- [sgcl::list\<T\>](../list.md)
