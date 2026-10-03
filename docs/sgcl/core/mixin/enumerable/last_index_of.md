[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [enumerable](../enumerable.md)

# sgcl::mixin::enumerable\<Derived\>::last_index_of

```cpp
constexpr size_t last_index_of(const auto& value) const noexcept(/* see below */);
```

Finds the position of the last element equal to `value`. The range is walked forward, from the first element to
the end, so it needs no bidirectional iterator: a `forward_list` has it too. Takes part only when the elements
are `req::equatable`; `value` may be of any type the elements compare with.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to look for |

## Return value

The position of the last element equal to `value`, `npos` when there is none.

## Complexity

Linear in the size of the range: exactly one comparison per element, the whole range walked.

## Exceptions

None when the `==` of an element with `value` is noexcept; otherwise what it throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {5, 3, 9, 3};
    println("{} {}", v.index_of(3), v.last_index_of(3));

    forward_list<string> words = {"to", "be", "or", "not", "to", "be"};
    println("{}", words.last_index_of("to"));
    println("{}", words.last_index_of("is") == npos);
}
```

Output:

```text
1 3
4
true
```

## See also

- [index_of](index_of.md): the position of the first element equal to a value
- [contains](contains.md): checks whether an element is equal to a value
- [sgcl::mixin::enumerable\<Derived\>](../enumerable.md)
