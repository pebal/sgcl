# Sgcl::Array

```cpp
#include "sgcl/Sgcl/Containers/Array.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, size_t N = sgcl::dynamic_extent>
    class Array;                             // Array<T, N>: the elements inline
    template<class T>
    class Array<T, sgcl::dynamic_extent>;    // Array<T>: a managed buffer sized at creation, held by a Ptr
}
```

The same class in the `sgcl` interface: [array](../../containers/array.md).

`Array` is two containers under one name, told apart by `N`.

`Array<T, N>` is `std::array`: the `N` elements inline, the tuple interface (`std::tuple_size`, `std::tuple_element`, `Get<I>`, structured bindings), and no memory of its own. It exists so that a fixed set of `Ptr`s can be written as one object, `Array<Ptr<T>, 4> roots;`, that lives wherever its elements may and costs nothing beyond them. `Array<T, 0>` is empty with the same interface.

`Array<T>` (no `N`) is a buffer on the managed heap whose size is fixed when it is created, behind a handle of two words: a `Ptr` to the first element and the count. It is the cheapest managed sequence: no capacity, no growth, no modifiers beyond `Fill` and `Swap`. Copying copies the elements into a buffer of its own; moving passes the buffer on. `std` has no direct counterpart; it is a `std::vector` that never changes size, or a `std::unique_ptr<T[]>` that knows its size and is collected.

## Rules

- `Array<T, N>` lives wherever its elements may: when `T` is or contains a `Ptr`, on a stack or inside a managed object only ([The rules](../../core/README.md#the-rules), 1). `Array<T>` holds a `Ptr` itself, so the same holds for it whatever `T` is: never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame.
- `Array<T>` destroys its elements itself, in its destructor and on assignment, wherever the handle dies, on a stack or in a sweep inside a dying managed object. The collector never destroys a buffer, it only frees one nothing refers to any more ([Containers](../../containers/README.md#containers)).
- The buffer of an `Array<T>` is referred to only through the pointer to its first element, the one the handle holds. A `Ptr` may not address an element of it: an alias into it would keep nothing, and debug builds assert on the attempt ([The rules](../../core/README.md#the-rules), 4). A raw pointer, a reference or an iterator to an element is valid until the handle is assigned over, moved from or destroyed, as with `std`.
- Iterators (the free `begin`/`end`) are raw pointers in a thin class (`std::contiguous_iterator`), cheap to copy, at home in any container.
- Thread safety is that of a `std::array`: concurrent readers, or one writer, with the program's own synchronization.

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::array<T, N>;
using SizeType = size_t;
static constexpr bool IsDynamic;         // N left out
```

The same in the two forms.

### Constructors

```cpp
Array();                                                        // N value-initialized elements; the dynamic one holds no buffer
explicit Array(SizeType count) requires IsDynamic;
Array(SizeType count, const T& value) requires IsDynamic;
template<std::input_iterator It> Array(It first, It last) requires IsDynamic;
Array(std::initializer_list<T> il);                             // the first N for a fixed N; all of them for the dynamic one
explicit Array(InnerType a);
Array(const Array& other);
Array(Array&& other) noexcept;
```

For `Array<T, N>`: the default constructor value-initializes the `N` elements (null pointers for `Ptr`s), the initializer list fills the first ones. For `Array<T>`: the default constructor holds no buffer; `Array(count)` holds `count` value-initialized elements, and for `Ptr` elements the buffer is already zeroed, so they are null pointers without a constructor run; the range constructor takes the count from a forward range in advance, and a single-pass range is collected first. A copy has a buffer of its own; a move takes the buffer over and leaves `other` empty.

```cpp
Array<int, 3> a = {1, 2, 3};                    // three, inline
Array<Ptr<int>, 2> roots;              // two null pointers, on the stack
roots[0] = Make<int>(5);
Array<int> zeros(5);                            // 0 0 0 0 0, in a managed buffer
Array<String> names(3, "n");               // "n" "n" "n"
Array<int> digits = {1, 2, 3};
List src = {4, 5, 6, 7};
Array from(begin(src), end(src));             // deduced: Array<int>
Array<int> taken = std::move(digits);           // digits is empty now
Array<Ptr<int>> ptrs(10);              // ten null pointers, in a managed buffer
```

### Destructor

```cpp
~Array();
```

`Array<T>` destroys the elements, wherever the handle dies; the buffer is left to the collector, which frees it once nothing refers to it. `Array<T, N>` destroys its elements as any object destroys its members.

### operator=

```cpp
Array& operator=(const Array& other);
Array& operator=(Array&& other) noexcept;
```

For `Array<T>`: copy assignment assigns the elements in place when the sizes are equal, else builds a copy and swaps it in; move assignment destroys the current elements and takes the other buffer over. `Array<T, N>` copies or moves the elements one by one.

```cpp
Array<int> a = {1, 2, 3};
Array<int> b(3);
b = a;                  // same size: assigned in place, b keeps its buffer
a = std::move(b);       // a holds 1 2 3, b is empty
```

### operator[]

```cpp
T& operator[](SizeType i) noexcept;
const T& operator[](SizeType i) const noexcept;
```

The element at `i`, unchecked.

```cpp
Array<int> a = {10, 20, 30};
a[1] = 25;
```

### First, Last, Data

```cpp
T& First() noexcept;
const T& First() const noexcept;
T& Last() noexcept;
const T& Last() const noexcept;
T* Data() noexcept;                                           // nullptr when there is no buffer, or N == 0
const T* Data() const noexcept;
```

`First` and `Last` require an element. `Data()` is the buffer as a plain pointer, valid under the same conditions as any pointer to an element.

```cpp
Array<int> a(4, 1);
a.First() = 0;
a.Last() = 9;
std::span<int> view(a.Data(), a.Count());   // 0 1 1 9
```

### begin, end

```cpp
auto begin(Array&) noexcept;    auto begin(const Array&) noexcept;   // free functions
auto end(Array&) noexcept;      auto end(const Array&) noexcept;
```

Contiguous iterators over the elements, raw pointers, as free functions: a range-for and the `std::ranges` algorithms work on both forms. An iterator keeps nothing alive; an iterator dying in a frame nulls its word, so a temporary left behind does not root the buffer under the conservative stack scan.

```cpp
Array<int> a = {3, 1, 2};
std::ranges::sort(a);                          // 1 2 3
Array<int, 3> b = {3, 1, 2};
std::ranges::sort(b);                          // 1 2 3
int last = *(end(a) - 1);                      // 3
```

### IsEmpty, Count

```cpp
bool IsEmpty() const noexcept;    // Array<T, N>: N == 0; Array<T>: no elements
SizeType Count() const noexcept;  // N, or the count fixed at creation
```

### Algorithms

```cpp
bool Contains(const auto& value) const;   // anything an element compares with
SizeType IndexOf(const auto& value) const;                // NoIndex when none
SizeType LastIndexOf(const auto& value) const;
template<class Pred> SizeType FindIndex(Pred pred) const;
template<class Pred> T* Find(Pred pred) noexcept;      // null when none; and const
template<class Pred> bool Exists(Pred pred) const;
template<class Pred> bool All(Pred pred) const;
template<class Pred> SizeType CountOf(Pred pred) const;
template<class F> void ForEach(F f);                   // and const
const T& Min() const;  template<class Compare> const T& Min(Compare cmp) const;   // undefined when empty, as First()
const T& Max() const;  template<class Compare> const T& Max(Compare cmp) const;
void Fill(const auto& value);
void Reverse() noexcept;
void Sort();  template<class Compare> void Sort(Compare cmp);
bool IsSorted() const;  template<class Compare> bool IsSorted(Compare cmp) const;
SizeType BinarySearch(const auto& value) const;   // on a sorted sequence: the value's position, NoIndex when it is not there; and with a comparator
auto LowerBound(const auto& value);  auto UpperBound(const auto& value);   // the first position not less than the value, the first greater; and const, and with a comparator
```

The members of [`MSequence`](MSequence.md), the algorithms every sequence of the interface has as members, on both forms of the array, so that `a.Sort()` reads as `a[0]` does. A linear search from the front; `IndexOf`, `LastIndexOf` and `FindIndex` give the position, or `NoIndex` when nothing matches; `Find` the element the predicate accepts first, or null. `Fill` is the one `std::array` has.

```cpp
Array a = {5, 3, 9, 3};
assert(a.Contains(9) && a.IndexOf(3) == 1 && a.LastIndexOf(3) == 3 && a.IndexOf(7) == NoIndex);
assert(a.FindIndex([](int x) { return x > 4; }) == 0 && a.Exists([](int x) { return x == 9; }) && !a.All([](int x) { return x > 3; }));
if (int* big = a.Find([](int x) { return x > 8; })) {
    *big = 8;
}
assert(a.CountOf([](int x) { return x == 3; }) == 2 && a.Min() == 3 && a.Max() == 8);
a.Sort();                                        // 3 3 5 8
assert(a.IsSorted());
a.Sort([](int a, int b) { return a > b; });     // 8 5 3 3
a.Reverse();                                     // 3 3 5 8
int sum = 0;
a.ForEach([&](int x) { sum += x; });         // 19
a.Fill(0);
```

### Swap

```cpp
void Swap(Array& other) noexcept;               // Array<T, N>: element by element; Array<T>: the handles are exchanged
void swap(Array& l, Array& r) noexcept;         // free function
```

```cpp
Array<int> a = {1, 2}, b = {3, 4, 5};
swap(a, b);                                     // a is 3 4 5, b is 1 2; no element moved
```

### Comparisons

```cpp
bool operator==(const Array& l, const Array& r);    // Array<T>: sizes first
auto operator<=>(const Array& l, const Array& r);
```

Element-wise, as for `std::array`: `<=>` is lexicographical with the synthesized three-way comparison (`<=>` of `T` when it has one, else a `std::weak_ordering` built from `<`). Two `Array<T, 0>` are always equal.

```cpp
Array<double> e = {1.0, 2.0}, f = {1.0, 3.0};
bool less = e < f;                                    // true
bool equal = (e <=> f) == std::partial_ordering::less;   // true: double's ordering
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The array inside, as its own type.

### Deduction guides

```cpp
template<class T, class... U>
Array(T, U...) -> Array<T, 1 + sizeof...(U)>;                  // all of the same type

template<std::input_iterator It>
Array(It, It) -> Array<std::iter_value_t<It>>;                 // Array<T>
```

```cpp
Array fixed = {1.5, 2.5};                         // Array<double, 2>
List src = {1, 2};
Array dynamic(begin(src), end(src));            // Array<int>
```

### Get, ToArray, the tuple interface

```cpp
template<size_t I, class T, size_t N> T& Get(Array<T, N>& a) noexcept;
template<size_t I, class T, size_t N> const T& Get(const Array<T, N>& a) noexcept;
template<size_t I, class T, size_t N> T&& Get(Array<T, N>&& a) noexcept;

template<class T, size_t N> Array<std::remove_cv_t<T>, N> ToArray(T (&a)[N]);
template<class T, size_t N> Array<std::remove_cv_t<T>, N> ToArray(T (&&a)[N]);

namespace std {
    template<class T, size_t N> struct tuple_size<Array<T, N>>;        // N
    template<size_t I, class T, size_t N> struct tuple_element<I, Array<T, N>>;   // T
}
```

For `Array<T, N>` only: `Get<I>` is the `I`-th element with a `static_assert` on the range, `ToArray` builds an array from a built-in array (copying or moving the elements), and the `std` specializations make structured bindings work.

```cpp
Array<int, 3> a = {1, 2, 3};
auto [x, y, z] = a;                          // structured bindings
Get<1>(a) = 20;
auto b = ToArray({3, 2, 1});        // Array<int, 3>
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <algorithm>
#include <iostream>
#include <numeric>

struct Node {
    int value = 0;
    Ptr<Node> next;
};

struct Holder {
    Array<int> values;                       // a managed buffer, one handle inside the object
    Array<Ptr<Node>, 3> nodes;      // three pointers inline, traced with the object
};

int main() {
    // A fixed set of roots on the stack, as one object
    Array<Ptr<Node>, 8> roots;
    for (int i : Range(8)) {
        roots[i] = Make<Node>();
        roots[i]->value = i;
        if (i) {
            roots[i]->next = roots[i - 1];          // a chain, rooted through the array
        }
    }

    // A buffer sized at creation: the elements are on the managed heap
    Array<int> squares(8);
    for (size_t i : Range(squares.Count())) {
        squares[i] = int(i * i);
    }
    Array copy = squares;                    // a deep copy, a buffer of its own
    copy.Reverse();

    // Inside a managed object: the buffer and the pointers go with the object
    Ptr h = Make<Holder>();
    h->values = std::move(squares);                 // the buffer is handed over, squares is empty
    h->nodes[2] = roots[7];

    Ptr<Node> keep = roots[3];
    roots.Fill(nullptr);                            // the chain lives on behind keep and h->nodes[2]
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << "sum of squares " << std::accumulate(begin(h->values), end(h->values), 0)
              << ", reversed copy starts with " << copy.First()
              << ", node behind keep " << keep->next->value << "\n";
    return keep->next->value == 2 && h->nodes[2]->value == 7 && squares.IsEmpty() ? 0 : 1;
}
```

The output:

```
sum of squares 140, reversed copy starts with 49, node behind keep 2
```

## See also

- [Ptr](../Core/Ptr.md), [Make](../Core/Make.md)
- [List](List.md) for a buffer that grows
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules), [README: Pointer aliases](../../core/README.md#pointer-aliases)
