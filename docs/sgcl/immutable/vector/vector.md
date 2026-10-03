[sgcl](../../README.md) › [immutable](../README.md) › [vector](../vector.md)

# sgcl::immutable::vector\<T\>::vector

```cpp
vector() noexcept;                                                                           // (1)
template<std::input_iterator InputIt> vector(InputIt first, InputIt last)                    // (2)
    noexcept(/* see below */);
vector(std::initializer_list<T> ilist) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (3)
vector(const vector& other) noexcept;                                                        // (4)
vector(vector&& other) noexcept;                                                             // (5)
```

Constructs a vector from one of the sources below.

1. An empty vector. It holds no node at all.
2. The elements of the range `[first, last)`, in its order.
3. The elements of `ilist`.
4. The version `other` holds: a copy of its four words, every node shared.
5. The same as (4): `other` keeps its version too.

The range and the list (2–3) fill the leaves in place, 32 elements at a time, and hang each full leaf on the trie
as it is: nobody holds the vector yet, so nothing is copied on the way.

## Parameters

| Parameter | Description |
|---|---|
| `first`, `last` | the range the elements are made from |
| `ilist` | the list the elements are copied from |
| `other` | the vector whose version is taken |

## Complexity

- (1) Constant.
- (2) Linear in the distance between `first` and `last`.
- (3) Linear in the size of `ilist`.
- (4–5) Constant.

## Exceptions

- (1), (4–5) None.
- (2) What the walk over the range (the copy, the comparison, the increment and the dereference of `InputIt`) and
  the constructor of `T` from an element throw; none when they are noexcept.
- (3) What the copy constructor of `T` throws; none when it is noexcept.

When an element's constructor throws, the leaves made so far are left to the collector, which destroys the
elements built in them; the exception propagates and no vector is made.

## Notes

Building from a range costs what filling a `std::vector` costs plus the branches, one per 32 leaves: a million
elements built at once take 4.9 ns each, where a million `push_back`s take 22 ns each
([Benchmarks](../benchmarks.md#against-immer)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<int> empty;
    immutable::vector<int> primes = {2, 3, 5, 7};

    vector<double> halves = {0.5, 1.5};
    immutable::vector from_range(halves.begin(), halves.end());  // deduced: vector<double>

    immutable::vector copy = primes;  // four words: the nodes are shared
    println("{} {} {} {}", empty, primes, from_range, copy);
    println("{}", copy == primes);
}
```

Output:

```text
[] [2, 3, 5, 7] [0.5, 1.5] [2, 3, 5, 7]
true
```

## See also

- [operator=](operator_assign.md): makes the variable hold another version
- [push_back](push_back.md): the vector with one more element
- [sgcl::immutable::vector\<T\>](../vector.md)
