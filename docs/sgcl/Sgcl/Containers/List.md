# Sgcl::List

```cpp
#include "sgcl/Sgcl/Containers/List.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class List;
    inline constexpr size_t NoIndex = SIZE_MAX;
}
```

The same class in the `sgcl` interface: [vector](../../containers/vector.md).

`List<T>` is `std::vector` over a buffer on the managed heap, with the names of a list: `Add`, `Insert`, `RemoveAt`, `Count`, `Contains`, `IndexOf`. The behaviour is `std::vector`'s: elements are constructed and destroyed one at a time, an explicit removal destroys them at once, a reallocation moves them and destroys the moved-from ones, `Clear()` keeps the capacity. Positions are unchecked; a search that finds nothing is `NoIndex`, a null pointer or `false`.

What differs from `std::vector` is where the memory lives and what the object is. The list object is three words: a `Ptr` to the first element of the buffer, the count and the capacity. The buffer is a managed array with a header of its own; the collector never destroys a buffer, it only frees one nothing refers to any more (the buffer a reallocation abandoned, the buffer of a list that is gone). A `List<Ptr<T>>` is therefore the managed form of a list of pointers: its elements are traced, and it may hold cycles like any other managed object. `List<bool>` is a plain list of `bool`; elements aligned beyond 16 bytes are not supported in buffers.

## Rules

- A list holds a `Ptr`, so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../../core/README.md#the-rules), 1).
- The elements are destroyed by the list itself, exactly when `std::vector` does: on removal (`RemoveAt`, `RemoveRange`, `RemoveLast`, `Remove`, `RemoveAll`, `Clear`, `Resize`, `Assign`), on a reallocation (the moved-from elements), and in the destructor, wherever that runs, on a stack or in a sweep inside a dying managed object ([Containers](../../containers/README.md#containers)).
- A buffer is referred to only through the pointer to its first element, the one the list holds. A `Ptr` may not address an element of the buffer: an alias into it would keep nothing, and debug builds assert on the attempt ([The rules](../../core/README.md#the-rules), 4). A raw pointer, a reference or an iterator to an element is valid exactly as long as with `std::vector`: until the element is removed, the list reallocates, or the list is destroyed.
- Iterators (the free `begin`/`end`) are raw pointers in a thin class (`std::contiguous_iterator`), cheap to copy, and may live anywhere, in a `std::vector` too.
- Thread safety is that of `std::vector`: concurrent readers, or one writer, with the program's own synchronization. The collector never waits for a mutator and never touches a buffer a list still holds.
- The cost of `Add`: a compare of two words of the list object, a store of the element and a store of the count; the growth is a cold call. The old buffer of a reallocation is collected, not freed at once, so the capacity doubles to halve what waits ([Benchmarks: Containers](../../containers/benchmarks.md#containers): 2.7 ns per push against 1.6 ns for `std::vector`).

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::vector<T>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // std::contiguous_iterator
using ConstIterator = InnerType::const_iterator;
```

### Constructors

```cpp
List() noexcept;
explicit List(SizeType count);
List(SizeType count, const T& value);
template<std::input_iterator It> List(It first, It last);
template<std::ranges::input_range R> explicit List(R&& r);   // from a range of what T is made of: the Pieces of a String, a view, another container (not a List: that is the copy)
List(std::initializer_list<T> il);
explicit List(InnerType v) noexcept;
List(const List& other);
List(List&& other) noexcept;
```

The default constructor allocates nothing. `List(count)` holds `count` value-initialized elements; for `Ptr` elements the buffer is already zeroed, so they are null pointers without a constructor run. The range constructor takes the count from a forward range in advance and appends a single-pass range element by element. A copy has a buffer of its own; a move takes the buffer over and leaves `other` empty. An element constructor that throws takes the elements built before it with it and the list holds nothing.

```cpp
List<int> zeros(4);                             // 0 0 0 0
List<String> words(2, "x");                // "x" "x"
List digits = {1, 2, 3};
List copy(begin(digits), end(digits));          // deduced: List<int>
List<int> taken = std::move(digits);            // digits is empty now
```

### Destructor

```cpp
~List();
```

Destroys the elements, wherever the list dies: on a stack, or in a sweep inside a managed object nobody refers to any more. The buffer itself is left to the collector, which frees it once nothing refers to it.

### operator=

```cpp
List& operator=(const List& other);
List& operator=(List&& other) noexcept;
List& operator=(std::initializer_list<T> il);
```

Copy assignment is `Assign(begin(other), end(other))`: the elements are assigned over in place when the capacity suffices, else into a fresh buffer. Move assignment destroys the current elements and takes the other buffer over.

```cpp
List a = {1, 2, 3};
List<int> b;
b = a;              // a copy, into b's own buffer
b = {4, 5};         // two elements; the buffer stays
b = std::move(a);   // a is empty
```

### Assign

```cpp
void Assign(SizeType count, const T& value);
template<std::input_iterator It> void Assign(It first, It last);
void Assign(std::initializer_list<T> il);
```

Replaces the contents. Within the capacity the elements are assigned over and the surplus destroyed, or the missing ones constructed; above it a fresh list is built and swapped in. `value` may be an element of this list. A single-pass range is cleared first and appended element by element.

```cpp
List v = {1, 2, 3};
v.Assign(2, 9);          // 9 9, the buffer stays
v.Assign({7, 8, 9, 10});
```

### operator[]

```cpp
T& operator[](SizeType i) noexcept;
const T& operator[](SizeType i) const noexcept;
```

The element at `i`, unchecked.

```cpp
List v = {10, 20, 30};
v[1] = 25;
```

### First, Last, Data

```cpp
T& First() noexcept;
const T& First() const noexcept;
T& Last() noexcept;
const T& Last() const noexcept;
T* Data() noexcept;
const T* Data() const noexcept;
```

`First` and `Last` require a non-empty list. `Data()` is the buffer as a plain pointer (null for a list that has no buffer), valid under the same conditions as any pointer to an element.

```cpp
List v = {1, 2, 3};
v.First() = 0;
v.Last() = 9;
std::span<int> view(v.Data(), v.Count());   // a view: valid until v reallocates or dies
```

### begin, end

```cpp
Iterator begin(List&) noexcept;             ConstIterator begin(const List&) noexcept;   // free functions
Iterator end(List&) noexcept;               ConstIterator end(const List&) noexcept;
```

Contiguous iterators over the buffer, as free functions so that a range-for and the `std` algorithms work on the list and the class keeps to its own names: raw pointers, so `std::ranges` algorithms and `std::span` work too. An iterator keeps nothing alive and is invalidated exactly when a `std::vector` iterator is. An iterator dying in a frame nulls its word, so a temporary left behind does not root the buffer under the conservative stack scan.

```cpp
List v = {3, 1, 2};
std::ranges::sort(v);                          // 1 2 3
for (int& x : v) {
    x *= 10;                                   // 10 20 30
}
```

### IsEmpty, Count

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
```

`Count()` is a word of the list object.

### Reserve, Capacity, Shrink

```cpp
void Reserve(SizeType n);
SizeType Capacity() const noexcept;
void Shrink();
```

`Reserve` moves the elements into a buffer of at least the requested capacity when it is above the current one; it never shrinks. `Capacity()` is what the size class granted, which may be a little more than asked. `Shrink` replaces the buffer by one sized for the elements, and drops the buffer altogether when the list is empty (`Clear()` keeps it).

```cpp
List<int> v;
v.Reserve(100);                      // one buffer for the adds below
for (int i : Range(100)) {
    v.Add(i);                        // no reallocation
}
v.Clear();                           // capacity kept
v.Shrink();                          // empty: the buffer is dropped, Capacity() == 0
```

### Clear

```cpp
void Clear() noexcept;
```

Destroys every element; the buffer and the capacity stay.

### Insert, InsertRange, EmplaceAt

```cpp
void Insert(SizeType i, const T& value);
void Insert(SizeType i, T&& value);
void Insert(SizeType i, SizeType count, const T& value);
void Insert(SizeType i, std::initializer_list<T> il);
template<std::ranges::input_range R> void InsertRange(SizeType i, R&& r);
template<class... A> T& EmplaceAt(SizeType i, A&&... a);
```

Inserts before position `i` (`Count()` for the end). Within the capacity the tail shifts up; above it the new elements are constructed into a fresh buffer first and the old elements move over after, so an argument that refers to an element of this list stays valid throughout. A single-pass range is collected first, then inserted by moving. On an exception the list is as it was when it reallocates, and within the capacity as long as what throws is a copy: nothing is ever alive and unaccounted, what was built above the old end is destroyed again and a copy that throws leaves its source intact. A move or an assignment of `T` that throws leaves the list consistent, every element destroyed exactly once, with the values and, after an assignment, the size changed, as `std::vector` does.

```cpp
List v = {1, 4};
v.Insert(1, 2);                           // 1 2 4
v.Insert(2, 2, 3);                        // 1 2 3 3 4
v.EmplaceAt(v.Count(), 5);                // 1 2 3 3 4 5
v.Insert(0, {-1, 0});                     // -1 0 1 2 3 3 4 5
v.Insert(v.Count(), v[0]);                // an element of v: fine, copied before anything moves
```

### RemoveAt, RemoveRange

```cpp
void RemoveAt(SizeType i);
void RemoveRange(SizeType i, SizeType n);
```

Shifts the tail down and destroys the last elements, as `std::vector` does. An empty range is a no-op.

```cpp
List v = {1, 2, 3, 4, 5};
v.RemoveAt(0);                            // 2 3 4 5
v.RemoveRange(1, 3);                      // 2
```

### Add, Emplace, AddRange

```cpp
void Add(const T& value);
void Add(T&& value);
template<class... A> T& Emplace(A&&... a);
template<std::ranges::input_range R> void AddRange(R&& r);
void AddRange(std::initializer_list<T> il);
```

Appends an element and (`Emplace`) returns a reference to it; `AddRange` appends every element of a range. The common case inlines into the caller's loop: a compare of the count against the capacity, the construction, a store of the count. The growth allocates a buffer of at least twice the capacity, constructs the new element there first (the arguments may refer to an element), then moves the others over and destroys the moved-from ones; the old buffer is the collector's.

```cpp
List<Ptr<int>> ptrs;
for (int i : Range(1000)) {
    ptrs.Add(Make<int>(i));     // the buffers outgrown on the way are collected
}
int& last = *ptrs.Emplace(Make<int>(1000));
ptrs.AddRange({Make<int>(1001), Make<int>(1002)});
```

### RemoveLast

```cpp
void RemoveLast() noexcept;
```

Destroys the last element. The list must not be empty.

### Resize

```cpp
void Resize(SizeType n);
void Resize(SizeType n, const T& value);
```

Destroys the elements past `n`, or appends value-initialized elements (copies of `value`) up to it, reallocating when the capacity does not suffice. The buffer then grows geometrically, as an `Add` grows it, so `Resize(Count() + 1)` repeated reallocates as rarely as adds do. A constructor that throws destroys the elements appended before it and leaves the count as it was.

```cpp
List v = {1, 2, 3};
v.Resize(5);          // 1 2 3 0 0
v.Resize(2);          // 1 2
v.Resize(4, 7);       // 1 2 7 7
```

### Remove, RemoveAll

```cpp
bool Remove(const T& value);
template<class U> SizeType RemoveAll(const U& value);
template<class Pred> SizeType RemoveAll(Pred pred);
```

`Remove` removes the first element equal to `value` and says whether there was one; `RemoveAll` removes every element equal to `value`, or every one satisfying `pred`, and returns how many were removed.

```cpp
List v = {1, 2, 2, 3, 4};
bool one = v.Remove(1);                                          // true; v is 2 2 3 4
size_t twos = v.RemoveAll(2);                                    // 2; v is 3 4
size_t big = v.RemoveAll([](int x) { return x > 3; });           // 1; v is 3
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

The members of [`MSequence`](MSequence.md), the mixin every sequence of the interface carries: the algorithms of `<algorithm>` as members, so that `v.Sort()` reads as `v.Add(x)` does. A linear search from the front (`LastIndexOf` walks the whole sequence); `IndexOf`, `LastIndexOf` and `FindIndex` give the position, or `NoIndex` when nothing matches; `Find` the element the predicate accepts first, or null. `Reverse` and `Sort` are `std::reverse` and `std::sort` over the buffer.

```cpp
List v = {5, 3, 9, 3};
assert(v.Contains(9) && v.IndexOf(3) == 1 && v.LastIndexOf(3) == 3 && v.IndexOf(7) == NoIndex);
assert(v.FindIndex([](int x) { return x > 4; }) == 0 && v.Exists([](int x) { return x == 9; }) && !v.All([](int x) { return x > 3; }));
if (int* big = v.Find([](int x) { return x > 8; })) {
    *big = 8;
}
assert(v.CountOf([](int x) { return x == 3; }) == 2 && v.Min() == 3 && v.Max() == 8);
v.Sort();                                        // 3 3 5 8
assert(v.IsSorted());
v.Sort([](int a, int b) { return a > b; });     // 8 5 3 3
v.Reverse();                                     // 3 3 5 8
int sum = 0;
v.ForEach([&](int x) { sum += x; });         // 19
v.Fill(0);
```

### Swap

```cpp
void Swap(List& other) noexcept;
void swap(List& l, List& r) noexcept;               // free function
```

Exchanges the buffers, counts and capacities; no element is touched.

### Comparisons

```cpp
bool operator==(const List& l, const List& r);
auto operator<=>(const List& l, const List& r);
```

Element-wise, as for `std::vector`: `==` compares sizes first, `<=>` is lexicographical with the synthesized three-way comparison (`<=>` of `T` when it has one, else a `std::weak_ordering` built from `<`), so `<`, `<=`, `>`, `>=` and `!=` follow.

```cpp
List<int> a = {1, 2}, b = {1, 3};
bool less = a < b;                  // true
bool same = a == b;                 // false
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The vector inside, as its own type: for what the list's names leave out.

### Deduction guide

```cpp
template<std::input_iterator It>
List(It, It) -> List<std::iter_value_t<It>>;
```

```cpp
std::list<double> src = {1.5, 2.5};
List v(src.begin(), src.end());     // List<double>
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <algorithm>
#include <iostream>

struct Node {
    int value;
    List<Ptr<Node>> edges;       // a managed buffer of traced pointers: any graph
};

int main() {
    // A list of values, on the stack: the buffer is on the managed heap
    List numbers = {5, 3, 9, 1};
    numbers.Add(7);
    numbers.Sort();                                 // 1 3 5 7 9
    numbers.RemoveAll([](int x) { return x > 5; }); // 1 3 5

    // A graph with a cycle, its edges in lists inside managed objects
    Ptr a = Make<Node>(1);
    Ptr b = Make<Node>(2);
    a->edges.Add(b);
    b->edges.Add(a);                                // a cycle: collected like anything else

    // A list of pointers on the stack roots every node it holds
    List<Ptr<Node>> nodes;
    for (int i : Range(100)) {
        Ptr n = Make<Node>(i);
        n->edges.Add(a);
        nodes.Add(n);                               // the buffers outgrown on the way are collected
    }
    nodes.RemoveRange(0, 90);                       // the ten last nodes remain reachable
    a = b = nullptr;                                // the cycle is still reachable through nodes[0]->edges

    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << nodes.Count() << " nodes kept, "
              << nodes.First()->edges.First()->edges.First()->value << " reachable through the cycle\n";
    std::cout << Collector::LiveObjectCount() << " live objects\n";
    return numbers.Count() == 3 && nodes.Count() == 10 ? 0 : 1;
}
```

The output:

```
10 nodes kept, 2 reachable through the cycle
26 live objects
```

The list `numbers` dies at the end of `main` and destroys its elements then; its buffer, and the buffers the adds outgrew, are freed by the collector once nothing refers to them.

## See also

- [Ptr](../Core/Ptr.md), [UniquePtr](../Core/UniquePtr.md), [Make](../Core/Make.md)
- [Array](Array.md) for a buffer whose size is fixed at creation, [Deque](Deque.md) for cheap adds at both ends
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules), [README: Stack roots](../../../garbage_collector/overview.md#stack-roots)
