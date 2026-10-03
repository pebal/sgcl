[sgcl](../../README.md) › [core](../README.md)

# sgcl::array\<T, N\>

```cpp
#include "sgcl/core/array.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, size_t N>
    class array;

    template<class T>
    class array<T, 0>;
}
```

`sgcl::array<T, N>` is `std::array`: the `N` elements inline, no memory of its own, the tuple interface
(`std::tuple_size`, `std::tuple_element`, `sgcl::get<I>`, structured bindings), `constexpr` throughout. It exists
so that a fixed set of `tracked_ptr`s can be written as one object,
`sgcl::array<sgcl::tracked_ptr<T>, 4> roots = {}`, that lives wherever its elements may and costs nothing beyond
them, and so that a fixed set of anything has the members of every range of the library: `a.sort()`,
`a.contains(x)`, `a.min()` ([the mixins](../mixin/README.md)).
`array<T, 0>` has the same interface over no elements. An array is made from a built-in array by
[to_array](../to_array.md). For a count known only at run time, in a managed buffer: [dynamic_array](../dynamic_array/README.md).

Unlike `std::array` it is not an aggregate — a class with the mixins as its bases, which an aggregate cannot
carry — but it keeps the aggregate's braces: `array<int, 3> a = {1, 2, 3}` ([constructor](array.md)), and
the rest of what an aggregate gives: a trivial default constructor, trivial copies, `sizeof == N * sizeof(T)`.

## Rules

- `array<T, N>` lives wherever its elements may: when `T` is or contains a `tracked_ptr`, on a stack or inside a
  managed object only ([The rules](../README.md#the-rules), 1).
- Iterators are raw pointers in a thin class (`std::contiguous_iterator`), cheap to copy, at home in any
  container; the array carries the contiguous category ([req::contiguous](../req/contiguous.md)).
- Thread safety is that of a `std::array`: concurrent readers, or one writer, with the program's own
  synchronization.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements: any object type that is not a reference or an array. An operation that copies or moves elements requires `T` to be copyable or movable. |
| `N` | The number of elements. `array<T, 0>` is a [specialization](#specializations). |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `reference` | `T&` |
| `const_reference` | `const T&` |
| `pointer` | `T*` |
| `const_pointer` | `const T*` |
| `iterator` | a pointer to `T` in a class of the library, `std::contiguous_iterator` |
| `const_iterator` | the same over `const T` |
| `reverse_iterator` | `std::reverse_iterator<iterator>` |
| `const_reverse_iterator` | `std::reverse_iterator<const_iterator>` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](array.md) | constructs the array from its elements, or leaves them uninitialized |
| `(destructor)` | destroys the elements; implicitly declared |
| `operator=` | assigns every element from the other array's; implicitly declared |

#### Element access

| Function | Description |
|---|---|
| [at](at.md) | access the element at a position, with bounds checking |
| [operator[]](operator_at.md) | access the element at a position |
| [front](front.md) | access the first element |
| [back](back.md) | access the last element |
| [data](data.md) | the elements as a plain pointer |
| [as_slice, operator slice](as_slice.md) | the elements, or a part of them, as a slice |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the beginning |
| [end, cend](end.md) | an iterator to the end |
| [rbegin, crbegin](rbegin.md) | a reverse iterator to the beginning |
| [rend, crend](rend.md) | a reverse iterator to the end |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the array is empty |
| [size](size.md) | the number of elements |
| [max_size](max_size.md) | the largest number of elements, `N` |

#### Operations

| Function | Description |
|---|---|
| [fill](fill.md) | assigns a value to every element |
| [swap](swap.md) | swaps the elements with another array's |

#### From mixin::enumerable

The questions asked of the elements, carried by every container of the library
([mixin::enumerable](../mixin/enumerable/README.md)). They are `constexpr`, so a table built at compile time can be searched
at compile time.

| Function | Description |
|---|---|
| [contains](../mixin/enumerable/contains.md) | checks whether an element is equal to a value |
| `index_of` | the position of the first element equal to a value |
| `last_index_of` | the position of the last element equal to a value |
| `find_if` | a pointer to the first element the predicate accepts |
| `find_index` | the position of the first element the predicate accepts |
| `exists` | checks whether the predicate accepts some element |
| `all` | checks whether the predicate accepts every element |
| `count_of` | the number of elements the predicate accepts |
| `min`, `max` | the smallest, the largest element |
| `for_each` | calls a function with every element |

#### From mixin::ordered

The order of the elements ([mixin::ordered](../mixin/ordered/README.md)).

| Function | Description |
|---|---|
| `sort` | sorts the elements |
| `sort_by` | sorts the elements by a projection |
| `stable_sort` | sorts the elements, keeping the order of equal ones |
| `is_sorted` | checks whether the elements are sorted |
| `binary_search` | checks whether a sorted array holds a value |
| `lower_bound`, `upper_bound` | the first element not less than, greater than a value, in a sorted array |
| `sorted_index_of` | the position of a value in a sorted array |

#### From mixin::sequence

The writes over every element ([mixin::sequence](../mixin/sequence/README.md)); the array's own [fill](fill.md)
hides the mixin's.

| Function | Description |
|---|---|
| `reverse` | reverses the order of the elements |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](operator_cmp.md) | compare the elements lexicographically |
| [swap](swap2.md) | swaps the elements of two arrays |
| [get](get.md) | the element at a position given at compile time |

## Deduction guides

```cpp
template<class T, class... U>
array(T, U...) -> array<std::enable_if_t<(std::is_same_v<T, U> && ...), T>, 1 + sizeof...(U)>;

template<class T, size_t N>
slice(array<T, N>&) -> slice<T>;

template<class T, size_t N>
slice(const array<T, N>&) -> slice<const T>;
```

The first deduces an array from its elements, all of one type: `array fixed = {1.5, 2.5}` is an
`array<double, 2>`, and `array mixed = {1, 2.5}` is an error. The two others, declared with the array, let a
[slice](../slice/README.md) deduce its type from an array: `slice s(a)` is a `slice<T>`, or a `slice<const T>` from a const
array.

## Specializations

`array<T, 0>` has the interface of the primary template over no elements, but for the members that would need an
element: it has no `operator[]`, `front` or `back`. Its `at` always throws `out_of_range`, `data()` is
`nullptr`, `empty()` is `true`, `fill` and `swap` (the member and the free one) do nothing; two `array<T, 0>` are
equal. It holds nothing: `sizeof` is 1, as for any empty class.

The tuple interface, in `std`:

| Specialization | Definition |
|---|---|
| `std::tuple_size<sgcl::array<T, N>>` | `N` |
| `std::tuple_element<I, sgcl::array<T, N>>` | `T` |

With [get](get.md) they make structured bindings work: `auto [x, y, z] = a`.

## Complexity

- Access to an element: constant.
- `fill`, `swap`, the comparisons, a copy: linear in `N`.

## Iterator invalidation

An iterator, a pointer or a reference to an element is valid as long as the array: nothing an array does moves
an element. After `swap` an iterator still points into its own array, at the value the other array had there.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value = 0;
    tracked_ptr<Node> next;
};

struct Holder {
    array<tracked_ptr<Node>, 3> nodes;  // three pointers inline, traced with the object
};

int main() {
    // A fixed set of roots on the stack, as one object
    array<tracked_ptr<Node>, 8> roots = {};
    for (int i : range(8)) {
        roots[i] = make_tracked<Node>(i);
        if (i) {
            roots[i]->next = roots[i - 1];  // a chain, rooted through the array
        }
    }

    // Inside a managed object: the pointers go with the object
    tracked_ptr h = make_tracked<Holder>();
    h->nodes[2] = roots[7];

    tracked_ptr<Node> keep = roots[3];
    roots.fill(nullptr);  // the chain lives on behind keep and h->nodes[2]
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    println("node behind keep {}, held by h {}", keep->next->value, h->nodes[2]->value);

    // A table known at compile time, searched at compile time
    constexpr array squares = {0, 1, 4, 9, 16, 25};
    constexpr bool found = squares.binary_search(16);
    constexpr size_t nine = squares.sorted_index_of(9);
    int even = squares.count_of([](int x) { return x % 2 == 0; });
    println("{} {}, squares up to {}, {} even", found, nine, squares.max(), even);
}
```

Output:

```text
node behind keep 2, held by h 7
true 3, squares up to 25, 3 even
```

## See also

- [dynamic_array](../dynamic_array/README.md): a count fixed at creation, in a managed buffer
- [vector](../vector/README.md): a buffer that grows
- [to_array](../to_array.md): an array from a built-in array
- [slice](../slice/README.md): a view of elements
- [the mixins and the requirements](../mixin/README.md), [tracked_ptr](../tracked_ptr/README.md), [make_tracked](../make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
