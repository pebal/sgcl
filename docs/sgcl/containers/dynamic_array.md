# sgcl::dynamic_array

```cpp
#include "sgcl/containers/dynamic_array.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class dynamic_array;                      // a managed buffer whose size is fixed when it is created, held by a tracked_ptr
}
```

`sgcl::dynamic_array<T>` is an array whose size is a value, fixed when the array is created and never changed after: Java's `new T[n]`, C#'s `T[]`, what `std::dynarray` was to be. A handle of two words — a `tracked_ptr` to the first element and the count — that lives on a stack or inside a managed object; the elements live in a buffer on the managed heap, which the collector reclaims once nothing refers to it. Copying copies the elements into a buffer of its own; moving passes the buffer on. It has the members of every range of the library ([the mixins](../core/mixin/README.md)) and, beyond them, only `fill` and `swap`: no capacity, no growth.

What it has that [vector](vector.md) has not: **the buffer never moves**. A pointer, an iterator or a [slice](../core/slice.md) to an element is valid for as long as the array is, not until the next `push_back`; threads may keep pointers to its slots; and a field of this type says in its type that it does not grow — the rings of [channel](../async/channel.md) and [broadcast](../async/broadcast.md) and the buckets of the concurrent containers are `dynamic_array`s. For `N` known at compile time, inline: [array](array.md).

## Rules

- A `dynamic_array` holds a `tracked_ptr`, so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../core/README.md#the-rules), 1).
- It destroys its elements itself, in its destructor and on assignment, wherever the handle dies, on a stack or in a sweep inside a dying managed object. The collector never destroys a buffer, it only frees one nothing refers to any more ([Containers](README.md#containers)).
- The buffer is referred to only through the pointer to its first element, the one the handle holds. A `tracked_ptr` may not address an element of it: an alias into it would keep nothing, and debug builds assert on the attempt ([The rules](../core/README.md#the-rules), 4). A raw pointer, a reference, an iterator or a slice of an element is valid until the handle is assigned over, moved from or destroyed.
- Iterators are raw pointers in a thin class (`std::contiguous_iterator`), cheap to copy, at home in any container.
- Thread safety is that of a `std::vector`: concurrent readers, or one writer, with the program's own synchronization.

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
dynamic_array() noexcept = default;
explicit dynamic_array(size_type count);
dynamic_array(size_type count, const T& value);
template<std::input_iterator InputIt> dynamic_array(InputIt first, InputIt last);
dynamic_array(std::initializer_list<T> ilist);
dynamic_array(const dynamic_array& other);
dynamic_array(dynamic_array&& other) noexcept;
```

The default constructor holds no buffer. `dynamic_array(count)` holds `count` value-initialized elements; for `tracked_ptr` elements the buffer is already zeroed, so they are null pointers without a constructor run. The range constructor takes the count from a forward range in advance; a single-pass range is collected into a `vector` first. A copy has a buffer of its own; a move takes the buffer over and leaves `other` empty. A `count` above `max_size()` throws `std::length_error`. An element constructor that throws takes the elements built before it with it and the array holds nothing.

```cpp
dynamic_array<int> zeros(5);                           // 0 0 0 0 0
dynamic_array<string> names(3, "n");              // "n" "n" "n"
dynamic_array<int> digits = {1, 2, 3};
vector src = {4, 5, 6, 7};
dynamic_array from(src.begin(), src.end());            // deduced: dynamic_array<int>
dynamic_array<int> taken = std::move(digits);          // digits is empty now
dynamic_array<tracked_ptr<int>> ptrs(10);          // ten null pointers, in a managed buffer
```

### Destructor

```cpp
~dynamic_array();
```

Destroys the elements, wherever the handle dies. The buffer is left to the collector, which frees it once nothing refers to it.

### operator=

```cpp
dynamic_array& operator=(const dynamic_array& other);
dynamic_array& operator=(dynamic_array&& other) noexcept;
dynamic_array& operator=(std::initializer_list<T> ilist);
```

Copy assignment assigns the elements in place when the sizes are equal, else builds a copy and swaps it in. Move assignment destroys the current elements and takes the other buffer over. Assignment from an initializer list builds a fresh array and swaps it in.

```cpp
dynamic_array<int> a = {1, 2, 3};
dynamic_array<int> b(3);
b = a;                  // same size: assigned in place, b keeps its buffer
b = {7, 8};             // a new buffer of two
a = std::move(b);       // a holds 7 8, b is empty
```

### at, operator[], front, back, data, as_slice

```cpp
reference at(size_type pos);  const_reference at(size_type pos) const;                       // out_of_range past the size
reference operator[](size_type pos) noexcept;  const_reference operator[](size_type pos) const noexcept;
reference front() noexcept;  const_reference front() const noexcept;                          // an element required
reference back() noexcept;  const_reference back() const noexcept;
T* data() noexcept;  const T* data() const noexcept;                                          // nullptr when there is no buffer
slice<T> as_slice() noexcept;  slice<const T> as_slice() const noexcept;                      // the elements, holding the buffer
slice<T> as_slice(size_type pos, size_type n = size_type(-1));                                // [pos, pos + n); and const
```

```cpp
dynamic_array<int> a(4, 1);
a.front() = 0;
a.back() = 9;
slice<int> all = a.as_slice();          // 0 1 1 9: valid for as long as a is, the buffer never moves
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

Contiguous iterators over the elements, raw pointers: `std::ranges` algorithms work. An iterator keeps nothing alive; an iterator dying in a frame nulls its word, so a temporary left behind does not root the buffer under the conservative stack scan.

### empty, size, max_size

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
size_type max_size() const noexcept;                    // PTRDIFF_MAX / sizeof(T)
```

### The mixins

`dynamic_array` carries [mixin::enumerable](../core/mixin/enumerable.md), [mixin::equatable](../core/mixin/equatable.md), [mixin::comparable](../core/mixin/comparable.md), [mixin::ordered](../core/mixin/ordered.md), [mixin::sequence](../core/mixin/sequence.md) and the contiguous category: everything a `vector` answers.

```cpp
dynamic_array<int> a = {5, 3, 9, 3};
assert(a.contains(9) && a.index_of(3) == 1 && a.min() == 3);
a.sort();                                        // 3 3 5 9
assert(a.is_sorted() && a.binary_search(5));
```

### fill, swap

```cpp
void fill(const T& value);
void swap(dynamic_array& other) noexcept;               // the handles are exchanged
friend void swap(dynamic_array& l, dynamic_array& r) noexcept;
```

```cpp
dynamic_array<int> a = {1, 2}, b = {3, 4, 5};
swap(a, b);                                     // a is 3 4 5, b is 1 2; no element moved
```

### Comparisons

`==` and `<=>` come with [mixin::equatable](../core/mixin/equatable.md) and [mixin::comparable](../core/mixin/comparable.md): element-wise, `<=>` lexicographical with the synthesized three-way comparison, only for elements that compare.

### Deduction guide

```cpp
template<std::input_iterator InputIt>
dynamic_array(InputIt, InputIt) -> dynamic_array<std::iter_value_t<InputIt>>;
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <numeric>

using namespace sgcl;

struct Holder {
    dynamic_array<int> values;                  // a managed buffer, one handle inside the object
};

int main() {
    // A buffer sized at creation: the elements are on the managed heap
    dynamic_array<int> squares(8);
    for (size_t i : range(squares.size())) {
        squares[i] = int(i * i);
    }
    int* third = &squares[3];                         // stays valid: the buffer never moves
    dynamic_array copy = squares;               // a deep copy, a buffer of its own
    copy.reverse();

    // Inside a managed object: the buffer goes with the object
    tracked_ptr h = make_tracked<Holder>();
    h->values = std::move(squares);                 // the buffer is handed over, squares is empty
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    std::cout << "sum of squares " << std::accumulate(h->values.begin(), h->values.end(), 0)
              << ", reversed copy starts with " << copy.front() << ", third " << *third << "\n";
    return *third == 9 && squares.empty() ? 0 : 1;
}
```

The output:

```
sum of squares 140, reversed copy starts with 49, third 9
```

## See also

- [array](array.md) for `N` in the type, [vector](vector.md) for a buffer that grows, [slice](../core/slice.md) for a piece of the buffer that holds it
- [the mixins and the requirements](../core/mixin/README.md), [tracked_ptr](../core/tracked_ptr.md), [make_tracked](../core/make_tracked.md)
- [README: Containers](README.md#containers), [README: The rules](../core/README.md#the-rules), [README: Pointer aliases](../core/README.md#pointer-aliases)
