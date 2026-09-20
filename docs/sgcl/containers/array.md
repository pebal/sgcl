# sgcl::array

```cpp
#include "sgcl/containers/array.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T, size_t N>
    class array;                              // the N elements inline, with the braces of an aggregate
}
```

`sgcl::array<T, N>` is `std::array`: the `N` elements inline, no memory of its own, the tuple interface (`std::tuple_size`, `std::tuple_element`, `sgcl::get<I>`, structured bindings), `constexpr` throughout. It exists so that a fixed set of `tracked_ptr`s can be written as one object, `sgcl::array<sgcl::tracked_ptr<T>, 4> roots = {}`, that lives wherever its elements may and costs nothing beyond them, and so that a fixed set of anything has the members of every range of the library: `a.sort()`, `a.contains(x)`, `a.min()` ([the mixins](../core/mixin/README.md)). `array<T, 0>` has the same interface over no elements. For a count known only at run time, in a managed buffer: [dynamic_array](dynamic_array.md).

Unlike `std::array` it is not an aggregate — a class with the mixins as its bases, which an aggregate cannot carry — but it keeps the aggregate's braces: `array<int, 3> a = {1, 2, 3}`, and the rest of what an aggregate gives (a trivial default constructor, trivial copies, `sizeof == N * sizeof(T)`).

## Rules

- `array<T, N>` lives wherever its elements may: when `T` is or contains a `tracked_ptr`, on a stack or inside a managed object only ([The rules](../core/README.md#the-rules), 1).
- Iterators are raw pointers in a thin class (`std::contiguous_iterator`), cheap to copy, at home in any container.
- Thread safety is that of a `std::array`: concurrent readers, or one writer, with the program's own synchronization.

## Members
### Types

```cpp
using value_type = T;  using reference = T&;  using const_reference = const T&;
using pointer = T*;  using const_pointer = const T*;
using size_type = size_t;  using difference_type = ptrdiff_t;
using iterator = detail::ContiguousIterator<T>;  using const_iterator = detail::ContiguousIterator<const T>;
using reverse_iterator = std::reverse_iterator<iterator>;  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
```

### Constructors

```cpp
constexpr array() = default;                 // trivial: the elements uninitialized, as an aggregate's; `= {}` zeroes them
constexpr array(T... elements);              // N parameters of type T, one per element: {1, 2, 3}
```

The braces of an aggregate, as a constructor: `N` parameters of type `T`, one per element, generated from an index sequence that is a third, defaulted template parameter of the class (`array<T, N>` is spelled as ever). Each element is constructed in place from its argument; a fourth argument for an `array<T, 3>` is an error at compile time; an element that only moves is moved in; an element that is itself an aggregate takes its brace, `array<point, 2> p = {{1, 2}, {3, 4}}`, one level for one level. What the constructor does not do that the aggregate did: fill in the rest when fewer elements are given — `array<int, 3> a = {7}` is an error, `= {}` the way to zero.

```cpp
sgcl::array<int, 3> a = {1, 2, 3};
sgcl::array<sgcl::tracked_ptr<int>, 2> roots = {};             // two null pointers, on the stack
roots[0] = sgcl::make_tracked<int>(5);
constexpr sgcl::array<int, 2> c = {4, 5};                    // constexpr: everything is
static_assert(c[1] == 5 && c.size() == 2 && c.contains(4));
sgcl::array<std::unique_ptr<int>, 2> owned = {std::make_unique<int>(1), std::make_unique<int>(2)};   // moved in
sgcl::array<int, 4> raw;                                     // uninitialized, as int[4]
```

### at, operator[]

```cpp
constexpr reference at(size_type pos);  constexpr const_reference at(size_type pos) const;
constexpr reference operator[](size_type pos) noexcept;  constexpr const_reference operator[](size_type pos) const noexcept;
```

`at` throws `std::out_of_range` for `pos >= N` (always, for `array<T, 0>`); `operator[]` does not check. `array<T, 0>` has no `operator[]`.

### front, back, data

```cpp
constexpr reference front() noexcept;  constexpr const_reference front() const noexcept;    // N > 0
constexpr reference back() noexcept;   constexpr const_reference back() const noexcept;
constexpr T* data() noexcept;  constexpr const T* data() const noexcept;                   // nullptr for N == 0
```

`front` and `back` require an element; `array<T, 0>` does not have them.

### Iterators

```cpp
constexpr iterator begin() noexcept;                      constexpr const_iterator begin() const noexcept;
constexpr const_iterator cbegin() const noexcept;
constexpr iterator end() noexcept;                        constexpr const_iterator end() const noexcept;
constexpr const_iterator cend() const noexcept;
constexpr reverse_iterator rbegin() noexcept;             constexpr const_reverse_iterator rbegin() const noexcept;
constexpr const_reverse_iterator crbegin() const noexcept;
constexpr reverse_iterator rend() noexcept;               constexpr const_reverse_iterator rend() const noexcept;
constexpr const_reverse_iterator crend() const noexcept;
```

Contiguous iterators over the elements, raw pointers: `std::ranges` algorithms work. An iterator dying in a frame nulls its word, so a temporary left behind does not root anything under the conservative stack scan. `array<T, 0>` has no `crbegin`/`crend`.

### empty, size, max_size

```cpp
[[nodiscard]] constexpr bool empty() const noexcept;    // N == 0
constexpr size_type size() const noexcept;              // N
constexpr size_type max_size() const noexcept;          // N
```

### The mixins

`array<T, N>` carries [mixin::enumerable](../core/mixin/enumerable.md), [mixin::equatable](../core/mixin/equatable.md), [mixin::comparable](../core/mixin/comparable.md), [mixin::ordered](../core/mixin/ordered.md), [mixin::sequence](../core/mixin/sequence.md) and the contiguous category: everything a `vector` answers, `constexpr`, so a table built at compile time can be searched at compile time.

```cpp
constexpr sgcl::array primes = {2, 3, 5, 7, 11};
static_assert(primes.is_sorted() && primes.binary_search(7) && primes.sorted_index_of(11) == 4 && primes.max() == 11);
sgcl::array a = {5, 3, 9, 3};
assert(a.contains(9) && a.index_of(3) == 1 && a.find_index([](int x) { return x > 4; }) == 0);
if (int* big = a.find_if([](int x) { return x > 8; })) {
    *big = 8;
}
a.sort();                                        // 3 3 5 8
a.reverse();                                     // 8 5 3 3
a.fill(0);
```

### fill, swap

```cpp
constexpr void fill(const T& value);
constexpr void swap(array& other) noexcept(std::is_nothrow_swappable_v<T>);   // element by element
friend constexpr void swap(array& l, array& r) noexcept(noexcept(l.swap(r)));
```

### Comparisons

`==` and `<=>` come with [mixin::equatable](../core/mixin/equatable.md) and [mixin::comparable](../core/mixin/comparable.md): element-wise, as for `std::array`, `<=>` lexicographical with the synthesized three-way comparison (`<=>` of `T` when it has one, else a `std::weak_ordering` built from `<`), and only for elements that compare. Two `array<T, 0>` are equal.

```cpp
sgcl::array<double, 2> e = {1.0, 2.0}, f = {1.0, 3.0};
bool less = e < f;                                    // true
bool equal = (e <=> f) == std::partial_ordering::less;   // true: double's ordering
```

### Deduction guide

```cpp
template<class T, class... U>
array(T, U...) -> array<T, 1 + sizeof...(U)>;                  // all of the same type
```

```cpp
sgcl::array fixed = {1.5, 2.5};                         // sgcl::array<double, 2>
```

### get, to_array, the tuple interface

```cpp
template<size_t I, class T, size_t N> constexpr T& get(array<T, N>& a) noexcept;
template<size_t I, class T, size_t N> constexpr const T& get(const array<T, N>& a) noexcept;
template<size_t I, class T, size_t N> constexpr T&& get(array<T, N>&& a) noexcept;

template<class T, size_t N> constexpr array<std::remove_cv_t<T>, N> to_array(T (&a)[N]);
template<class T, size_t N> constexpr array<std::remove_cv_t<T>, N> to_array(T (&&a)[N]);

namespace std {
    template<class T, size_t N> struct tuple_size<sgcl::array<T, N>>;                    // N
    template<size_t I, class T, size_t N> struct tuple_element<I, sgcl::array<T, N>>;   // T
}
```

`get<I>` is the `I`-th element with a `static_assert` on the range, `to_array` builds an array from a built-in array (copying or moving the elements), and the `std` specializations make structured bindings work.

```cpp
sgcl::array<int, 3> a = {1, 2, 3};
auto [x, y, z] = a;                          // structured bindings
sgcl::get<1>(a) = 20;
auto b = sgcl::to_array({3, 2, 1});            // sgcl::array<int, 3>
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

struct Node {
    int value = 0;
    sgcl::tracked_ptr<Node> next;
};

struct Holder {
    sgcl::array<sgcl::tracked_ptr<Node>, 3> nodes;      // three pointers inline, traced with the object
};

int main() {
    // A fixed set of roots on the stack, as one object
    sgcl::array<sgcl::tracked_ptr<Node>, 8> roots = {};
    for (int i : sgcl::range(8)) {
        roots[i] = sgcl::make_tracked<Node>();
        roots[i]->value = i;
        if (i) {
            roots[i]->next = roots[i - 1];          // a chain, rooted through the array
        }
    }

    // Inside a managed object: the pointers go with the object
    sgcl::tracked_ptr h = sgcl::make_tracked<Holder>();
    h->nodes[2] = roots[7];

    sgcl::tracked_ptr<Node> keep = roots[3];
    roots.fill(nullptr);                            // the chain lives on behind keep and h->nodes[2]
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    sgcl::collector::force_collect(true);

    // A table known at compile time, searched at compile time
    constexpr sgcl::array squares = {0, 1, 4, 9, 16, 25};
    static_assert(squares.binary_search(16) && squares.sorted_index_of(9) == 3);
    std::cout << "node behind keep " << keep->next->value << ", squares up to " << squares.max()
              << ", " << squares.count_of([](int x) { return x % 2 == 0; }) << " even\n";
    return keep->next->value == 2 && h->nodes[2]->value == 7 ? 0 : 1;
}
```

The output:

```
node behind keep 2, squares up to 25, 3 even
```

## See also

- [dynamic_array](dynamic_array.md): a count fixed at creation, in a managed buffer; [vector](vector.md): a buffer that grows
- [the mixins and the requirements](../core/mixin/README.md), [tracked_ptr](../core/tracked_ptr.md), [make_tracked](../core/make_tracked.md)
- [README: Containers](README.md#containers), [README: The rules](../core/README.md#the-rules)
