# Sgcl::Deque

```cpp
#include "sgcl/Sgcl/Containers/Deque.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class Deque;
}
```

The same class in the `sgcl` interface: [deque](../../containers/deque.md).

`Deque<T>` is `std::deque` over managed memory, with the names of the interface (`AddFirst`, `AddLast`, `RemoveFirst`, `RemoveLast`, `Insert`, `RemoveAt`, `Count`). The behaviour is `std::deque`'s: elements are constructed at insertion and destroyed at removal, references stay valid across an add or a removal at either end, iterators do not.

What differs is where the memory lives. The elements live in blocks on the managed heap (a block holds as many elements as fit in 4 KB, rounded down to a power of two, at least one), addressed through a managed array of block pointers, the map; the deque object is four words (the map, its size, the index of the first element, the count). A map that is outgrown is replaced by a fresh one, never shifted in place. A block emptied by removals stays in the map as the spare block of its end, so a window of elements travelling through the deque allocates no blocks; every block goes when the deque becomes empty. Nothing is ever freed by hand: the blocks and the maps the deque lets go of are reclaimed by the collector once nothing refers to them. A `Deque<Ptr<T>>` is the managed form of a deque of pointers: its elements are traced, and it may hold cycles like any other managed object.

## Rules

- A deque holds a `Ptr` (the map), so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../../core/README.md#the-rules), 1).
- The deque destroys its elements itself, exactly when `std::deque` does: on removal (`RemoveAt`, `RemoveRange`, `RemoveLast`, `RemoveFirst`, `Remove`, `RemoveAll`, `Clear`, `Resize`, `Assign`) and in the destructor. The one exception is a deque dying in a sweep, inside a managed object nobody refers to any more: its blocks are garbage of the same sweep, and each destroys the elements it still holds when the sweep reaches it ([Containers](../../containers/README.md#containers)).
- A `Ptr` may not address an element of a deque ([The rules](../../core/README.md#the-rules), 4): elements are reached through the deque, its iterators, references and raw pointers, valid exactly as long as with `std::deque` (a reference survives an insertion or removal at either end; any other insertion or removal invalidates everything). An invalid iterator must not be used, as in `std`.
- Iterators (the free `begin`/`end`) are raw (a pointer to the map, an index, a pointer to the element), cheap to copy, and may live anywhere, in a `std::vector` too. An iterator dying in a frame nulls its words, so a temporary left behind does not root the blocks under the conservative stack scan.
- Thread safety is that of `std::deque`: concurrent readers, or one writer, with the program's own synchronization. The collector never waits for a mutator and never touches a block the deque still holds.
- The cost of an add at either end: a load of the map, a load of the block, the construction, two stores (the block's range and the count); the map at its end or a missing block goes the slow way ([Benchmarks: Containers](../../containers/benchmarks.md#containers): 2.0 ns per push against 1.5 ns for `std::deque`).

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::deque<T>;
using SizeType = size_t;
```

### Constructors

```cpp
Deque();
explicit Deque(SizeType count);
Deque(SizeType count, const T& value);
template<std::input_iterator It> Deque(It first, It last);
template<std::ranges::input_range R> explicit Deque(R&& r);   // from a range of what T is made of: the Pieces of a String, a view, another container (not a Deque: that is the copy)
Deque(std::initializer_list<T> il);
explicit Deque(InnerType d) noexcept;
Deque(const Deque& other);
Deque(Deque&& other) noexcept;
```

The default constructor allocates nothing. `Deque(count)` holds `count` value-initialized elements, `Deque(count, value)` `count` copies. The range constructor appends element by element, single-pass ranges included. A copy has blocks of its own; a move takes the map over and leaves `other` empty. An element constructor that throws leaves the deque empty and the exception propagates.

```cpp
Deque<int> zeros(4);                          // 0 0 0 0
Deque<String> words(2, "x");             // "x" "x"
Deque digits = {1, 2, 3};
Deque<int> copy(begin(digits), end(digits));
Deque<int> taken = std::move(digits);         // digits is empty now
```

### Destructor

```cpp
~Deque();
```

Destroys the elements, unless the deque dies in a sweep: then its blocks are garbage of the same sweep and destroy their elements themselves. The blocks and the map are left to the collector.

### operator=

```cpp
Deque& operator=(const Deque& other);
Deque& operator=(Deque&& other) noexcept;
Deque& operator=(std::initializer_list<T> il);
```

Copy assignment is `Assign(begin(other), end(other))`. Move assignment clears this deque and takes the other map over, leaving `other` empty.

```cpp
Deque a = {1, 2, 3};
Deque<int> b;
b = a;              // a copy, in blocks of its own
b = {4, 5};         // two elements
b = std::move(a);   // a is empty
```

### Assign

```cpp
void Assign(SizeType count, const T& value);
template<std::input_iterator It> void Assign(It first, It last);
void Assign(std::initializer_list<T> il);
```

Replaces the contents: the existing elements are assigned over in place, the surplus removed, the missing ones appended.

```cpp
Deque d = {1, 2, 3};
d.Assign(2, 9);          // 9 9
d.Assign({7, 8, 9, 10});
```

### operator[]

```cpp
T& operator[](SizeType i) noexcept;
const T& operator[](SizeType i) const noexcept;
```

The element at `i`, unchecked: a load of the map and a load of the block.

```cpp
Deque d = {10, 20, 30};
d[1] = 25;
```

### First, Last

```cpp
T& First() noexcept;
const T& First() const noexcept;
T& Last() noexcept;
const T& Last() const noexcept;
```

The first and the last element; the deque must not be empty.

### begin, end

```cpp
auto begin(Deque&) noexcept;    auto begin(const Deque&) noexcept;   // free functions
auto end(Deque&) noexcept;      auto end(const Deque&) noexcept;
```

Random-access iterators, as free functions: a step within a block and the access are plain loads, a step across a block boundary one load of the map, so a range-for, the `std::ranges` algorithms and `std::sort` work on the deque. An iterator keeps nothing alive by itself and is invalidated exactly when a `std::deque` iterator is.

```cpp
Deque d = {3, 1, 2};
std::ranges::sort(d);                          // 1 2 3
for (int& x : d) {
    x *= 10;                                   // 10 20 30
}
```

### IsEmpty, Count

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
```

`Count()` is a word of the deque object.

### Shrink

```cpp
void Shrink();
```

Replaces the map by one holding exactly the blocks in use: the spare blocks go. An empty deque drops everything, as `Clear()` does.

### Clear

```cpp
void Clear() noexcept;
```

Destroys every element and drops the blocks and the map: unlike `List`, nothing is kept for the next add.

### Insert, InsertRange, EmplaceAt

```cpp
void Insert(SizeType i, const T& value);
void Insert(SizeType i, T&& value);
void Insert(SizeType i, SizeType count, const T& value);
void Insert(SizeType i, std::initializer_list<T> il);
template<std::ranges::input_range R> void InsertRange(SizeType i, R&& r);
template<class... A> T& EmplaceAt(SizeType i, A&&... a);
```

Inserts before position `i` (`Count()` for the end). An insertion at either end is an add there; in the middle the shorter side of the deque shifts by the count. The value is built before anything moves, so an argument that refers to an element of this deque stays valid. A single-pass range is collected first, so that a failure leaves the deque as it was; on an exception the deque is as it was.

```cpp
Deque d = {1, 4};
d.Insert(1, 2);                           // 1 2 4
d.Insert(2, 2, 3);                        // 1 2 3 3 4
d.EmplaceAt(d.Count(), 5);                // 1 2 3 3 4 5
d.Insert(0, {-1, 0});                     // -1 0 1 2 3 3 4 5
d.Insert(d.Count(), d[0]);                // an element of d: fine, copied before anything moves
```

### RemoveAt, RemoveRange

```cpp
void RemoveAt(SizeType i);
void RemoveRange(SizeType i, SizeType n);
```

Removes the elements, shifting the shorter side of the deque over them and popping at that end, so the elements are destroyed at the end nearer to the range, as `std::deque` may do. An empty range is a no-op.

```cpp
Deque d = {1, 2, 3, 4, 5};
d.RemoveAt(0);                            // 2 3 4 5
d.RemoveRange(1, 3);                      // 2
```

### AddLast, EmplaceLast, AddFirst, EmplaceFirst

```cpp
void AddLast(const T& value);
void AddLast(T&& value);
template<class... A> T& EmplaceLast(A&&... a);
void AddFirst(const T& value);
void AddFirst(T&& value);
template<class... A> T& EmplaceFirst(A&&... a);
```

Appends an element at the back or the front and (`Emplace*`) returns a reference to it. The common case, a block with room at that end (the last one in use, or the spare), is two loads, the construction and two stores; the map at its end or a missing block allocates a block, and a fresh map when the map is full. References to the other elements stay valid, iterators do not.

```cpp
Deque<Ptr<int>> ptrs;
for (int i : Range(1000)) {
    ptrs.AddLast(Make<int>(i));
    ptrs.AddFirst(Make<int>(-i));     // the outgrown maps are collected
}
int& last = *ptrs.EmplaceLast(Make<int>(1000));
```

### RemoveLast, RemoveFirst

```cpp
void RemoveLast() noexcept;
void RemoveFirst() noexcept;
```

Destroys the last or the first element. A block left empty stays as the spare of its end (an older spare at the same end goes) while the deque holds elements, and every block goes when it holds none. The deque must not be empty.

### Resize

```cpp
void Resize(SizeType n);
void Resize(SizeType n, const T& value);
```

Removes from the back down to `n`, or adds value-initialized elements (copies of `value`) at the back up to it.

```cpp
Deque d = {1, 2, 3};
d.Resize(5);          // 1 2 3 0 0
d.Resize(2);          // 1 2
d.Resize(4, 7);       // 1 2 7 7
```

### Remove, RemoveAll

```cpp
bool Remove(const T& value);
template<class U> SizeType RemoveAll(const U& value);
template<class Pred> SizeType RemoveAll(Pred pred);
```

`Remove` removes the first element equal to `value` and says whether there was one; `RemoveAll` removes every element equal to `value`, or every one satisfying `pred`, and returns how many were removed.

```cpp
Deque d = {1, 2, 2, 3, 4};
size_t twos = d.RemoveAll(2);                                    // 2; d is 1 3 4
size_t big = d.RemoveAll([](int x) { return x > 2; });           // 2; d is 1
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

The members of [`MSequence`](MSequence.md), the mixin every sequence of the interface carries: the algorithms of `<algorithm>` as members, so that `d.Sort()` reads as `d.Add(x)` does. A linear search from the front (`LastIndexOf` walks the whole sequence); `IndexOf`, `LastIndexOf` and `FindIndex` give the position, or `NoIndex` when nothing matches; `Find` the element the predicate accepts first, or null. `Reverse` and `Sort` are `std::reverse` and `std::sort` over the buffer.

```cpp
Deque d = {5, 3, 9, 3};
assert(d.Contains(9) && d.IndexOf(3) == 1 && d.LastIndexOf(3) == 3 && d.IndexOf(7) == NoIndex);
assert(d.FindIndex([](int x) { return x > 4; }) == 0 && d.Exists([](int x) { return x == 9; }) && !d.All([](int x) { return x > 3; }));
if (int* big = d.Find([](int x) { return x > 8; })) {
    *big = 8;
}
assert(d.CountOf([](int x) { return x == 3; }) == 2 && d.Min() == 3 && d.Max() == 8);
d.Sort();                                        // 3 3 5 8
assert(d.IsSorted());
d.Sort([](int a, int b) { return a > b; });     // 8 5 3 3
d.Reverse();                                     // 3 3 5 8
int sum = 0;
d.ForEach([&](int x) { sum += x; });         // 19
d.Fill(0);
```

### Swap

```cpp
void Swap(Deque& other) noexcept;
void swap(Deque& l, Deque& r) noexcept;           // free function
```

Exchanges the maps and the counts; no element is touched.

### Comparisons

```cpp
bool operator==(const Deque& l, const Deque& r);
auto operator<=>(const Deque& l, const Deque& r);
```

Element-wise, as for `std::deque`: `==` compares sizes first, `<=>` is lexicographical with the synthesized three-way comparison (`<=>` of `T` when it has one, else a `std::weak_ordering` built from `<`), so `<`, `<=`, `>`, `>=` and `!=` follow.

```cpp
Deque<int> a = {1, 2}, b = {1, 3};
bool less = a < b;                  // true
bool same = a == b;                 // false
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The deque inside, as its own type.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Job {
    int id;
    List<Ptr<Job>> dependsOn;         // a job may wait for others
};

struct Dispatcher {
    Deque<Ptr<Job>> pending;          // inside a managed object: traced with it
};

int main() {
    // A work queue on the stack: adds at the back, removals at the front,
    // and a window of elements travelling through it allocates no blocks
    Deque<int> window;
    for (int i : Range(100000)) {
        window.AddLast(i);
        if (window.Count() > 16) {
            window.RemoveFirst();                   // the emptied block stays as the spare
        }
    }

    // A dispatcher in a managed object; urgent jobs go to the front
    Ptr s = Make<Dispatcher>();
    for (int i : Range(1000)) {
        Ptr job = Make<Job>(i);
        if (i % 100 == 0) {
            s->pending.AddFirst(job);
        } else {
            s->pending.AddLast(job);
            s->pending.First()->dependsOn.Add(job);   // the urgent job waits for it
        }
    }

    // Drain the front half: a removed job nothing else refers to is garbage,
    // a removed urgent job lives on while a pending job it waits for is held
    int drained = 0;
    while (s->pending.Count() > 500) {
        s->pending.RemoveFirst();
        ++drained;
    }
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << drained << " jobs drained, " << s->pending.Count() << " pending, first is job "
              << s->pending.First()->id << "; " << Collector::LiveObjectCount() << " live objects\n";
    return window.Count() == 16 && window.First() == 99984 && s->pending.Count() == 500 ? 0 : 1;
}
```

The output:

```
500 jobs drained, 500 pending, first is job 495; 508 live objects
```

The deque `window` dies at the end of `main` and destroys its sixteen elements then; its blocks and its map are freed by the collector once nothing refers to them.

## See also

- [Ptr](../Core/Ptr.md), [UniquePtr](../Core/UniquePtr.md), [Make](../Core/Make.md)
- [List](List.md) for a contiguous buffer, [LinkedList](LinkedList.md) for stable references, [Stack](Stack.md) and [Queue](Queue.md), the adapters over a deque
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules), [README: Stack roots](../../../garbage_collector/overview.md#stack-roots)
