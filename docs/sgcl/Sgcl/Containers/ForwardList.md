# Sgcl::ForwardList

```cpp
#include "sgcl/Sgcl/Containers/LinkedList.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class ForwardList;
}
```

The same class in the `sgcl` interface: [forward_list](../../containers/forward_list.md).

`ForwardList<T>` is `std::forward_list` over managed nodes: singly linked nodes behind a sentinel, the one `BeforeBegin()` addresses. The interface is that of `std::forward_list` under the interface's names (constructors, `Assign`, `First`, forward iterators, `AddAfter`, `EmplaceAfter`, `RemoveAfter`, `AddFirst`, `Merge`, `SpliceAfter`, `Remove`, `RemoveAll`, `Reverse`, `Unique`, `Sort`, three-way comparison; no `Count()`), and so is the behaviour: an element is constructed at insertion and destroyed at removal, references and iterators to the other elements stay valid through every insertion, erasure and relink.

What differs is who frees the nodes. The links are `Ptr`s, so the rooted sentinel keeps every node alive and the list walks them through raw pointers; a `RemoveAfter` unlinks a node and destroys its element, and the collector reclaims the node later, once nothing refers to it. Nothing is ever freed by hand, so a cycle through a list is collected like any other cycle. The list object is one word, the sentinel, which every list owns from its construction on (a default-constructed list allocates it), so `AddAfter` and `RemoveAfter` work the same at any position; the sentinel is a bare link with no element. A node is as big as its `std` counterpart and pays no malloc rounding ([Benchmarks: Containers](../../containers/benchmarks.md#containers): 14.5 ns per add at the front against 22.1 ns for `std::forward_list`).

## Rules

- A forward list holds a `Ptr` (the sentinel), so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../../core/README.md#the-rules), 1).
- The list destroys an element the moment it is removed, cleared, assigned over or the list is destroyed, exactly like `std::forward_list`. The one exception is a list dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it ([Containers](../../containers/README.md#containers)).
- An iterator to a removed element is invalid as in `std`: it keeps the node's memory mapped but not the element. A `Ptr` may not address an element of a list ([The rules](../../core/README.md#the-rules), 4): elements are reached through the list, its iterators, references and raw pointers.
- Iterators (the free `begin`/`end`) are raw node pointers, trivially copyable and at home in any container, a `std::vector` too: the list roots every linked node. Stepping and dereferencing are plain loads, with no write barrier.
- Relinking operations (`SpliceAfter`, `Merge`, `Sort`, `Reverse`) move nodes, never elements, and every node stays reachable through a `Ptr` while it is being moved: every reference stays valid.
- Thread safety is that of `std::forward_list`: concurrent readers, or one writer, with the program's own synchronization. The collector never waits for a mutator and never touches a node the list still links.

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::forward_list<T>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // forward, over T
using ConstIterator = InnerType::const_iterator;    // over const T
```

### Constructors

```cpp
ForwardList();
explicit ForwardList(SizeType count);
ForwardList(SizeType count, const T& value);
template<std::input_iterator It> ForwardList(It first, It last);
template<std::ranges::input_range R> explicit ForwardList(R&& r);   // from a range of what T is made of: the Pieces of a String, a view, another container (not a ForwardList: that is the copy)
ForwardList(std::initializer_list<T> il);
explicit ForwardList(InnerType l) noexcept;
ForwardList(const ForwardList& other);
ForwardList(ForwardList&& other);
```

Every constructor allocates the sentinel. `ForwardList(count)` holds `count` value-initialized elements, `ForwardList(count, value)` `count` copies. A copy has nodes of its own; a move takes the chain of `other`, which keeps its sentinel and is empty. An element constructor that throws leaves the list empty and the exception propagates.

```cpp
ForwardList<int> zeros(4);                          // 0 0 0 0
ForwardList<String> words(2, "x");             // "x" "x"
ForwardList digits = {1, 2, 3};
ForwardList<int> copy(begin(digits), end(digits));
ForwardList<int> taken = std::move(digits);         // digits is empty now
```

### Destructor

```cpp
~ForwardList();
```

Destroys the elements, unless the list dies in a sweep: then its nodes are garbage of the same sweep and destroy their elements themselves. The nodes and the sentinel are left to the collector.

### operator=

```cpp
ForwardList& operator=(const ForwardList& other);
ForwardList& operator=(ForwardList&& other);
```

Copy assignment is `Assign(begin(other), end(other))`. Move assignment clears this list and takes the chain of `other`, leaving `other` empty.

### Assign

```cpp
void Assign(SizeType count, const T& value);
template<std::input_iterator It> void Assign(It first, It last);
void Assign(std::initializer_list<T> il);
```

Replaces the contents: the existing elements are assigned over in their nodes, the surplus removed, the missing ones appended.

```cpp
ForwardList l = {1, 2, 3};
l.Assign(2, 9);          // 9 9, in the first two nodes
l.Assign({7, 8, 9, 10});
```

### First

```cpp
T& First();
const T& First() const;
```

The first element; the list must not be empty (debug builds assert).

### BeforeBegin, begin, end

```cpp
Iterator BeforeBegin() noexcept;                    ConstIterator BeforeBegin() const noexcept;
Iterator begin(ForwardList&) noexcept;              ConstIterator begin(const ForwardList&) noexcept;   // free functions
Iterator end(ForwardList&) noexcept;                ConstIterator end(const ForwardList&) noexcept;
```

Forward iterators, raw node pointers. `BeforeBegin()` is the sentinel, the position to add or remove after for the front; it must not be dereferenced. `end(l)` is a null iterator. An iterator keeps nothing alive by itself and is invalidated only by the removal of its own element.

```cpp
ForwardList l = {3, 1, 2};
auto it = std::ranges::find(l, 1);
*it = 10;                                      // 3 10 2
auto before = l.BeforeBegin();
l.AddAfter(before, 0);                         // 0 3 10 2
```

### IsEmpty

```cpp
bool IsEmpty() const noexcept;
```

There is no `Count()`: a singly linked list does not know its length without a walk (`std::ranges::distance(l)`).

### Clear

```cpp
void Clear() noexcept;
```

Destroys every element and unlinks every node; the sentinel stays.

### AddAfter, EmplaceAfter

```cpp
Iterator AddAfter(ConstIterator pos, const T& value);
Iterator AddAfter(ConstIterator pos, T&& value);
Iterator AddAfter(ConstIterator pos, SizeType count, const T& value);
template<class... A> Iterator EmplaceAfter(ConstIterator pos, A&&... a);
```

Inserts after `pos` (`BeforeBegin()` for the front) and returns an iterator to the last inserted element (`pos` when nothing is inserted). A node is made and its element constructed in one step, so no node ever holds an unconstructed element; a range is built as a chain and linked in, so a throwing constructor leaves the list as it was.

```cpp
ForwardList l = {1, 4};
auto it = l.AddAfter(begin(l), 2);             // 1 2 4, it -> 2
it = l.AddAfter(it, 2, 3);                     // 1 2 3 3 4, it -> the second 3
l.EmplaceAfter(it, 9);                         // 1 2 3 3 9 4
l.AddAfter(l.BeforeBegin(), 0);                // 0 1 2 3 3 9 4
```

### RemoveAfter

```cpp
Iterator RemoveAfter(ConstIterator pos);
Iterator RemoveAfter(ConstIterator first, ConstIterator last);
```

Destroys the element after `pos`, or the elements in `(first, last)`, and unlinks their nodes; returns the iterator to the element after the removed ones (`end(l)` when there is none after `pos`). The nodes are the collector's once unlinked.

```cpp
ForwardList l = {1, 2, 3, 4, 5};
auto it = l.RemoveAfter(l.BeforeBegin());      // 2 3 4 5, it -> 2
l.RemoveAfter(it, end(l));                     // 2
```

### AddFirst, EmplaceFirst, RemoveFirst

```cpp
void AddFirst(const T& value);
void AddFirst(T&& value);
template<class... A> T& EmplaceFirst(A&&... a);
void RemoveFirst();
```

Adds an element at the front, in a node of its own, and (`EmplaceFirst`) returns a reference to it; `RemoveFirst` destroys the first element and unlinks its node (the list must not be empty).

```cpp
ForwardList<Ptr<int>> ptrs;
for (int i : Range(1000)) {
    ptrs.AddFirst(Make<int>(i));
}
int& first = *ptrs.EmplaceFirst(Make<int>(-1));
ptrs.RemoveFirst();
```

### Resize

```cpp
void Resize(SizeType n);
```

Removes the elements past `n`, or appends value-initialized elements up to it.

### Swap

```cpp
void Swap(ForwardList& other) noexcept;
template<class T> void swap(ForwardList<T>& l, ForwardList<T>& r) noexcept;   // free function
```

Exchanges the chains; no element is touched.

### Merge

```cpp
void Merge(ForwardList&& other);
template<class Compare> void Merge(ForwardList&& other, Compare cmp);
```

Merges two sorted lists into this one, stable: of equal elements, those of this list come first; `other` is empty afterwards. The nodes are relinked, never copied.

```cpp
ForwardList<int> a = {1, 3, 5}, b = {2, 4, 6};
a.Merge(std::move(b));                         // a is 1 2 3 4 5 6, b is empty
```

### Prepend, SpliceAfter

```cpp
void Prepend(ForwardList&& other) noexcept;
void SpliceAfter(ConstIterator pos, ForwardList& other) noexcept;
void SpliceAfter(ConstIterator pos, ForwardList& other, ConstIterator it) noexcept;
void SpliceAfter(ConstIterator pos, ForwardList& other, ConstIterator first, ConstIterator last) noexcept;
```

`Prepend` moves every node of `other` to the front; `SpliceAfter` moves the nodes of `other`, the node after `it`, or the nodes in `(first, last)`, after `pos`. No element is copied or destroyed and every iterator stays valid, now naming an element of this list. `other` may be this list. Moving all of `other` walks it to its last node.

```cpp
ForwardList<int> a = {1, 2}, b = {3, 4, 5};
a.SpliceAfter(begin(a), b, b.BeforeBegin());   // a is 1 3 2, b is 4 5
a.Prepend(std::move(b));                       // a is 4 5 1 3 2, b is empty
```

### Remove, RemoveAll

```cpp
SizeType Remove(const T& value);
template<class Pred> SizeType RemoveAll(Pred pred);
```

Remove every element equal to `value`, or satisfying `pred`, and return how many were removed.

```cpp
ForwardList l = {1, 2, 2, 3, 4};
size_t twos = l.Remove(2);                                       // 2; l is 1 3 4
size_t big = l.RemoveAll([](int x) { return x > 2; });           // 2; l is 1
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
bool IsSorted() const;  template<class Compare> bool IsSorted(Compare cmp) const;
SizeType BinarySearch(const auto& value) const;   // on a sorted sequence: the value's position, NoIndex when it is not there; and with a comparator
auto LowerBound(const auto& value);  auto UpperBound(const auto& value);   // the first position not less than the value, the first greater; and const, and with a comparator
```

The members of [`MSequence`](MSequence.md), the algorithms every sequence of the interface has as members, so that `l.Contains(x)` reads as `l.AddFirst(x)` does. A linear search from the front, a walk over the nodes; `IndexOf`, `LastIndexOf` and `FindIndex` give the position, or `NoIndex` when nothing matches; `Find` the element the predicate accepts first, or null. `Reverse` and `Sort` are the list's own, on the nodes (below).

```cpp
ForwardList l = {5, 3, 9, 3};
assert(l.Contains(9) && l.IndexOf(3) == 1 && l.LastIndexOf(3) == 3 && l.IndexOf(7) == NoIndex);
assert(l.FindIndex([](int x) { return x > 4; }) == 0 && l.Exists([](int x) { return x == 9; }) && !l.All([](int x) { return x > 3; }));
if (int* big = l.Find([](int x) { return x > 8; })) {
    *big = 8;
}
assert(l.CountOf([](int x) { return x == 3; }) == 2 && l.Min() == 3 && l.Max() == 8);
l.Sort();                                        // 3 3 5 8
assert(l.IsSorted());
l.Sort([](int a, int b) { return a > b; });     // 8 5 3 3
l.Reverse();                                     // 3 3 5 8
int sum = 0;
l.ForEach([&](int x) { sum += x; });         // 19
l.Fill(0);
```

### Unique

```cpp
SizeType Unique();
template<class Pred> SizeType Unique(Pred pred);
```

Removes every element equal to the one before it (`pred(previous, current)`), keeping the first of each run; returns how many were removed.

### Reverse

```cpp
void Reverse() noexcept;
```

Reverses the order of the nodes, in place.

### Sort

```cpp
void Sort();
template<class Compare> void Sort(Compare cmp);
```

A stable merge sort in place: the nodes are relinked, never detached.

```cpp
ForwardList l = {3, 1, 2};
l.Sort();                                      // 1 2 3
l.Sort(std::greater<>());                      // 3 2 1
```

### Comparisons

```cpp
bool operator==(const ForwardList& l, const ForwardList& r);
auto operator<=>(const ForwardList& l, const ForwardList& r);
```

Element-wise, as for `std::forward_list`: `<=>` is lexicographical with the synthesized three-way comparison, so `<`, `<=`, `>`, `>=` and `!=` follow.

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The list inside, as its own type.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>
#include <ranges>

struct Vertex {
    int id;
    ForwardList<Ptr<Vertex>> edges;       // an adjacency list, inside the vertex
};

int main() {
    // A list of values on the stack: the nodes are on the managed heap
    ForwardList numbers = {5, 3, 9, 1};
    numbers.AddFirst(7);
    numbers.Sort();                                 // 1 3 5 7 9, the nodes relinked in place
    numbers.RemoveAll([](int x) { return x > 5; });

    // A graph: every vertex points at every other, so every vertex is in a cycle
    ForwardList<Ptr<Vertex>> vertices;
    for (int i : Range(100)) {
        vertices.AddFirst(Make<Vertex>(i));
    }
    for (const auto& v : vertices) {
        for (const auto& w : vertices) {
            if (v != w) {
                v->edges.AddFirst(w);
            }
        }
    }

    // Keep one vertex, drop the list: the whole graph stays reachable through it
    Ptr keep = vertices.First();
    vertices.Clear();
    keep->edges.RemoveAll([](const Ptr<Vertex>& w) { return w->id % 2; });     // the odd ids, kept by the cycle still

    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    int edges = int(std::ranges::distance(keep->edges));
    std::cout << std::ranges::distance(numbers) << " numbers, vertex " << keep->id << " with "
              << edges << " edges, " << Collector::LiveObjectCount() << " live objects\n";

    keep = nullptr;                                 // the cycle is unreachable now: collected as a whole
    Collector::Collect(true);              // optional, as above
    std::cout << Collector::LiveObjectCount() << " live objects after the graph is gone\n";
    return std::ranges::distance(numbers) == 3 && edges == 50 ? 0 : 1;
}
```

The output:

```
3 numbers, vertex 99 with 50 edges, 10056 live objects
5 live objects after the graph is gone
```

The list `numbers` dies at the end of `main` and destroys its elements then; its nodes are freed by the collector once nothing refers to them.

## See also

- [Ptr](../Core/Ptr.md), [UniquePtr](../Core/UniquePtr.md), [Make](../Core/Make.md)
- [LinkedList](LinkedList.md) for a doubly linked list with `Count()`, `AddLast` and reverse iteration
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules), [README: Stack roots](../../../garbage_collector/overview.md#stack-roots)
