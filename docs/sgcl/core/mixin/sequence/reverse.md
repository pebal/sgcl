[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [sequence](../sequence.md)

# sgcl::mixin::sequence\<Derived\>::reverse

```cpp
constexpr void reverse() noexcept(/* see below */) requires req::bidirectional<Derived>;
```

Reverses the order of the elements in place, swapping the first with the last, the second with the one before
the last, and so on to the middle. Takes part only on a range walked backwards (`req::bidirectional`); it asks
nothing of the element beyond its swap.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the size of the range: exactly n / 2 swaps for n elements, rounded down.

## Exceptions

None when the swap of the elements is noexcept; otherwise what it throws.

If an exception is thrown, the pairs swapped before it stay swapped, and the elements of the pair whose swap threw
are as that swap left them.

## Notes

`list` and `forward_list` hide this one with a `reverse` of their own, on the nodes: the elements are not moved
and the call cannot throw; `forward_list`, which is not bidirectional, has only its own. An immutable list has a
`reverse` that returns a new list.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {1, 2, 3};
    v.reverse();
    println("{}", v);

    vector<string> words = {"one", "two", "three", "four"};
    words.as_slice(1).reverse();
    println("{}", words);
}
```

Output:

```text
[3, 2, 1]
["one", "four", "three", "two"]
```

## See also

- [fill](fill.md): assigns a value to every element
- [sort](../ordered/sort.md): sorts the elements
- [sgcl::mixin::sequence\<Derived\>](../sequence.md)
