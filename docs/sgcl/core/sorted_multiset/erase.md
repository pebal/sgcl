[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::erase

```cpp
iterator erase(const_iterator pos) noexcept;                             // (1)
iterator erase(const_iterator first, const_iterator last) noexcept;      // (2)
size_type erase(const key_type& key) noexcept;                           // (3)
template<class K> size_type erase(K&& key) noexcept(/* see below */);    // (4)
```

Erases elements: each is destroyed at once and its node unlinked from the tree; the collector reclaims the node
later.

1. Erases the element at `pos`, one element, whatever its equivalents.
2. Erases the elements of the range `[first, last)`. The whole multiset, `[begin(), end())`, is erased as by
   [clear](clear.md).
3. Erases every element with a key equivalent to `key`.
4. As (3), with a key of another type, compared without building a `key_type`. Takes part only when `Compare`
   declares `is_transparent`, as `std::less` of a [string](../string/README.md) does, and `K` converts to neither
   `iterator` nor `const_iterator`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | an iterator to the element to erase; not `end()` |
| `first`, `last` | the range of the elements to erase |
| `key` | the key of the elements to erase |

## Return value

- (1) An iterator to the element after `pos`, or `end()`.
- (2) `last`.
- (3–4) The number of elements erased.

## Complexity

- (1) Amortized constant.
- (2) Linear in the number of elements erased.
- (3–4) Logarithmic in the size of the multiset, plus the number of elements erased.

## Exceptions

- (1–3) None.
- (4) What the calls of `Compare` with a `K` throw; none when they are noexcept or `Compare` is a
  function object of `std`.

## Notes

The erased elements die in `erase`, not when the collector comes. Iterators to the other elements stay valid,
so a loop erases as it goes with `it = multiset.erase(it)`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multiset<int> numbers = {1, 1, 2, 3, 3, 3, 4};

    size_t threes = numbers.erase(3);
    println("{} {}", threes, numbers);

    numbers.erase(numbers.find(1));  // one of the two
    println("{}", numbers);

    numbers.erase(numbers.begin(), numbers.find(4));
    println("{}", numbers);
}
```

Output:

```text
3 {1, 1, 2, 4}
{1, 2, 4}
{4}
```

## See also

- [clear](clear.md): erases every element
- [extract](extract.md): takes an element out without destroying it
- [erase_if](erase_if.md): erases the elements a predicate accepts
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
