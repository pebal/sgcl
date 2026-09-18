# sgcl::set

```cpp
#include "sgcl/containers/set.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class Compare = std::less<Key>>
    class set;
}
```

The same class in the `Sgcl` interface: [SortedSet](../Sgcl/Containers/SortedSet.md).

`sgcl::set<Key, Compare>` is `std::set` on a red-black tree whose nodes are managed objects: the same tree as [map](map.md), holding keys alone. The interface is the one of `std::set` (constructors, `insert`, `emplace`, `erase`, `extract`, `merge`, node handles, the lookups with transparent comparators, bidirectional iterators, `key_comp`/`value_comp`, `swap`, `==` and `<=>`, `std::erase_if`), and so is the behaviour: unique keys in `Compare` order, elements that cannot be modified through an iterator (`iterator` is `const_iterator`), an element destroyed the moment it is erased.

What differs from `std` is where the memory lives. The set object holds one `tracked_ptr` (to a header node), a count and the comparator, so it lives where a `tracked_ptr` may live; the nodes are managed objects linked by tracked pointers and traced from the header, so a `set<tracked_ptr<T>>` is a set of traced pointers (`tracked_ptr` compares with `<=>` and hashes, so it is a key as it is), and a cycle through a set is collected like any other. Nothing is freed by hand: an `erase` destroys the element and unlinks the node, the collector reclaims the node later. Iterators are one raw node pointer each, trivially copyable, storable anywhere, valid while their element is in the set. Lookups and iteration read raw pointers and pay no write barrier; insertions, erasures and rebalancing store tracked pointers and pay the barrier on each link they relink ([README: Containers](README.md#containers)). The header is allocated on the first insertion: an empty set costs nothing.

## Rules

- A set holds a `tracked_ptr`, so it lives on a stack or inside a managed object: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame ([The rules](../core/README.md#the-rules), 1). The same holds for a node handle.
- The elements may be, or hold, tracked pointers: the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the set is destroyed, exactly as in `std`. The one exception is a set dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector thread.
- An iterator, a reference or a pointer to an element is valid while the element is in the set, across insertions, erasures of other elements, `swap`, `merge` and a move of the set. An iterator to an erased element is invalid as in `std`.
- A `tracked_ptr` may point at an element (a node is a managed object); it keeps the node alive, not the element.
- Thread safety is that of `std::set`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../core/README.md#the-rules), 6).

## Members

### Types

```cpp
using key_type = Key;
using value_type = Key;
using key_compare = Compare;
using value_compare = Compare;
using size_type = size_t;
using difference_type = ptrdiff_t;
using reference = value_type&;
using const_reference = const value_type&;
using pointer = value_type*;
using const_pointer = const value_type*;
using iterator = /* bidirectional, one raw node pointer, yields const Key& */;
using const_iterator = iterator;
using reverse_iterator = std::reverse_iterator<iterator>;
using const_reverse_iterator = std::reverse_iterator<const_iterator>;
using node_type = /* the node handle, below */;
using insert_return_type = /* struct { iterator position; bool inserted; node_type node; } */;
static constexpr bool Multi = false;
```

### Constructors

```cpp
set() noexcept(std::is_nothrow_default_constructible_v<Compare>);
explicit set(const Compare& comp);
template<std::input_iterator InputIt> set(InputIt first, InputIt last, const Compare& comp = Compare());
set(std::initializer_list<value_type> ilist, const Compare& comp = Compare());
set(const set& other);
set(set&& other) noexcept(std::is_nothrow_move_constructible_v<Compare>);
```

The default constructor allocates nothing. The range and list constructors insert in order with `end()` as the hint (sorted input costs one comparison per element); duplicates are dropped. A range of another type (`string_view`s for `string` keys) is converted once per element, into its node, before the node's key is compared: the source need not be comparable with the keys at all. A copy has nodes of its own: the tree copied shape for shape, a node per element with its colour and its links, no comparison and no rebalancing; a move takes the tree over and leaves `other` empty. A constructor or comparator that throws destroys the elements built so far.

```cpp
sgcl::set primes = {5, 3, 2, 7, 3};                          // 2 3 5 7
sgcl::set<int, std::greater<int>> desc(std::greater<int>{});
sgcl::vector<sgcl::string> words = {"b", "a"};
sgcl::set<sgcl::string> from_range(words.begin(), words.end());    // a b
sgcl::set<int> taken = std::move(primes);                         // primes is empty now
```

### Destructor

```cpp
~set();
```

Destroys the elements when the set dies on a stack or inside a managed object destroyed by hand; in a sweep it leaves the nodes to the same sweep, which destroys the elements. The node memory is reclaimed by the collector in both cases.

### operator=

```cpp
set& operator=(const set& other);
set& operator=(set&& other) noexcept(std::is_nothrow_move_assignable_v<Compare>);
set& operator=(std::initializer_list<value_type> ilist);
```

Copy assignment clears this set (destroying its elements at once), takes `other`'s comparator and copies its tree shape for shape, as the copy constructor does (an element copy that throws leaves the set empty); move assignment clears and takes the tree over; the list form clears and inserts.

```cpp
sgcl::set<int> a = {1, 2}, b;
b = a;
b = {5, 6};                  // the old elements die here
a = std::move(b);            // a is 5 6, b is empty
```

### key_comp, value_comp

```cpp
key_compare key_comp() const;
value_compare value_comp() const;
```

Copies of the comparator (the same type for both, as in `std::set`).

```cpp
sgcl::set s = {1, 2};
bool less = s.value_comp()(*s.begin(), *s.rbegin());   // true
```

### Iterators

```cpp
iterator begin() const noexcept;                 const_iterator cbegin() const noexcept;
iterator end() const noexcept;                   const_iterator cend() const noexcept;
reverse_iterator rbegin() const noexcept;        const_reverse_iterator crbegin() const noexcept;
reverse_iterator rend() const noexcept;          const_reverse_iterator crend() const noexcept;
```

`iterator` and `const_iterator` are one type, yielding `const Key&`: a key is never modified in place. `begin()` is the smallest key in O(1), `--end()` the largest. Before the first insertion `begin()` and `end()` are both null iterators, equal to each other, neither of which may be dereferenced or moved; an `end()` taken then does not compare equal to `end()` after the first insertion. Iterators are raw node pointers: copying and advancing costs a load, and they may be kept in unmanaged memory (a `std::vector<iterator>`) while their element is in the set.

```cpp
sgcl::set<sgcl::string> s = {"b", "a", "c"};
sgcl::string joined;
for (const auto& key : s) {             // a b c
    joined += key;
}
auto largest = std::prev(s.end());      // "c"
```

### empty, size, max_size

```cpp
[[nodiscard]] bool empty() const noexcept;
size_type size() const noexcept;
size_type max_size() const noexcept;
```

`size()` is a stored count, O(1).

### clear

```cpp
void clear() noexcept;
```

Destroys every element at once and unlinks every node; the header stays. The nodes are reclaimed by the collector.

```cpp
sgcl::set<sgcl::string> s = {"a", "b"};
s.clear();                     // both strings are destroyed here
bool gone = s.empty();         // true
```

### insert

```cpp
std::pair<iterator, bool> insert(const value_type& value);
std::pair<iterator, bool> insert(value_type&& value);
iterator insert(const_iterator hint, const value_type& value);
iterator insert(const_iterator hint, value_type&& value);
template<std::input_iterator InputIt> void insert(InputIt first, InputIt last);
void insert(std::initializer_list<value_type> ilist);
insert_return_type insert(node_type&& nh);
iterator insert(const_iterator hint, node_type&& nh);
```

As in `std::set`: the single-element forms return the element with the key and whether it was inserted, and nothing is built for a key already there. The hinted forms are O(1) amortized when the key belongs right before `hint`; an append in sorted order at `end()` costs one comparison. The range and list forms insert one by one with `end()` as the hint; a range of another type is converted once per element, into the node, as `emplace_hint` would. The node-handle forms link the node of `nh` without copying the element: on success `nh` is empty afterwards; on a duplicate key the returned `node` (or `nh`, for the hinted form) keeps it and `position` is the element in the way. An empty handle inserts nothing (`position == end()`, `inserted == false`).

```cpp
sgcl::set<sgcl::string> s;
auto [it, inserted] = s.insert("a");                  // inserted: true
inserted = s.insert("a").second;                      // false
s.insert(s.end(), "z");                               // an append: one comparison
s.insert({"b", "c"});
sgcl::set<sgcl::string> other = {"q"};
auto r = s.insert(other.extract("q"));                // relinked, no copy: r.inserted is true
```

### emplace, emplace_hint

```cpp
template<class... A> std::pair<iterator, bool> emplace(A&&... a);
template<class... A> iterator emplace_hint(const_iterator hint, A&&... a);
```

Builds the key from `a...` in a new node before its place is known, as in `std`; if the key is already there the new element is destroyed and the existing one returned. A comparator that throws destroys the new element and leaves the set as it was.

```cpp
sgcl::set<sgcl::string> s;
s.emplace(3, 'x');                          // "xxx"
auto [it, fresh] = s.emplace("xxx");        // fresh: false
s.emplace_hint(s.end(), "zzz");
```

### erase

```cpp
iterator erase(const_iterator pos);
iterator erase(const_iterator first, const_iterator last);
size_type erase(const key_type& key);
```

Destroys the element at once, unlinks the node (the collector reclaims it later) and returns the iterator after it. Erasing `[begin(), end())` is a `clear()`. The key form returns 0 or 1. There is no transparent `erase`.

```cpp
sgcl::set s = {1, 2, 3, 4};
s.erase(2);
for (auto it = s.begin(); it != s.end();) {
    it = *it % 2 ? s.erase(it) : std::next(it);   // 1 and 3 go
}
auto rest = s.size();                              // 1
```

### swap

```cpp
void swap(set& other) noexcept(std::is_nothrow_swappable_v<Compare>);
friend void swap(set& lhs, set& rhs) noexcept(noexcept(lhs.swap(rhs)));   // free function in namespace sgcl
```

Exchanges the trees, counts and comparators; no element is touched, and every iterator keeps pointing at its element, now in the other set.

```cpp
sgcl::set<int> a = {1}, b = {2};
auto it = a.begin();
swap(a, b);                       // it still points at 1, which is in b now
bool moved = it == b.begin();     // true
```

### extract

```cpp
node_type extract(const_iterator pos);
node_type extract(const key_type& key);
```

Unlinks the node and hands it over in a node handle, the element untouched; the handle destroys the element if it dies unused. The key form returns an empty handle when the key is absent. See [node_type](#node_type-the-node-handle).

```cpp
sgcl::set<sgcl::string> s = {"a", "b"};
auto nh = s.extract("a");         // s holds "b"
nh.value() += "!";                // the key may change outside a set
s.insert(std::move(nh));          // "a!" "b"; no string was copied
```

### merge

```cpp
template<class Traits2> void merge(detail::RbTree<Traits2>& source);    // any sgcl::set or multiset<Key, C2>
template<class Traits2> void merge(detail::RbTree<Traits2>&& source);
```

Relinks the nodes of `source` whose keys are not yet here into this set; a node whose key is already here stays in `source`. No element is copied or destroyed; iterators follow their nodes. `source` may be a `sgcl::set` or `sgcl::multiset` with the same `Key` and any comparator.

```cpp
sgcl::set a = {1, 3};
sgcl::multiset b = {2, 3, 3};
a.merge(b);                       // a: 1 2 3;  b keeps both 3s
```

### count, find, contains

```cpp
size_type count(const key_type& key) const;
iterator find(const key_type& key) const;
bool contains(const key_type& key) const;
template<class K> size_type count(const K& key) const;        // when Compare::is_transparent
template<class K> iterator find(const K& key) const;          //   "
template<class K> bool contains(const K& key) const;          //   "
```

O(log n), reading raw pointers only. `count` is 0 or 1. The `K` overloads exist for a transparent comparator, which `std::less` of a [string](../core/string.md) is.

```cpp
sgcl::set<sgcl::string> s = {"apple"};
bool has = s.contains("apple");    // no sgcl::string is built for the literal
sgcl::string line = "apple pie";
bool piece = s.contains(line.view(0, 5));   // a view of another string, nothing built either
```

### equal_range, lower_bound, upper_bound

```cpp
std::pair<iterator, iterator> equal_range(const key_type& key) const;
iterator lower_bound(const key_type& key) const;
iterator upper_bound(const key_type& key) const;
template<class K> std::pair<iterator, iterator> equal_range(const K& key) const;   // when Compare::is_transparent
template<class K> iterator lower_bound(const K& key) const;                        //   "
template<class K> iterator upper_bound(const K& key) const;                        //   "
```

As in `std::set`: `lower_bound` is the first element not less than `key`, `upper_bound` the first greater, `equal_range` both.

```cpp
sgcl::set s = {10, 20, 30};
auto from = s.lower_bound(15);    // 20
auto to = s.upper_bound(25);      // 30
auto between = std::distance(from, to);   // 1
```

### Comparisons

```cpp
friend bool operator==(const set& lhs, const set& rhs);
friend auto operator<=>(const set& lhs, const set& rhs);
```

Element-wise in order, as for `std::set`: `==` compares sizes first; `<=>` is lexicographical with the synthesized three-way comparison, so `!=`, `<`, `<=`, `>` and `>=` follow.

```cpp
sgcl::set<int> a = {1, 2}, b = {1, 3};
bool less = a < b;                                // true
auto ord = a <=> b;                               // std::strong_ordering::less
```

### _check

```cpp
bool _check() const;
```

Verifies the red-black invariants, the header links, the order and the count; for the tests. O(n).

### node_type (the node handle)

```cpp
class node_type {
public:
    using value_type = Key;
    node_type() noexcept;
    node_type(node_type&&) noexcept;
    node_type& operator=(node_type&&) noexcept;
    ~node_type();
    [[nodiscard]] bool empty() const noexcept;
    explicit operator bool() const noexcept;
    value_type& value() const noexcept;    // writable: the node is out of any set
};
```

Owns one unlinked node: movable, not copyable; the element is destroyed when the handle dies without having been inserted. The handle holds the node through a `tracked_ptr`, so it lives on a stack or inside a managed object. It is the same handle type as `sgcl::multiset<Key>::node_type`. There is no `swap` member.

### std::erase_if

```cpp
namespace sgcl {
    template<class Key, class Compare, class Pred>
    typename set<Key, Compare>::size_type erase_if(set<Key, Compare>& c, Pred pred);
}
namespace std { using sgcl::erase_if; }
```

Erases every element for which `pred(*it)` is true and returns how many.

```cpp
sgcl::set s = {1, 2, 3, 4};
auto n = std::erase_if(s, [](int x) { return x % 2 == 0; });   // 2; s is 1 3
```

### Deduction guides

```cpp
template<std::input_iterator InputIt, class Compare = std::less<Key>>   // Key from the iterator
set(InputIt, InputIt, Compare = Compare()) -> set<Key, Compare>;
template<class Key, class Compare = std::less<Key>>
set(std::initializer_list<Key>, Compare = Compare()) -> set<Key, Compare>;
```

```cpp
sgcl::set s = {3, 1, 2};                                // set<int>
sgcl::vector<sgcl::string> src = {"b", "a"};
sgcl::set from_range(src.begin(), src.end());           // set<sgcl::string>
sgcl::set greater({3, 1}, std::greater<int>());         // set<int, std::greater<int>>
```

From an iterator pair or an initializer list, as for `std::set`; an initializer list of a map spells its pairs out (`std::pair{1, 2.0}`), since a braced pair alone names no type.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

struct Node {
    sgcl::string name;
    sgcl::set<sgcl::tracked_ptr<Node>> peers;       // a set of traced pointers inside a managed object
};

int main() {
    // A graph whose edges are sets: each node is reachable from its peers
    sgcl::tracked_ptr a = sgcl::make_tracked<Node>("a");
    sgcl::tracked_ptr b = sgcl::make_tracked<Node>("b");
    sgcl::tracked_ptr c = sgcl::make_tracked<Node>("c");
    a->peers.insert(b);
    b->peers.insert(a);                         // a cycle
    b->peers.insert(c);
    b->peers.insert(c);                         // a duplicate: nothing is inserted

    // A set of values on the stack: keys in order, each exactly once
    sgcl::set<sgcl::string> names;
    for (const auto& peer : b->peers) {         // pointers compare by address: any order
        names.insert(peer->name);
    }
    names.insert("b");
    std::cout << "b's neighbourhood:";
    for (const auto& n : names) {
        std::cout << ' ' << n;                  // a b c
    }
    std::cout << '\n';

    // Dropping the stack roots: a and b keep each other alive only through
    // their sets, which the collector sees as a cycle
    std::size_t node_count = names.size();
    a = b = c = nullptr;
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    sgcl::collector::force_collect(true);
    std::cout << sgcl::collector::get_live_object_count() << " live objects\n";     // the header and three nodes of `names`
    return node_count == 3 ? 0 : 1;
}
```

The output:

```
b's neighbourhood: a b c
7 live objects
```

## See also

- [multiset](multiset.md) for equal keys, [map](map.md) and [multimap](multimap.md) for key-value pairs, [unordered_set](unordered_set.md) for a hash table
- [tracked_ptr](../core/tracked_ptr.md), [make_tracked](../core/make_tracked.md)
- [README: Containers](README.md#containers), [README: The rules](../core/README.md#the-rules)
