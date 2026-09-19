# Sgcl::LinkedList

```cpp
#include "sgcl/Sgcl/Containers/LinkedList.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class LinkedList;
}
```

The same class in the `sgcl` interface: [list](../../containers/list.md).

`LinkedList<T>` is `std::list` over managed nodes: a circular doubly linked list around a managed sentinel, with the names of a linked list: `AddFirst`, `AddLast`, `AddBefore`, `AddAfter`, `RemoveAt`, `Count`. The behaviour is `std::list`'s: an element is constructed at insertion and destroyed at removal, references and iterators to the other elements stay valid through every insertion, erasure and relink; `Merge`, `Splice`, `Remove`, `RemoveAll`, `Reverse`, `Unique`, `Sort` relink nodes, never copy elements.

What differs from `std::list` is who frees the nodes. The links are `Ptr`s, so a rooted sentinel keeps every node alive and the list walks them through raw pointers; a removal unlinks a node and destroys its element, and the collector reclaims the node later, once nothing refers to it. Nothing is ever freed by hand, so a cycle through a list is collected like any other cycle. The list object is two words (the sentinel and the count); the sentinel is created on first use, so a default-constructed list allocates nothing. A node is as big as its `std` counterpart and pays no malloc rounding ([Benchmarks: Containers](../../containers/benchmarks.md#containers): 17.8 ns per add at the back against 26.4 ns for `std::list`, 2.0 ns per step of iteration against 2.2 ns).

## Rules

- A list holds a `Ptr` (the sentinel), so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../../core/README.md#the-rules), 1).
- The list destroys an element the moment it is removed, cleared, assigned over or the list is destroyed, exactly like `std::list`. The one exception is a list dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it ([Containers](../../containers/README.md#containers)).
- An iterator to a removed element is invalid as in `std`: it keeps the node's memory mapped but not the element. A `Ptr` may not address an element of a list ([The rules](../../core/README.md#the-rules), 4): elements are reached through the list, its iterators, references and raw pointers.
- Iterators (the free `begin`/`end`) are raw node pointers, trivially copyable and at home in any container, a `std::vector` too: the list roots every linked node. Stepping and dereferencing are plain loads, with no write barrier.
- Relinking operations (`Splice`, `Merge`, `Sort`, `Reverse`) move nodes, never elements: every reference stays valid, and a throwing comparator leaves valid lists of the same elements.
- Thread safety is that of `std::list`: concurrent readers, or one writer, with the program's own synchronization. The collector never waits for a mutator and never touches a node the list still links.

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::list<T>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // bidirectional, over T
using ConstIterator = InnerType::const_iterator;    // over const T
```

`Iterator` converts to `ConstIterator`; both model `std::bidirectional_iterator`.

### Constructors

```cpp
LinkedList() noexcept;
explicit LinkedList(SizeType count);
LinkedList(SizeType count, const T& value);
template<std::input_iterator It> LinkedList(It first, It last);
template<std::ranges::input_range R> explicit LinkedList(R&& r);   // from a range of what T is made of: the Pieces of a String, a view, another container (not a LinkedList: that is the copy)
LinkedList(std::initializer_list<T> il);
explicit LinkedList(InnerType l) noexcept;
LinkedList(const LinkedList& other);
LinkedList(LinkedList&& other) noexcept;
```

The default constructor allocates nothing, not even the sentinel. `LinkedList(count)` holds `count` value-initialized elements, `LinkedList(count, value)` `count` copies. A copy has nodes of its own; a move takes the sentinel over and leaves `other` empty. An element constructor that throws leaves the list as it was before that insertion.

```cpp
LinkedList<int> zeros(4);                          // 0 0 0 0
LinkedList<String> words(2, "x");             // "x" "x"
LinkedList digits = {1, 2, 3};
LinkedList<int> copy(begin(digits), end(digits));
LinkedList<int> taken = std::move(digits);         // digits is empty now
```

### Destructor

```cpp
~LinkedList();
```

Destroys the elements, unless the list dies in a sweep: then its nodes are garbage of the same sweep and destroy their elements themselves. The nodes and the sentinel are left to the collector.

### operator=

```cpp
LinkedList& operator=(const LinkedList& other);
LinkedList& operator=(LinkedList&& other) noexcept;
LinkedList& operator=(std::initializer_list<T> il);
```

Copy assignment is `Assign(begin(other), end(other))`. Move assignment clears this list and takes the other sentinel over, leaving `other` empty.

```cpp
LinkedList a = {1, 2, 3};
LinkedList<int> b;
b = a;              // a copy, in nodes of its own
b = {4, 5};         // two elements
b = std::move(a);   // a is empty
```

### Assign

```cpp
void Assign(SizeType count, const T& value);
template<std::input_iterator It> void Assign(It first, It last);
void Assign(std::initializer_list<T> il);
```

Replaces the contents: the existing elements are assigned over in their nodes, the surplus removed, the missing ones appended.

```cpp
LinkedList l = {1, 2, 3};
l.Assign(2, 9);          // 9 9, in the first two nodes
l.Assign({7, 8, 9, 10});
```

### First, Last

```cpp
T& First();
const T& First() const;
T& Last();
const T& Last() const;
```

The first and the last element; the list must not be empty (debug builds assert).

### begin, end

```cpp
Iterator begin(LinkedList&) noexcept;               ConstIterator begin(const LinkedList&) noexcept;   // free functions
Iterator end(LinkedList&) noexcept;                 ConstIterator end(const LinkedList&) noexcept;
```

Bidirectional iterators, raw node pointers, as free functions: a range-for and the `std::ranges` algorithms that need no random access work on the list. `end(l)` is the sentinel (null for a list that has none yet, which means the same). An iterator keeps nothing alive by itself and is invalidated only by the removal of its own element, with one exception: the `end(l)` of a list that never held an element is a null iterator, and the first insertion, which makes the sentinel, invalidates it, so take a fresh `end(l)` after it. Used all the same, it does no harm: it no longer compares equal to `end(l)`, but it still means the end as a position and as the end of a range, and a range that starts at it is empty.

```cpp
LinkedList l = {3, 1, 2};
auto it = std::ranges::find(l, 1);
l.RemoveAt(it);                                // 3 2; it is invalid now
for (int& x : l) {
    x *= 10;                                   // 30 20
}
```

### IsEmpty, Count

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
```

`Count()` is a word of the list object, kept through every splice.

### Clear

```cpp
void Clear() noexcept;
```

Destroys every element and unlinks every node; the sentinel stays.

### AddBefore, AddAfter, EmplaceBefore

```cpp
Iterator AddBefore(ConstIterator pos, const T& value);
Iterator AddBefore(ConstIterator pos, T&& value);
Iterator AddBefore(ConstIterator pos, SizeType count, const T& value);
Iterator AddBefore(ConstIterator pos, std::initializer_list<T> il);
template<class... A> Iterator EmplaceBefore(ConstIterator pos, A&&... a);
Iterator AddAfter(ConstIterator pos, const T& value);
Iterator AddAfter(ConstIterator pos, T&& value);
```

Inserts before `pos` (`end(l)` for the back), or after it, and returns an iterator to the first inserted element (`pos` itself when nothing is inserted). A node is made and its element constructed in one step, so no node ever holds an unconstructed element; a range is built as a chain first and linked in at once, so a throwing constructor leaves the list as it was.

```cpp
LinkedList l = {1, 4};
auto it = l.AddBefore(std::next(begin(l)), 2);   // 1 2 4
l.AddAfter(it, 3);                               // 1 2 3 4
l.AddBefore(std::next(it, 2), 1, 3);             // 1 2 3 3 4
l.EmplaceBefore(end(l), 5);                      // 1 2 3 3 4 5
l.AddBefore(begin(l), {-1, 0});                  // -1 0 1 2 3 3 4 5
```

### RemoveAt, RemoveRange

```cpp
Iterator RemoveAt(ConstIterator pos);
Iterator RemoveRange(ConstIterator first, ConstIterator last);
```

Destroys the elements and unlinks their nodes; returns the iterator to the element after the removed range. `RemoveAt(end(l))` is a no-op, and so is a range that starts at the null `end(l)` of a list that had no sentinel yet ([Iterators](#iterators)). The nodes are the collector's once unlinked.

```cpp
LinkedList l = {1, 2, 3, 4, 5};
auto it = l.RemoveAt(begin(l));                // 2 3 4 5, it -> 2
l.RemoveRange(std::next(it), end(l));          // 2
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

Appends an element at the back or the front, in a node of its own, and (`Emplace*`) returns a reference to it.

```cpp
LinkedList<Ptr<int>> ptrs;
for (int i : Range(1000)) {
    ptrs.AddLast(Make<int>(i));
}
int& first = *ptrs.EmplaceFirst(Make<int>(-1));
```

### RemoveLast, RemoveFirst

```cpp
void RemoveLast();
void RemoveFirst();
```

Destroys the last or the first element and unlinks its node. The list must not be empty (debug builds assert).

### Resize

```cpp
void Resize(SizeType n);
void Resize(SizeType n, const T& value);
```

Removes the elements past `n`, or appends value-initialized elements (copies of `value`) up to it.

```cpp
LinkedList l = {1, 2, 3};
l.Resize(5);          // 1 2 3 0 0
l.Resize(2);          // 1 2
l.Resize(4, 7);       // 1 2 7 7
```

### Swap

```cpp
void Swap(LinkedList& other) noexcept;
template<class T> void swap(LinkedList<T>& l, LinkedList<T>& r) noexcept;   // free function
```

Exchanges the sentinels and the counts; no element is touched.

### Merge

```cpp
void Merge(LinkedList&& other);
template<class Compare> void Merge(LinkedList&& other, Compare cmp);
```

Merges two sorted lists into this one, stable: of equal elements, those of this list come first. Runs of the other list move over in one relink; the sizes follow each move, so a throwing comparator leaves two valid lists.

```cpp
LinkedList<int> a = {1, 3, 5}, b = {2, 4, 6};
a.Merge(std::move(b));                         // a is 1 2 3 4 5 6, b is empty
```

### Append, Prepend, Splice

```cpp
void Append(LinkedList&& other) noexcept;
void Prepend(LinkedList&& other) noexcept;
void Splice(ConstIterator pos, LinkedList& other) noexcept;
void Splice(ConstIterator pos, LinkedList& other, ConstIterator it) noexcept;
void Splice(ConstIterator pos, LinkedList& other, ConstIterator first, ConstIterator last) noexcept;
```

`Append` and `Prepend` move every node of `other` to the back or the front of this list; `Splice` moves the nodes of `other`, the node `it` names, or the nodes in `[first, last)`, before `pos`. No element is copied or destroyed and every iterator stays valid, now naming an element of this list. `other` may be this list. Splicing a range from another list counts its nodes.

```cpp
LinkedList<int> a = {1, 2}, b = {3, 4, 5};
a.Splice(end(a), b, begin(b));                 // a is 1 2 3, b is 4 5
a.Prepend(std::move(b));                       // a is 4 5 1 2 3, b is empty
```

### Remove, RemoveAll

```cpp
SizeType Remove(const T& value);
template<class Pred> SizeType RemoveAll(Pred pred);
```

Remove every element equal to `value`, or satisfying `pred`, and return how many were removed. A `value` that is an element of this list is removed last, after the comparisons that read it.

```cpp
LinkedList l = {1, 2, 2, 3, 4};
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
LinkedList l = {5, 3, 9, 3};
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

### Reverse

```cpp
void Reverse() noexcept;
```

Reverses the order of the nodes, in place.

### Unique

```cpp
SizeType Unique();
template<class Pred> SizeType Unique(Pred pred);
```

Removes every element equal to the one before it (`pred(previous, current)`), keeping the first of each run; returns how many were removed.

```cpp
LinkedList l = {1, 1, 2, 2, 2, 3};
size_t dropped = l.Unique();                   // 3; l is 1 2 3
```

### Sort

```cpp
void Sort();
template<class Compare> void Sort(Compare cmp);
```

A stable merge sort in place: the nodes are relinked within the list, never detached, so a throwing comparator leaves a valid list of the same elements.

```cpp
LinkedList l = {3, 1, 2};
l.Sort();                                      // 1 2 3
l.Sort(std::greater<>());                      // 3 2 1
```

### Comparisons

```cpp
bool operator==(const LinkedList& l, const LinkedList& r);
auto operator<=>(const LinkedList& l, const LinkedList& r);
```

Element-wise, as for `std::list`: `==` compares sizes first, `<=>` is lexicographical with the synthesized three-way comparison (`<=>` of `T` when it has one, else a `std::weak_ordering` built from `<`), so `<`, `<=`, `>`, `>=` and `!=` follow.

```cpp
LinkedList<int> a = {1, 2}, b = {1, 3};
bool less = a < b;                  // true
bool same = a == b;                 // false
```

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

struct Item {
    int key;
    Ptr<Item> twin;                          // items may point at each other
};

struct Registry {
    LinkedList<Ptr<Item>> items;    // inside a managed object: traced with it
};

int main() {
    // A list of values on the stack: the nodes are on the managed heap
    LinkedList numbers = {5, 3, 9, 1};
    numbers.AddFirst(7);
    numbers.Sort();                                 // 1 3 5 7 9, the nodes relinked in place
    numbers.RemoveAll([](int x) { return x % 2 == 0; });

    // A registry in a managed object; the items live in its list
    Ptr r = Make<Registry>();
    for (int i : Range(1000)) {
        r->items.AddLast(Make<Item>(i));
    }
    r->items.First()->twin = r->items.Last();       // a cycle through the list: collected like any other
    r->items.Last()->twin = r->items.First();

    // An iterator survives every other removal: remove the odd keys around it
    auto kept = std::next(begin(r->items), 500);
    for (auto it = begin(r->items); it != end(r->items);) {
        it = (*it)->key % 2 ? r->items.RemoveAt(it) : std::next(it);   // the unlinked nodes are the collector's
    }
    LinkedList<Ptr<Item>> moved;
    moved.Splice(end(moved), r->items, kept);       // the node moves, the iterator still names it

    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << numbers.Count() << " odd numbers, " << r->items.Count() << " items left in the registry, "
              << "item " << (*kept)->key << " moved out; "
              << Collector::LiveObjectCount() << " live objects\n";
    return numbers.Count() == 5 && r->items.Count() == 499 && moved.First()->key == 500 ? 0 : 1;
}
```

The output:

```
5 odd numbers, 499 items left in the registry, item 500 moved out; 1010 live objects
```

The lists `numbers` and `moved` die at the end of `main` and destroy their elements then; their nodes are freed by the collector once nothing refers to them.

## See also

- [Ptr](../Core/Ptr.md), [UniquePtr](../Core/UniquePtr.md), [Make](../Core/Make.md)
- [ForwardList](ForwardList.md) for a singly linked list, [Deque](Deque.md) and [List](List.md) for random access
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules), [README: Stack roots](../../../garbage_collector/overview.md#stack-roots)
