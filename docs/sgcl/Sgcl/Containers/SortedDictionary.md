# Sgcl::SortedDictionary

```cpp
#include "sgcl/Sgcl/Containers/SortedDictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key, class Value, class Compare = std::less<Key>>
    class SortedDictionary;
}
```

The same class in the `sgcl` interface: [map](../../containers/map.md).

`SortedDictionary<Key, Value, Compare>` is `std::map` on a red-black tree whose nodes are managed objects, with the names of a dictionary. The interface is that of [Dictionary](Dictionary.md), the keys in `Compare` order: constructors, `Add`, `Emplace`, `Set`, `operator[]`, `Remove`, `RemoveAt`, `RemoveAll`, the lookups with a transparent comparator (`Find`, `FindEntry`, `ContainsKey`), `First`/`Last`, `LowerBound`/`UpperBound`/`EqualRange`, bidirectional iterators through the free `begin`/`end` (`std::ranges` algorithms work), `KeyCompare`, `Swap`, `==` and `<=>`. The behaviour is the one of `std::map` too: unique keys in `Compare` order, an element destroyed the moment it is removed, iterators that stay valid until their element is removed.

What differs is where the memory lives. The dictionary object holds one `Ptr` (to a header node whose parent is the root, plus a count and the comparator), so it lives where a `Ptr` may live. Every node is a managed object and the links between nodes are tracked pointers: the whole tree hangs off the header and is traced from there, so a `SortedDictionary<Key, Ptr<T>>` or a dictionary inside a managed object is traced like any other managed data, and a cycle through it is collected like any other cycle. Nothing is freed by hand: a removal destroys the element and unlinks the node, and the collector reclaims the node's memory later. Iterators are one raw node pointer each, trivially copyable, and may live anywhere, a `std::vector` of them included: the dictionary roots every node it holds, and a raw pointer in a stack frame is a root of its own under the conservative scan. A lookup, an iteration and an iterator copy read raw pointers only and pay no write barrier; an insertion, a removal and a rebalancing store tracked pointers and pay the barrier on each link they relink ([README: Containers](../../containers/README.md#containers)).

The header node is allocated on the first insertion, so an empty dictionary costs nothing and the default constructor cannot throw.

## Rules

- A `SortedDictionary` holds a `Ptr`, so it lives on a stack or inside a managed object: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame ([The rules](../../core/README.md#the-rules), 1).
- The elements may hold tracked pointers (`SortedDictionary<int, Ptr<T>>`, a key that is a `Ptr`, ordered by address): the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is removed, cleared, assigned over, or the dictionary is destroyed, exactly as in `std::map`. The one exception is a dictionary dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector thread ([README: Threads](../../async/README.md#threads)).
- An iterator, a reference or a pointer to an element is valid while the element is in the dictionary, across insertions, removals of other elements, `Swap` and a move of the dictionary (it follows the node). An iterator to a removed element is invalid as in `std`; it keeps the node's memory mapped but not the element.
- A `Ptr` may point at an element, or a member of one: a node is a managed object, and an alias to a part of a managed object is allowed ([The rules](../../core/README.md#the-rules), 4). Such a pointer keeps the node alive, not the element, which dies with the removal.
- Thread safety is that of `std::map`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../../core/README.md#the-rules), 6). The collector never waits for a mutator and never touches a node the dictionary still links. A sorted dictionary shared between threads is a [ConcurrentSortedDictionary](../Concurrent/ConcurrentSortedDictionary.md).

## Members

### Types

```cpp
using KeyType = Key;
using ValueType = Value;
using PairType = std::pair<const Key, Value>;
using InnerType = sgcl::map<Key, Value, Compare>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // bidirectional, one raw node pointer
using ConstIterator = InnerType::const_iterator;
```

### Constructors

```cpp
SortedDictionary();
explicit SortedDictionary(const Compare& cmp);
template<std::input_iterator It> SortedDictionary(It first, It last);
SortedDictionary(std::initializer_list<PairType> il);
explicit SortedDictionary(InnerType c) noexcept;
SortedDictionary(const SortedDictionary& other);
SortedDictionary(SortedDictionary&& other) noexcept;
```

The default constructor allocates nothing. The range and list constructors insert in order, so sorted input costs one comparison per element; a key seen twice keeps its first value. A range of another type (pairs of a `string_view` and an `int` for `string` keys) is converted once per element, into its node, before the node's key is compared: the source need not be comparable with the keys at all. A copy has nodes of its own, in the same order: the tree copied shape for shape, a node per element with its colour and its links, no comparison and no rebalancing; a move takes the tree over and leaves `other` empty. An element constructor or comparator that throws while a constructor runs destroys the elements built so far and the dictionary holds nothing.

```cpp
SortedDictionary<String, int> ages = {{"ann", 31}, {"bob", 27}};
SortedDictionary<int, int, std::greater<int>> desc(std::greater<int>{});       // 3 2 1 order
List<Pair<int, int>> src = {{2, 20}, {1, 10}};
SortedDictionary<int, int> fromRange(begin(src), end(src));
SortedDictionary<String, int> taken = std::move(ages);                   // ages is empty now
```

### Destructor, operator=

```cpp
~SortedDictionary();
SortedDictionary& operator=(const SortedDictionary& other);
SortedDictionary& operator=(SortedDictionary&& other) noexcept;
```

The destructor destroys the elements when the dictionary dies on a stack or inside a managed object destroyed by hand; in a sweep it does nothing, the nodes being garbage of the same sweep. Copy assignment builds a copy and swaps it in; move assignment clears this dictionary and takes the tree over.

### KeyCompare

```cpp
Compare KeyCompare() const;
```

A copy of the comparator.

```cpp
SortedDictionary<int, int> m = {{1, 1}, {2, 2}};
bool byKey = m.KeyCompare()(1, 2);                          // true
```

### begin, end

```cpp
Iterator begin(SortedDictionary&) noexcept;         ConstIterator begin(const SortedDictionary&) noexcept;   // free functions
Iterator end(SortedDictionary&) noexcept;           ConstIterator end(const SortedDictionary&) noexcept;
```

`begin(m)` is the smallest key, in O(1) (the header keeps the leftmost node); `end(m)` is the header, and `std::prev(end(m))` the largest key. Before the first insertion there is no header: `begin` and `end` are both null iterators, equal to each other, neither of which may be dereferenced or moved, and an `end` taken then does not compare equal to `end` after the first insertion. An iterator is one raw node pointer: copying and advancing it costs a load, never a write barrier, and it may be kept in unmanaged memory (a `std::vector` of iterators) for as long as its element is in the dictionary.

```cpp
SortedDictionary<String, int> m = {{"b", 2}, {"a", 1}, {"c", 3}};
for (auto& [key, value] : m) {          // a b c
    value *= 10;
}
auto last = std::prev(end(m));          // "c"
std::vector<decltype(m)::Iterator> kept;   // iterators in unmanaged memory: fine
kept.push_back(begin(m));
```

### First, Last

```cpp
auto& First() noexcept;                 // the pair with the least key
const auto& First() const noexcept;
auto& Last() noexcept;                  // the pair with the greatest key
const auto& Last() const noexcept;
```

The dictionary must not be empty.

### IsEmpty, Count, Clear

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
void Clear() noexcept;
```

`Count()` is a stored count. `Clear` destroys every element at once and unlinks every node; the header stays.

### Add, Emplace, Set, operator[]

```cpp
bool Add(const Key& key, const Value& value);
bool Add(const Key& key, Value&& value);
bool Add(Key&& key, Value&& value);
template<class... A> bool Emplace(const Key& key, A&&... a);
template<class... A> bool Emplace(Key&& key, A&&... a);
void Set(const Key& key, const Value& value);
void Set(const Key& key, Value&& value);
Value& operator[](const Key& key);
Value& operator[](Key&& key);
```

As for [Dictionary](Dictionary.md#add-emplace): `Add` and `Emplace` add only when the key is new and say so, `Emplace` building the value in place; `Set` adds or replaces; `operator[]` adds a value-initialized value for a new key. O(log n) each.

```cpp
SortedDictionary<String, int> m;
bool added = m.Add("b", 2);              // true
added = m.Emplace("a", 1);               // true: built in place
added = m.Add("a", 9);                   // false: 1 stays
m.Set("a", 3);                           // replaced
++m["c"];                                // added as 0, now 1
```

### Remove, RemoveAt, RemoveRange, RemoveAll

```cpp
template<class K = Key> bool Remove(const K& key);
Iterator RemoveAt(ConstIterator pos);
Iterator RemoveRange(ConstIterator first, ConstIterator last);
template<class Pred> SizeType RemoveAll(Pred pred);
```

Destroys the element at once, unlinks and rebalances (the collector reclaims the node later) and, the iterator forms, returns the iterator after it. The key form says whether there was one.

```cpp
SortedDictionary<int, String> m = {{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}};
m.Remove(2);                                       // "b" is destroyed here
for (auto it = begin(m); it != end(m);) {
    it = it->first % 2 ? m.RemoveAt(it) : std::next(it);   // 1 and 3 go
}
auto n = m.RemoveAll([](const auto& p) { return p.first > 3; });   // 1: {4, "d"}; m is empty
```

### Take

```cpp
template<class K = Key> Optional<Value> Take(const K& key);
```

The value under the key, moved out, and the entry removed; `None` when the key is absent: what Java's `remove(key)` and C#'s `Remove(key, out value)` hand back. A `K` other than the key type looks up without building a key when the lookups are transparent (a `string_view` for a `String`).

```cpp
SortedDictionary<String, int> m = {{"a", 1}};
Optional<int> taken = m.Take("a");                 // 1, and m is empty
Optional<int> none = m.Take("a");                  // None
```

### Swap

```cpp
void Swap(SortedDictionary& other) noexcept;
void swap(SortedDictionary& l, SortedDictionary& r) noexcept;   // free function
```

Exchanges the trees, counts and comparators; no element is touched, and every iterator keeps pointing at its element, now in the other dictionary.

### Find, FindEntry, ContainsKey

```cpp
template<class K = Key> Value* Find(const K& key) noexcept;
template<class K = Key> const Value* Find(const K& key) const noexcept;
template<class K = Key> Iterator FindEntry(const K& key) noexcept;
template<class K = Key> ConstIterator FindEntry(const K& key) const noexcept;
template<class K = Key> bool ContainsKey(const K& key) const;
```

O(log n), reading raw pointers only. `Find` is the value as a pointer, null when the key is absent; `FindEntry` the pair as an iterator, `end(m)` when absent. A `K` other than the key type looks up without building a key for a transparent comparator, which `std::less` of a [String](../Core/String.md) is: a literal, a `StringView` or a `std::string_view` finds a `String` key.

```cpp
SortedDictionary<String, int> m = {{"apple", 1}};
bool has = m.ContainsKey("apple"); // no String is built for the literal
String line = "apple pie";
StringView key = line.View(0, 5);  // a piece of another string, a view holding its object
int* value = m.Find(key);          // *value == 1, nothing built either
```

### EqualRange, LowerBound, UpperBound

```cpp
template<class K> Iterator LowerBound(const K& key) noexcept;
template<class K> ConstIterator LowerBound(const K& key) const noexcept;
template<class K> Iterator UpperBound(const K& key) noexcept;
template<class K> ConstIterator UpperBound(const K& key) const noexcept;
template<class K> Range<Iterator> EqualRange(const K& key) noexcept;
template<class K> Range<ConstIterator> EqualRange(const K& key) const noexcept;
```

As in `std::map`: `LowerBound` is the first element not less than `key`, `UpperBound` the first greater, `EqualRange` both as a [Range](../Core/Range.md) (an empty or a one-element range).

```cpp
SortedDictionary<int, char> m = {{10, 'a'}, {20, 'b'}, {30, 'c'}};
auto from = m.LowerBound(15);     // 20
auto to = m.UpperBound(25);       // 30
for (auto it = from; it != to; ++it) { /* 20 only */ }
```

### Comparisons

```cpp
bool operator==(const SortedDictionary& l, const SortedDictionary& r);
auto operator<=>(const SortedDictionary& l, const SortedDictionary& r);
```

Element-wise, as for `std::map`: `==` compares counts first and then the elements in order; `<=>` is lexicographical with the synthesized three-way comparison (`<=>` of the element when it has one, else a `std::weak_ordering` built from `<`), so `!=`, `<`, `<=`, `>` and `>=` follow.

```cpp
SortedDictionary<int, int> a = {{1, 1}, {2, 2}}, b = {{1, 1}, {2, 3}};
bool less = a < b;                                // true
bool same = a == b;                               // false
auto ord = a <=> b;                               // std::strong_ordering::less
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The tree inside, as its own type: for the node handles (`extract`, `merge`) and `value_comp`.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Account {
    String owner;
    int balance;
    Ptr<Account> linked;          // a pointer inside an element: traced through the node
};

// The dictionary object lives inside a managed object: the nodes hang off
// it and die with it, elements included, when the Bank is collected.
struct Bank {
    SortedDictionary<String, Ptr<Account>> accounts;
};

int main() {
    Ptr bank = Make<Bank>();
    for (const char* name : {"carol", "alice", "bob"}) {
        // operator[] inserts a null Ptr; the object is made afterwards
        bank->accounts[name] = Make<Account>(name, 100);
    }
    bank->accounts["alice"]->linked = bank->accounts["bob"];
    bank->accounts["bob"]->linked = bank->accounts["alice"];   // a cycle: collected like any other

    // A dictionary on the stack, keys in order; the iterator follows the node
    SortedDictionary<String, int> balances;
    for (auto& [name, account] : bank->accounts) {             // alice bob carol
        balances.Emplace(name, account->balance);
    }
    auto bob = balances.FindEntry("bob");
    balances.Remove("alice");                                  // the int and the string die here
    balances.Set("carol", 250);
    std::cout << bob->first << " still there, " << balances.Count() << " balances\n";

    // Removing from the bank drops the node; alice and bob keep each other
    // reachable only through their cycle, which the collector breaks
    bank->accounts.Remove("alice");
    bank->accounts.Remove("bob");
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << bank->accounts.Count() << " account left, "
              << Collector::LiveObjectCount() << " live objects\n";
    return balances.Count() == 2 && bank->accounts.Count() == 1 ? 0 : 1;
}
```

The output:

```
bob still there, 2 balances
1 account left, 10 live objects
```

## See also

- [SortedMultiDictionary](SortedMultiDictionary.md) for equal keys, [SortedSet](SortedSet.md) and [SortedMultiSet](SortedMultiSet.md) for keys alone, [Dictionary](Dictionary.md) for a hash table
- [ConcurrentSortedDictionary](../Concurrent/ConcurrentSortedDictionary.md) for a sorted dictionary shared between threads
- [Ptr](../Core/Ptr.md), [UniquePtr](../Core/UniquePtr.md), [Make](../Core/Make.md)
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules), [README: Stack roots](../../../garbage_collector/overview.md#stack-roots)
