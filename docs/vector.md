# sgcl::vector

```cpp
#include "sgcl/vector.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T, template<class> class Ptr = tracked_ptr>
    class vector;
}
```

`sgcl::vector<T>` is `std::vector` over a buffer on the managed heap. The interface is the one of `std::vector` (constructors, `assign`, element access, contiguous iterators, capacity, modifiers, three-way comparison, `std::erase`/`std::erase_if`, a deduction guide from an iterator pair), and so is the behaviour: elements are constructed and destroyed one at a time, an explicit removal destroys them at once, a reallocation moves them and destroys the moved-from ones, `clear()` keeps the capacity.

`Ptr`, the last parameter, is the kind of the word by which the container holds its memory: `tracked_ptr` by default, so that the container lives where a `tracked_ptr` may (on a stack or inside a managed object), or [`gc::tracked_ptr`](gc/tracked_ptr.md), so that it lives anywhere, at the cost of a `gc::tracked_ptr` on each access to that word; `gc::vector` ([gc/gc.h](README.md#the-gc-namespace)) names the latter. The nodes and buffers are the same managed objects either way, and the elements are a choice apart; an element type that names a `tracked_type` (`gc::tracked_ptr<T>` names `sgcl::tracked_ptr<T>`) is stored as that type, one word in the same mode, and handed out as the type it was given, so a container of `gc::tracked_ptr`s costs what one of `sgcl::tracked_ptr`s does ([the gc namespace](README.md#the-gc-namespace)).

What differs is where the memory lives and what the object is. The vector object is three words: a `tracked_ptr` to the first element of the buffer, the count and the capacity. The buffer is a managed array with a header of its own; the collector never destroys a buffer, it only frees one nothing refers to any more (the buffer a reallocation abandoned, the buffer of a vector that is gone). A `sgcl::vector<tracked_ptr<T>>` is therefore the managed form of a vector of pointers: its elements are traced, and it may hold cycles like any other managed object. `vector<bool>` is a plain vector of `bool`; elements aligned beyond 16 bytes are not supported in buffers.

## Rules

- A vector holds a `tracked_ptr`, so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../README.md#the-rules), 1).
- The elements are destroyed by the vector itself, exactly when `std::vector` does: on removal (`erase`, `pop_back`, `clear`, `resize`, `assign`), on a reallocation (the moved-from elements), and in the destructor, wherever that runs, on a stack or in a sweep inside a dying managed object ([Containers](../README.md#containers)).
- A buffer is referred to only through the pointer to its first element, the one the vector holds. A `tracked_ptr` may not address an element of the buffer: an alias into it would keep nothing, and debug builds assert on the attempt ([The rules](../README.md#the-rules), 4). A raw pointer, a reference or an iterator to an element is valid exactly as long as with `std::vector`: until the element is removed, the vector reallocates, or the vector is destroyed.
- Iterators are raw pointers in a thin class (`std::contiguous_iterator`), cheap to copy, and may live anywhere, in a `std::vector` too.
- Thread safety is that of `std::vector`: concurrent readers, or one writer, with the program's own synchronization. The collector never waits for a mutator and never touches a buffer a vector still holds.
- The cost of `push_back`: a compare of two words of the vector object, a store of the element and a store of the count; the growth is a cold call. The old buffer of a reallocation is collected, not freed at once, so the capacity doubles to halve what waits ([Benchmarks: Containers](../README.md#containers-1): 2.7 ns per `push_back` against 1.6 ns for `std::vector`).

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

### Constructors

```cpp
vector() noexcept = default;
explicit vector(size_type count);
vector(size_type count, const T& value);
template<std::input_iterator InputIt> vector(InputIt first, InputIt last);
vector(std::initializer_list<T> ilist);
vector(const vector& other);
vector(vector&& other) noexcept;
```

The default constructor allocates nothing. `vector(count)` holds `count` value-initialized elements; for `tracked_ptr` elements the buffer is already zeroed, so they are null pointers without a constructor run. The range constructor takes the count from a forward range in advance and appends a single-pass range element by element. A copy has a buffer of its own; a move takes the buffer over and leaves `other` empty. An element constructor that throws takes the elements built before it with it and the vector holds nothing.

```cpp
gc::vector<int> zeros(4);                         // 0 0 0 0
gc::vector<std::string> words(2, "x");            // "x" "x"
gc::vector<int> digits = {1, 2, 3};
gc::vector copy(digits.begin(), digits.end());    // deduced: gc::vector<int>
gc::vector<int> taken = std::move(digits);        // digits is empty now
```

### Destructor

```cpp
~vector();
```

Destroys the elements, wherever the vector dies: on a stack, or in a sweep inside a managed object nobody refers to any more. The buffer itself is left to the collector, which frees it once nothing refers to it.

### operator=

```cpp
vector& operator=(const vector& other);
vector& operator=(vector&& other) noexcept;
vector& operator=(std::initializer_list<T> ilist);
```

Copy assignment is `assign(other.begin(), other.end())`: the elements are assigned over in place when the capacity suffices, else into a fresh buffer. Move assignment destroys the current elements and takes the other buffer over.

```cpp
gc::vector<int> a = {1, 2, 3};
gc::vector<int> b;
b = a;              // a copy, into b's own buffer
b = {4, 5};         // two elements; the buffer stays
b = std::move(a);   // a is empty
```

### assign

```cpp
void assign(size_type count, const T& value);
template<std::input_iterator InputIt> void assign(InputIt first, InputIt last);
void assign(std::initializer_list<T> ilist);
```

Replaces the contents. Within the capacity the elements are assigned over and the surplus destroyed, or the missing ones constructed; above it a fresh vector is built and swapped in. `value` may be an element of this vector. A single-pass range is cleared first and appended element by element.

```cpp
gc::vector<int> v = {1, 2, 3};
v.assign(2, 9);          // 9 9, the buffer stays
v.assign({7, 8, 9, 10});
```

### at, operator[]

```cpp
reference at(size_type pos);
const_reference at(size_type pos) const;
reference operator[](size_type pos) noexcept;
const_reference operator[](size_type pos) const noexcept;
```

`at` throws `std::out_of_range` for `pos >= size()`; `operator[]` does not check.

```cpp
gc::vector<int> v = {10, 20, 30};
v[1] = 25;
try { v.at(3); } catch (const std::out_of_range&) { /* 3 >= size() */ }
```

### front, back, data

```cpp
reference front() noexcept;
const_reference front() const noexcept;
reference back() noexcept;
const_reference back() const noexcept;
T* data() noexcept;
const T* data() const noexcept;
```

`front` and `back` require a non-empty vector. `data()` is the buffer as a plain pointer (null for a vector that has no buffer), valid under the same conditions as any pointer to an element.

```cpp
gc::vector<int> v = {1, 2, 3};
v.front() = 0;
v.back() = 9;
std::span<int> view(v.data(), v.size());   // a view: valid until v reallocates or dies
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

Contiguous iterators over the buffer: raw pointers, so `std::ranges` algorithms and `std::span` work on the vector. An iterator keeps nothing alive and is invalidated exactly when a `std::vector` iterator is. An iterator dying in a frame nulls its word, so a temporary left behind does not root the buffer under the conservative stack scan.

```cpp
gc::vector<int> v = {3, 1, 2};
std::ranges::sort(v);                          // 1 2 3
for (auto it = v.rbegin(); it != v.rend(); ++it) {
    *it *= 10;                                 // 10 20 30
}
```

### empty, size, max_size

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
size_type max_size() const noexcept;
```

`size()` is a word of the vector object; `max_size()` is `PTRDIFF_MAX / sizeof(T)`, and a request above it throws `std::length_error`.

### reserve, capacity, shrink_to_fit

```cpp
void reserve(size_type new_capacity);
size_type capacity() const noexcept;
void shrink_to_fit();
```

`reserve` moves the elements into a buffer of at least the requested capacity when it is above the current one; it never shrinks. `capacity()` is what the size class granted, which may be a little more than asked. `shrink_to_fit` replaces the buffer by one sized for the elements, and drops the buffer altogether when the vector is empty (`clear()` keeps it).

```cpp
gc::vector<int> v;
v.reserve(100);                      // one buffer for the pushes below
for (int i = 0; i < 100; ++i) {
    v.push_back(i);                  // no reallocation
}
v.clear();                           // capacity kept
v.shrink_to_fit();                   // empty: the buffer is dropped, capacity() == 0
```

### clear

```cpp
void clear() noexcept;
```

Destroys every element; the buffer and the capacity stay.

### insert, emplace

```cpp
iterator insert(const_iterator pos, const T& value);
iterator insert(const_iterator pos, T&& value);
iterator insert(const_iterator pos, size_type count, const T& value);
template<std::input_iterator InputIt> iterator insert(const_iterator pos, InputIt first, InputIt last);
iterator insert(const_iterator pos, std::initializer_list<T> ilist);
template<class... A> iterator emplace(const_iterator pos, A&&... a);
```

Inserts before `pos` and returns an iterator to the first inserted element (`pos` itself when nothing is inserted). Within the capacity the tail shifts up; above it the new elements are constructed into a fresh buffer first and the old elements move over after, so an argument that refers to an element of this vector stays valid throughout. A single-pass range is collected first, then inserted by moving. On an exception the vector is as it was.

```cpp
gc::vector<int> v = {1, 4};
v.insert(v.begin() + 1, 2);               // 1 2 4
v.insert(v.begin() + 2, 2, 3);            // 1 2 3 3 4
v.emplace(v.end(), 5);                    // 1 2 3 3 4 5
v.insert(v.begin(), {-1, 0});             // -1 0 1 2 3 3 4 5
v.insert(v.end(), v[0]);                  // an element of v: fine, copied before anything moves
```

### erase

```cpp
iterator erase(const_iterator pos);
iterator erase(const_iterator first, const_iterator last);
```

Shifts the tail down and destroys the last `count` elements, as `std::vector` does; returns the iterator to the element after the erased range. `erase(end())` and an empty range are no-ops.

```cpp
gc::vector<int> v = {1, 2, 3, 4, 5};
auto it = v.erase(v.begin());             // 2 3 4 5, it -> 2
v.erase(it + 1, v.end());                 // 2 3
```

### push_back, emplace_back

```cpp
void push_back(const T& value);
void push_back(T&& value);
template<class... A> reference emplace_back(A&&... a);
```

Appends an element and (`emplace_back`) returns a reference to it. The common case inlines into the caller's loop: a compare of the count against the capacity, the construction, a store of the count. The growth allocates a buffer of at least twice the capacity, constructs the new element there first (the arguments may refer to an element), then moves the others over and destroys the moved-from ones; the old buffer is the collector's.

```cpp
gc::vector<gc::tracked_ptr<int>> ptrs;
for (int i = 0; i < 1000; ++i) {
    ptrs.push_back(gc::make_tracked<int>(i));     // the buffers outgrown on the way are collected
}
int& last = *ptrs.emplace_back(gc::make_tracked<int>(1000));
```

### pop_back

```cpp
void pop_back();
```

Destroys the last element. The vector must not be empty.

### resize

```cpp
void resize(size_type count);
void resize(size_type count, const value_type& value);
```

Destroys the elements past `count`, or appends value-initialized elements (copies of `value`) up to it, reallocating when the capacity does not suffice.

```cpp
gc::vector<int> v = {1, 2, 3};
v.resize(5);          // 1 2 3 0 0
v.resize(2);          // 1 2
v.resize(4, 7);       // 1 2 7 7
```

### swap

```cpp
void swap(vector& other) noexcept;
friend void swap(vector& l, vector& r) noexcept;
```

Exchanges the buffers, counts and capacities; no element is touched.

### Comparisons

```cpp
friend bool operator==(const vector& l, const vector& r);
friend auto operator<=>(const vector& l, const vector& r);
```

Element-wise, as for `std::vector`: `==` compares sizes first, `<=>` is lexicographical with the synthesized three-way comparison (`<=>` of `T` when it has one, else a `std::weak_ordering` built from `<`), so `<`, `<=`, `>`, `>=` and `!=` follow.

```cpp
gc::vector<int> a = {1, 2}, b = {1, 3};
bool less = a < b;                  // true
bool same = a == b;                 // false
```

### Deduction guide

```cpp
template<std::input_iterator InputIt>
vector(InputIt, InputIt) -> vector<std::iter_value_t<InputIt>>;
```

```cpp
std::list<double> src = {1.5, 2.5};
gc::vector v(src.begin(), src.end());     // gc::vector<double>
```

### std::erase, std::erase_if

```cpp
namespace std {
    template<class T, class U> size_t erase(sgcl::vector<T>& v, const U& value);
    template<class T, class Pred> size_t erase_if(sgcl::vector<T>& v, Pred pred);
}
```

Remove every element equal to `value`, or satisfying `pred`, and return how many were removed.

```cpp
gc::vector<int> v = {1, 2, 2, 3, 4};
size_t twos = std::erase(v, 2);                                  // 2; v is 1 3 4
size_t big = std::erase_if(v, [](int x) { return x > 2; });      // 2; v is 1
```

### vector<unique_ptr<T>>

```cpp
template<typename T>
class vector<unique_ptr<T>> : public std::vector<unique_ptr<T>>;
```

A `unique_ptr` owns its object and needs no tracing, so a vector of them is a plain `std::vector` with the constructors and assignments of the base: it may live anywhere a `std::vector` may, and the objects die when their `unique_ptr` does.

```cpp
gc::vector<gc::unique_ptr<int>> owned;
owned.push_back(gc::make_tracked<int>(1));
owned.pop_back();                                  // the int is destroyed here, deterministically
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <algorithm>
#include <iostream>

struct Node {
    int value;
    gc::vector<gc::tracked_ptr<Node>> edges;       // a managed buffer of traced pointers: any graph
};

int main() {
    // A vector of values, on the stack: the buffer is on the managed heap
    gc::vector<int> numbers = {5, 3, 9, 1};
    numbers.push_back(7);
    std::ranges::sort(numbers);                     // contiguous iterators: 1 3 5 7 9
    std::erase_if(numbers, [](int x) { return x > 5; });        // 1 3 5

    // A graph with a cycle, its edges in vectors inside managed objects
    gc::tracked_ptr a = gc::make_tracked<Node>(1);
    gc::tracked_ptr b = gc::make_tracked<Node>(2);
    a->edges.push_back(b);
    b->edges.push_back(a);                          // a cycle: collected like anything else

    // A vector of pointers on the stack roots every node it holds
    gc::vector<gc::tracked_ptr<Node>> nodes;
    for (int i = 0; i < 100; ++i) {
        gc::tracked_ptr n = gc::make_tracked<Node>(i);
        n->edges.push_back(a);
        nodes.push_back(n);                         // the buffers outgrown on the way are collected
    }
    nodes.erase(nodes.begin(), nodes.begin() + 90); // the ten last nodes remain reachable
    a = b = nullptr;                                // the cycle is still reachable through nodes[0]->edges

    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    gc::collector::force_collect(true);
    std::cout << nodes.size() << " nodes kept, "
              << nodes.front()->edges.front()->edges.front()->value << " reachable through the cycle\n";
    std::cout << gc::collector::get_live_object_count() << " live objects\n";
    return numbers.size() == 3 && nodes.size() == 10 ? 0 : 1;
}
```

The vector `numbers` dies at the end of `main` and destroys its elements then; its buffer, and the buffers the pushes outgrew, are freed by the collector once nothing refers to them.

## See also

- [tracked_ptr](tracked_ptr.md), [unique_ptr](unique_ptr.md), [make_tracked](make_tracked.md)
- [array](array.md) for a buffer whose size is fixed at creation, [deque](deque.md) for cheap pushes at both ends
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules), [README: Stack roots](../README.md#stack-roots)
- `examples/example.cpp`, `examples/threads.cpp`
