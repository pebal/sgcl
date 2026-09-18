# sgcl::array

```cpp
#include "sgcl/containers/array.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    using std::dynamic_extent;

    template<class T, size_t N = dynamic_extent>
    struct array;                             // array<T, N>: the elements inline, an aggregate
    template<class T>
    struct array<T, dynamic_extent>;          // array<T>: a managed buffer sized at creation, held by a tracked_ptr
}
```

The same class in the `Sgcl` interface: [Array](../Sgcl/Containers/Array.md).

`sgcl::array` is two containers under one name, told apart by `N`.

`sgcl::array<T, N>` is `std::array`: an aggregate with the `N` elements inline, the tuple interface (`std::tuple_size`, `std::tuple_element`, `sgcl::get<I>`, structured bindings), `constexpr` throughout, and no memory of its own. It exists so that a fixed set of `tracked_ptr`s can be written as one object, `sgcl::array<sgcl::tracked_ptr<T>, 4> roots = {}`, that lives wherever its elements may and costs nothing beyond them. `array<T, 0>` is an empty aggregate with the same interface.

`sgcl::array<T>` (no `N`) is a buffer on the managed heap whose size is fixed when it is created, behind a handle of two words: a `tracked_ptr` to the first element and the count. It is the cheapest managed sequence: no capacity, no growth, no modifiers beyond `fill` and `swap`. Copying copies the elements into a buffer of its own; moving passes the buffer on. `std` has no direct counterpart; it is a `std::vector` that never changes size, or a `std::unique_ptr<T[]>` that knows its size and is collected.

## Rules

- `array<T, N>` lives wherever its elements may: when `T` is or contains a `tracked_ptr`, on a stack or inside a managed object only ([The rules](../core/README.md#the-rules), 1). `array<T>` holds a `tracked_ptr` itself, so the same holds for it whatever `T` is: never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame.
- `array<T>` destroys its elements itself, in its destructor and on assignment, wherever the handle dies, on a stack or in a sweep inside a dying managed object. The collector never destroys a buffer, it only frees one nothing refers to any more ([Containers](README.md#containers)).
- The buffer of an `array<T>` is referred to only through the pointer to its first element, the one the handle holds. A `tracked_ptr` may not address an element of it: an alias into it would keep nothing, and debug builds assert on the attempt ([The rules](../core/README.md#the-rules), 4). A raw pointer, a reference or an iterator to an element is valid until the handle is assigned over, moved from or destroyed, as with `std`.
- Iterators are raw pointers in a thin class (`std::contiguous_iterator`), cheap to copy, at home in any container.
- Thread safety is that of a `std::array`: concurrent readers, or one writer, with the program's own synchronization.

## Members

### Types

```cpp
using value_type = T;
using reference = T&;
using const_reference = const T&;
using pointer = T*;
using const_pointer = const T*;
using size_type = size_t;
using difference_type = ptrdiff_t;
using iterator = detail::ContiguousIterator<value_type>;          // std::contiguous_iterator
using const_iterator = detail::ContiguousIterator<const value_type>;
using reverse_iterator = std::reverse_iterator<iterator>;
using const_reverse_iterator = std::reverse_iterator<const_iterator>;
```

The same in the three forms (`array<T, N>`, `array<T, 0>`, `array<T>`).

### array<T, N>: the elements

```cpp
T elems[N];   // public, so that aggregate initialization applies
```

The only member of `array<T, N>`; `array<T, 0>` has none. Being public makes the class an aggregate: `sgcl::array<int, 3> a = {1, 2, 3}` and `sgcl::array<sgcl::tracked_ptr<Node>, 4> roots = {}` (four null pointers) are its constructors, and `sizeof(array<T, N>) == N * sizeof(T)`.

```cpp
sgcl::array<int, 3> a = {1, 2, 3};                           // aggregate initialization
sgcl::array<sgcl::tracked_ptr<int>, 2> roots = {};             // two null pointers, on the stack
roots[0] = sgcl::make_tracked<int>(5);
constexpr sgcl::array<int, 2> c = {4, 5};                    // constexpr: everything is
static_assert(c[1] == 5 && c.size() == 2);
```

### array<T>: constructors

```cpp
array() noexcept = default;
explicit array(size_type count);
array(size_type count, const T& value);
template<std::input_iterator InputIt> array(InputIt first, InputIt last);
array(std::initializer_list<T> ilist);
array(const array& other);
array(array&& other) noexcept;
```

The default constructor holds no buffer. `array(count)` holds `count` value-initialized elements; for `tracked_ptr` elements the buffer is already zeroed, so they are null pointers without a constructor run. The range constructor takes the count from a forward range in advance; a single-pass range is collected into a `vector` first. A copy has a buffer of its own; a move takes the buffer over and leaves `other` empty. A `count` above `max_size()` throws `std::length_error`. An element constructor that throws takes the elements built before it with it and the array holds nothing.

```cpp
sgcl::array<int> zeros(5);                               // 0 0 0 0 0
sgcl::array<sgcl::string> names(3, "n");                  // "n" "n" "n"
sgcl::array<int> digits = {1, 2, 3};
sgcl::vector src = {4, 5, 6, 7};
sgcl::array from(src.begin(), src.end());                // deduced: sgcl::array<int>
sgcl::array<int> taken = std::move(digits);              // digits is empty now
sgcl::array<sgcl::tracked_ptr<int>> ptrs(10);              // ten null pointers, in a managed buffer
```

### array<T>: destructor

```cpp
~array();
```

Destroys the elements, wherever the handle dies. The buffer is left to the collector, which frees it once nothing refers to it.

### array<T>: operator=

```cpp
array& operator=(const array& other);
array& operator=(array&& other) noexcept;
array& operator=(std::initializer_list<T> ilist);
```

Copy assignment assigns the elements in place when the sizes are equal, else builds a copy and swaps it in. Move assignment destroys the current elements and takes the other buffer over. Assignment from an initializer list builds a fresh array and swaps it in.

```cpp
sgcl::array<int> a = {1, 2, 3};
sgcl::array<int> b(3);
b = a;                  // same size: assigned in place, b keeps its buffer
b = {7, 8};             // a new buffer of two
a = std::move(b);       // a holds 7 8, b is empty
```

`array<T, N>` has the implicit copy and move of an aggregate: the elements are copied or moved one by one.

### at, operator[]

```cpp
constexpr reference at(size_type pos);                        // array<T, N>
constexpr const_reference at(size_type pos) const;
constexpr reference operator[](size_type pos) noexcept;
constexpr const_reference operator[](size_type pos) const noexcept;

reference at(size_type pos);                                  // array<T>
const_reference at(size_type pos) const;
reference operator[](size_type pos) noexcept;
const_reference operator[](size_type pos) const noexcept;
```

`at` throws `std::out_of_range` for `pos >= size()` (always, for `array<T, 0>`); `operator[]` does not check. `array<T, 0>` has no `operator[]`.

```cpp
sgcl::array<int> a = {10, 20, 30};
a[1] = 25;
try { a.at(3); } catch (const std::out_of_range&) { /* 3 >= size() */ }
```

### front, back, data

```cpp
constexpr reference front() noexcept;                         // array<T, N>, N > 0
constexpr const_reference front() const noexcept;
constexpr reference back() noexcept;
constexpr const_reference back() const noexcept;
constexpr T* data() noexcept;                                 // array<T, N>; nullptr for N == 0
constexpr const T* data() const noexcept;

reference front() noexcept;                                   // array<T>
const_reference front() const noexcept;
reference back() noexcept;
const_reference back() const noexcept;
T* data() noexcept;                                           // nullptr when there is no buffer
const T* data() const noexcept;
```

`front` and `back` require an element; `array<T, 0>` does not have them. `data()` is the buffer as a plain pointer, valid under the same conditions as any pointer to an element.

```cpp
sgcl::array<int> a(4, 1);
a.front() = 0;
a.back() = 9;
std::span<int> view(a.data(), a.size());   // 0 1 1 9
```

### Iterators

```cpp
iterator begin() noexcept;                      const_iterator begin() const noexcept;
const_iterator cbegin() const noexcept;
iterator end() noexcept;                        const_iterator end() const noexcept;
const_iterator cend() const noexcept;
reverse_iterator rbegin() noexcept;             const_reverse_iterator rbegin() const noexcept;
const_reverse_iterator crbegin() const noexcept;
reverse_iterator rend() noexcept;               const_reverse_iterator rend() const noexcept;
const_reverse_iterator crend() const noexcept;
```

All `constexpr` in `array<T, N>`. Contiguous iterators over the elements, raw pointers: `std::ranges` algorithms work on both forms. An iterator keeps nothing alive; an iterator dying in a frame nulls its word, so a temporary left behind does not root the buffer under the conservative stack scan. `array<T, 0>` has no `crbegin`/`crend`.

```cpp
sgcl::array<int> a = {3, 1, 2};
std::ranges::sort(a);                          // 1 2 3
sgcl::array<int, 3> b = {3, 1, 2};
std::ranges::sort(b);                          // 1 2 3
int last = *a.rbegin();                        // 3
```

### empty, size, max_size

```cpp
[[nodiscard]] constexpr bool empty() const noexcept;    // array<T, N>: N == 0
constexpr size_type size() const noexcept;              // N
constexpr size_type max_size() const noexcept;          // N

bool empty() const noexcept;                            // array<T>
size_type size() const noexcept;
size_type max_size() const noexcept;                    // PTRDIFF_MAX / sizeof(T)
```

### Algorithms

```cpp
bool contains(const auto& value) const;    // anything an element compares with
size_t index_of(const auto& value) const;                 // npos when none
size_t last_index_of(const auto& value) const;
template<class Pred> size_t find_index(Pred pred) const;
template<class Pred> T* find(Pred pred) noexcept;      // null when none; and const
template<class Pred> bool exists(Pred pred) const;
template<class Pred> bool all(Pred pred) const;
template<class Pred> size_t count_of(Pred pred) const;
template<class F> void for_each(F f);                  // and const
const T& min() const;  template<class Compare> const T& min(Compare cmp) const;   // undefined when empty, as front()
const T& max() const;  template<class Compare> const T& max(Compare cmp) const;
void fill(const auto& value);
void reverse() noexcept;
void sort();  template<class Compare> void sort(Compare cmp);
bool is_sorted() const;  template<class Compare> bool is_sorted(Compare cmp) const;
bool binary_search(const auto& value) const;  size_t sorted_index_of(const auto& value) const;   // on a sorted sequence: whether the value is there, its position (npos when not); and with a comparator
auto lower_bound(const auto& value);  auto upper_bound(const auto& value);   // the first position not less than the value, the first greater; and const, and with a comparator
```

The members of [`m_sequence`](m_sequence.md), the algorithms every sequence of the library has as members: the same set on `array<T>` and, as `constexpr` forwards, on the aggregate `array<T, N>`, so that `a.sort()` reads as `a[0]` does. A linear search from the front; `index_of`, `last_index_of` and `find_index` give the position, or `npos` when nothing matches; `find` the element the predicate accepts first, or null. `fill` is the one `std::array` has.

```cpp
sgcl::array a = {5, 3, 9, 3};
assert(a.contains(9) && a.index_of(3) == 1 && a.last_index_of(3) == 3 && a.index_of(7) == sgcl::npos);
assert(a.find_index([](int x) { return x > 4; }) == 0 && a.exists([](int x) { return x == 9; }) && !a.all([](int x) { return x > 3; }));
if (int* big = a.find([](int x) { return x > 8; })) {
    *big = 8;
}
assert(a.count_of([](int x) { return x == 3; }) == 2 && a.min() == 3 && a.max() == 8);
a.sort();                                        // 3 3 5 8
assert(a.is_sorted());
a.sort([](int a, int b) { return a > b; });     // 8 5 3 3
a.reverse();                                     // 3 3 5 8
int sum = 0;
a.for_each([&](int x) { sum += x; });         // 19
a.fill(0);
```

### swap

```cpp
constexpr void swap(array& other) noexcept(std::is_nothrow_swappable_v<T>);   // array<T, N>: element by element
friend constexpr void swap(array& l, array& r) noexcept(noexcept(l.swap(r)));

void swap(array& other) noexcept;               // array<T>: the handles are exchanged
friend void swap(array& l, array& r) noexcept;
```

```cpp
sgcl::array<int> a = {1, 2}, b = {3, 4, 5};
swap(a, b);                                     // a is 3 4 5, b is 1 2; no element moved
```

### Comparisons

```cpp
friend constexpr bool operator==(const array& l, const array& r);    // array<T, N>
friend constexpr auto operator<=>(const array& l, const array& r);
friend bool operator==(const array& l, const array& r);              // array<T>: sizes first
friend auto operator<=>(const array& l, const array& r);
```

Element-wise, as for `std::array`: `<=>` is lexicographical with the synthesized three-way comparison (`<=>` of `T` when it has one, else a `std::weak_ordering` built from `<`). Two `array<T, 0>` are always equal.

```cpp
sgcl::array<double> e = {1.0, 2.0}, f = {1.0, 3.0};
bool less = e < f;                                    // true
bool equal = (e <=> f) == std::partial_ordering::less;   // true: double's ordering
```

### Deduction guides

```cpp
template<class T, class... U>
array(T, U...) -> array<T, 1 + sizeof...(U)>;                  // all of the same type

template<std::input_iterator InputIt>
array(InputIt, InputIt) -> array<std::iter_value_t<InputIt>>;  // array<T>
```

```cpp
sgcl::array fixed = {1.5, 2.5};                         // sgcl::array<double, 2>
sgcl::vector src = {1, 2};
sgcl::array dynamic(src.begin(), src.end());            // sgcl::array<int>
```

### get, to_array, the tuple interface

```cpp
template<size_t I, class T, size_t N> constexpr T& get(array<T, N>& a) noexcept;
template<size_t I, class T, size_t N> constexpr const T& get(const array<T, N>& a) noexcept;
template<size_t I, class T, size_t N> constexpr T&& get(array<T, N>&& a) noexcept;

template<class T, size_t N> constexpr array<std::remove_cv_t<T>, N> to_array(T (&a)[N]);
template<class T, size_t N> constexpr array<std::remove_cv_t<T>, N> to_array(T (&&a)[N]);

namespace std {
    template<class T, size_t N> struct tuple_size<sgcl::array<T, N>>;        // N; array<T, N> only
    template<size_t I, class T, size_t N> struct tuple_element<I, sgcl::array<T, N>>;   // T; array<T, N> only
}
```

For `array<T, N>` only: `get<I>` is the `I`-th element with a `static_assert` on the range, `to_array` builds an array from a built-in array (copying or moving the elements), and the `std` specializations make structured bindings work.

```cpp
sgcl::array<int, 3> a = {1, 2, 3};
auto [x, y, z] = a;                          // structured bindings
sgcl::get<1>(a) = 20;
auto b = sgcl::to_array({3, 2, 1});            // sgcl::array<int, 3>
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <algorithm>
#include <iostream>
#include <numeric>

struct Node {
    int value = 0;
    sgcl::tracked_ptr<Node> next;
};

struct Holder {
    sgcl::array<int> values;                          // a managed buffer, one handle inside the object
    sgcl::array<sgcl::tracked_ptr<Node>, 3> nodes;      // three pointers inline, traced with the object
};

int main() {
    // A fixed set of roots on the stack, as one aggregate
    sgcl::array<sgcl::tracked_ptr<Node>, 8> roots = {};
    for (int i : sgcl::range(8)) {
        roots[i] = sgcl::make_tracked<Node>();
        roots[i]->value = i;
        if (i) {
            roots[i]->next = roots[i - 1];          // a chain, rooted through the array
        }
    }

    // A buffer sized at creation: the elements are on the managed heap
    sgcl::array<int> squares(8);
    for (size_t i : sgcl::range(squares.size())) {
        squares[i] = int(i * i);
    }
    sgcl::array copy = squares;                       // a deep copy, a buffer of its own
    copy.reverse();

    // Inside a managed object: the buffer and the pointers go with the object
    sgcl::tracked_ptr h = sgcl::make_tracked<Holder>();
    h->values = std::move(squares);                 // the buffer is handed over, squares is empty
    h->nodes[2] = roots[7];

    sgcl::tracked_ptr<Node> keep = roots[3];
    roots.fill(nullptr);                            // the chain lives on behind keep and h->nodes[2]
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    sgcl::collector::force_collect(true);
    std::cout << "sum of squares " << std::accumulate(h->values.begin(), h->values.end(), 0)
              << ", reversed copy starts with " << copy.front()
              << ", node behind keep " << keep->next->value << "\n";
    return keep->next->value == 2 && h->nodes[2]->value == 7 && squares.empty() ? 0 : 1;
}
```

The output:

```
sum of squares 140, reversed copy starts with 49, node behind keep 2
```

## See also

- [tracked_ptr](../core/tracked_ptr.md), [make_tracked](../core/make_tracked.md)
- [vector](vector.md) for a buffer that grows
- [README: Containers](README.md#containers), [README: The rules](../core/README.md#the-rules), [README: Pointer aliases](../core/README.md#pointer-aliases)
