# sgcl::sorted_map

```cpp
#include "sgcl/containers/sorted_map.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class T, class Compare = std::less<Key>>
    class sorted_map;
}
```

`sgcl::sorted_map<Key, T, Compare>` is `std::map` on a red-black tree whose nodes are managed objects. The interface is the one of `std::map`: constructors, `insert`, `emplace`, `try_emplace`, `insert_or_assign`, `operator[]`, `at`, `erase`, `extract`, `merge`, node handles, the lookups with transparent comparators, bidirectional iterators (`std::ranges` algorithms work), `key_comp`/`value_comp`, `swap`, `==` and `<=>`, `std::erase_if`. The behaviour is the one of `std::map` too: unique keys in `Compare` order, an element destroyed the moment it is erased, iterators that stay valid until their element is erased.

What differs is where the memory lives. The map object holds one `tracked_ptr` (to a header node whose parent is the root, plus a count and the comparator), so it lives where a `tracked_ptr` may live. Every node is a managed object and the links between nodes are tracked pointers: the whole tree hangs off the header and is traced from there, so a `sorted_map<Key, tracked_ptr<T>>` or a map inside a managed object is traced like any other managed data, and a cycle through a map is collected like any other cycle. Nothing is freed by hand: an `erase` destroys the element and unlinks the node, and the collector reclaims the node's memory later. Iterators are one raw node pointer each, trivially copyable, and may live anywhere, a `std::vector` of them included: the map roots every node it holds, and a raw pointer in a stack frame is a root of its own under the conservative scan. A lookup, an iteration and an iterator copy read raw pointers only and pay no write barrier; an insertion, an erasure and a rebalancing store tracked pointers and pay the barrier on each link they relink ([README: Containers](README.md#containers)).

The header node is allocated on the first insertion, so an empty map costs nothing and the default constructor cannot throw.

## Rules

- A map holds a `tracked_ptr`, so it lives on a stack or inside a managed object: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame ([The rules](../core/README.md#the-rules), 1). The same holds for a node handle (`node_type`), which holds the node through a `tracked_ptr`.
- The elements may hold tracked pointers (`sgcl::sorted_map<int, sgcl::tracked_ptr<T>>`, a key that is a `tracked_ptr`): the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the map is destroyed, exactly as in `std::map`. The one exception is a map dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector thread ([README: Threads](../async/README.md#threads)).
- An iterator, a reference or a pointer to an element is valid while the element is in the map, across insertions, erasures of other elements, `swap`, `merge` and a move of the map (it follows the node). An iterator to an erased element is invalid as in `std`; it keeps the node's memory mapped but not the element.
- A `tracked_ptr` may point at an element of a map, or a member of one: a node is a managed object, and an alias to a part of a managed object is allowed ([The rules](../core/README.md#the-rules), 4). Such a pointer keeps the node alive, not the element, which dies with the erase.
- Thread safety is that of `std::map`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../core/README.md#the-rules), 6). The collector never waits for a mutator and never touches a node the map still links.

## Members

### Types

```cpp
using key_type = Key;
using mapped_type = T;
using value_type = std::pair<const Key, T>;
using key_compare = Compare;
using value_compare = /* compares two value_type by key with Compare */;
using size_type = size_t;
using difference_type = ptrdiff_t;
using reference = value_type&;
using const_reference = const value_type&;
using pointer = value_type*;
using const_pointer = const value_type*;
using iterator = /* bidirectional, one raw node pointer */;
using const_iterator = /* bidirectional, one raw node pointer */;
using reverse_iterator = std::reverse_iterator<iterator>;
using const_reverse_iterator = std::reverse_iterator<const_iterator>;
using node_type = /* the node handle, below */;
using insert_return_type = /* struct { iterator position; bool inserted; node_type node; } */;
static constexpr bool Multi = false;
```

`iterator` converts to `const_iterator`, not back. Both are trivially copyable and one word.

### Constructors

```cpp
map() noexcept(std::is_nothrow_default_constructible_v<Compare>);
explicit map(const Compare& comp);
template<std::input_iterator InputIt> map(InputIt first, InputIt last, const Compare& comp = Compare());
map(std::initializer_list<value_type> ilist, const Compare& comp = Compare());
map(const map& other);
map(map&& other) noexcept(std::is_nothrow_move_constructible_v<Compare>);
```

The default constructor allocates nothing. The range and list constructors insert in order, with `end()` as the hint, so sorted input costs one comparison per element; a key seen twice keeps its first value. A range of another type (pairs of a `string_view` and an `int` for `string` keys) is converted once per element, into its node, before the node's key is compared: the source need not be comparable with the keys at all. A copy has nodes of its own, in the same order: the tree copied shape for shape, a node per element with its colour and its links, no comparison and no rebalancing; a move takes the tree over and leaves `other` empty (and its comparator moved-from). An element constructor or comparator that throws while a constructor runs destroys the elements built so far and the map holds nothing.

```cpp
sorted_map<string, int> ages = {{"ann", 31}, {"bob", 27}};
sorted_map<int, int, std::greater<int>> desc(std::greater<int>{});       // 3 2 1 order
vector<pair<int, int>> src = {{2, 20}, {1, 10}};
sorted_map<int, int> from_range(src.begin(), src.end());
sorted_map<string, int> taken = std::move(ages);                   // ages is empty now
```

### Destructor

```cpp
~map();
```

Destroys the elements, in tree order, when the map dies on a stack or inside a managed object destroyed by hand. In a sweep (the map inside a dying managed object) it does nothing: the nodes are garbage of the same sweep and destroy their elements when the sweep reaches them. The node memory is reclaimed by the collector in both cases.

### operator=

```cpp
map& operator=(const map& other);
map& operator=(map&& other) noexcept(std::is_nothrow_move_assignable_v<Compare>);
map& operator=(std::initializer_list<value_type> ilist);
```

Copy assignment clears this map (destroying its elements at once), takes `other`'s comparator and copies its tree shape for shape, as the copy constructor does (an element copy that throws leaves the map empty); self-assignment is a no-op. Move assignment clears and takes the tree over. The list form clears and inserts.

```cpp
sorted_map<int, int> a = {{1, 1}}, b;
b = a;                       // copies
b = {{5, 5}, {6, 6}};        // the old elements die here
a = std::move(b);            // a holds 5 and 6, b is empty
```

### key_comp, value_comp

```cpp
key_compare key_comp() const;
value_compare value_comp() const;
```

Copies of the comparator: `key_comp()` compares keys, `value_comp()` compares two `value_type` by their keys.

```cpp
sorted_map<int, int> m = {{1, 1}, {2, 2}};
bool by_key = m.key_comp()(1, 2);                          // true
bool by_value = m.value_comp()(*m.begin(), *m.rbegin());   // true
```

### Iterators

```cpp
iterator begin() noexcept;                       const_iterator begin() const noexcept;
iterator end() noexcept;                         const_iterator end() const noexcept;
const_iterator cbegin() const noexcept;          const_iterator cend() const noexcept;
reverse_iterator rbegin() noexcept;              const_reverse_iterator rbegin() const noexcept;
reverse_iterator rend() noexcept;                const_reverse_iterator rend() const noexcept;
const_reverse_iterator crbegin() const noexcept; const_reverse_iterator crend() const noexcept;
```

`begin()` is the smallest key, in O(1) (the header keeps the leftmost node); `end()` is the header, and `--end()` the largest key. Before the first insertion there is no header: `begin()` and `end()` are both null iterators, equal to each other, neither of which may be dereferenced or moved, and an `end()` taken then does not compare equal to `end()` after the first insertion. An iterator is one raw node pointer: copying and advancing it costs a load, never a write barrier, and it may be kept in unmanaged memory (a `std::vector<iterator>`) for as long as its element is in the map.

```cpp
sorted_map<string, int> m = {{"b", 2}, {"a", 1}, {"c", 3}};
for (auto& [key, value] : m) {          // a b c
    value *= 10;
}
auto last = std::prev(m.end());         // "c"
std::vector<decltype(m)::iterator> kept;   // iterators in unmanaged memory: fine
kept.push_back(m.begin());
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

Destroys every element at once and unlinks every node; the header stays, so iterators taken from `end()` remain equal to `end()`. The nodes are reclaimed by the collector.

```cpp
sorted_map<int, string> m = {{1, "a"}, {2, "b"}};
m.clear();                     // both strings are destroyed here
bool gone = m.empty();         // true
```

### insert

```cpp
std::pair<iterator, bool> insert(const value_type& value);
std::pair<iterator, bool> insert(value_type&& value);
template<class P> requires std::is_constructible_v<value_type, P&&>
std::pair<iterator, bool> insert(P&& value);
iterator insert(const_iterator hint, const value_type& value);
iterator insert(const_iterator hint, value_type&& value);
template<class P> requires std::is_constructible_v<value_type, P&&>
iterator insert(const_iterator hint, P&& value);
template<std::input_iterator InputIt> void insert(InputIt first, InputIt last);
void insert(std::initializer_list<value_type> ilist);
insert_return_type insert(node_type&& nh);
iterator insert(const_iterator hint, node_type&& nh);
```

As in `std::map`. The single-element forms return the element with the key and whether it was inserted; nothing is built when the key is already there (the `P&&` forms go through `emplace`, which builds the element first and destroys it again on a duplicate). The hinted forms insert in O(1) amortized when the key belongs right before `hint`, and appending in sorted order at `end()` costs one comparison. The range and list forms insert one by one with `end()` as the hint; a range of another type is converted once per element, into the node, as `emplace_hint` would. The node-handle forms link the node of `nh` without copying the element: on success `nh` is empty afterwards; on a duplicate key the returned `node` (or `nh`, for the hinted form) keeps it, and `position` points at the element in the way. An empty handle inserts nothing (`position == end()`, `inserted == false`).

```cpp
sorted_map<string, int> m;
auto [it, inserted] = m.insert({"a", 1});                       // inserted: true
inserted = m.insert(std::pair<const char*, int>("a", 2)).second;   // false: "a" stays 1
m.insert(m.end(), {"z", 26});                                    // an append: one comparison
m.insert({{"b", 2}, {"c", 3}});
sorted_map<string, int> other = {{"q", 17}};
auto r = m.insert(other.extract("q"));                          // relinked, no copy: r.inserted is true
```

### emplace, emplace_hint

```cpp
template<class... A> std::pair<iterator, bool> emplace(A&&... a);
template<class... A> iterator emplace_hint(const_iterator hint, A&&... a);
```

Builds the `value_type` from `a...` in a new node before its place is known, as in `std`; if the key is already there the new element is destroyed and the existing one returned (the node is garbage for the collector). A comparator that throws destroys the new element and leaves the map as it was.

```cpp
sorted_map<string, string> m;
m.emplace("k", "v");
m.emplace(std::piecewise_construct, std::forward_as_tuple("p"), std::forward_as_tuple(3, 'x'));   // "p" -> "xxx"
auto it = m.emplace_hint(m.end(), "z", "last");
```

### try_emplace

```cpp
template<class... A> std::pair<iterator, bool> try_emplace(const key_type& key, A&&... a);
template<class... A> std::pair<iterator, bool> try_emplace(key_type&& key, A&&... a);
template<class... A> iterator try_emplace(const_iterator hint, const key_type& key, A&&... a);
template<class... A> iterator try_emplace(const_iterator hint, key_type&& key, A&&... a);
```

Looks the key up first and builds the mapped value from `a...` in place only when the key is new: the mapped type need not be movable or copyable, and `a...` are not touched on a duplicate.

```cpp
struct Pinned {
    int value;
    explicit Pinned(int v) : value(v) {}
    Pinned(const Pinned&) = delete;
    Pinned& operator=(const Pinned&) = delete;
};
sorted_map<int, Pinned> m;
m.try_emplace(1, 10);                                       // Pinned(10) built inside the node
auto [it, fresh] = m.try_emplace(1, 11);                    // fresh: false; no Pinned(11) was built
m.try_emplace(m.end(), 2, 20);                              // the hinted form returns an iterator
sorted_map<string, std::unique_ptr<int>> owners;
owners.try_emplace("a", new int(1));                        // a mapped type that cannot be copied
```

### insert_or_assign

```cpp
template<class M> std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj);
template<class M> std::pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj);
template<class M> iterator insert_or_assign(const_iterator hint, const key_type& key, M&& obj);
template<class M> iterator insert_or_assign(const_iterator hint, key_type&& key, M&& obj);
```

Inserts `{key, obj}` when the key is new (`true`), otherwise assigns `obj` to the mapped value in place (`false`).

```cpp
sorted_map<string, int> m;
m.insert_or_assign("a", 1);                 // inserted
auto [it, inserted] = m.insert_or_assign("a", 2);   // assigned: inserted is false, it->second is 2
```

### at, operator[]

```cpp
mapped_type& at(const key_type& key);
const mapped_type& at(const key_type& key) const;
template<class K> mapped_type& at(const K& key);               // when Compare::is_transparent
template<class K> const mapped_type& at(const K& key) const;   //   "
mapped_type& operator[](const key_type& key);
mapped_type& operator[](key_type&& key);
```

`at` throws `std::out_of_range` when the key is absent; with a transparent comparator it takes a key of another type (a `string_view` for a `string`), as the other lookups do. `operator[]` inserts a value-initialized mapped value for a new key (built in place, through `try_emplace`) and returns a reference to it; for a `tracked_ptr` mapped type that is a null pointer.

```cpp
sorted_map<string, int> counts;
++counts["x"];                       // inserted as 0, now 1
++counts["x"];                       // 2
int x = counts.at("x");              // 2
try { counts.at("y"); } catch (const std::out_of_range&) { /* absent */ }
```

### erase

```cpp
iterator erase(iterator pos);
iterator erase(const_iterator pos);
iterator erase(const_iterator first, const_iterator last);
size_type erase(const key_type& key);
```

Destroys the element at once, unlinks the node (the collector reclaims it later) and returns the iterator after it. Erasing `[begin(), end())` is a `clear()`. The key form returns 0 or 1. Erasing during an iteration is done as with `std::map`: `it = m.erase(it)`. There is no transparent `erase`: the key form takes a `key_type`.

### take

```cpp
optional<mapped_type> take(const key_type& key);
template<class K> optional<mapped_type> take(const K& key);   // when Compare::is_transparent
```

The mapped value under the key, moved out, and the element erased; `nullopt` when the key is absent. What Java's `remove(key)` and C#'s `Remove(key, out value)` hand back, and what `std` has no word for (`extract` gives the node, `erase` a count).

```cpp
sorted_map<string, int> m = {{"a", 1}};
optional<int> taken = m.take("a");           // 1, and m is empty
optional<int> none = m.take("a");            // nullopt
```

```cpp
sorted_map<int, string> m = {{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}};
m.erase(2);                                        // "b" is destroyed here
for (auto it = m.begin(); it != m.end();) {
    it = it->first % 2 ? m.erase(it) : std::next(it);   // 1 and 3 go
}
auto rest = m.size();                              // 1: {4, "d"}
```

### swap

```cpp
void swap(map& other) noexcept(std::is_nothrow_swappable_v<Compare>);
friend void swap(map& lhs, map& rhs) noexcept(noexcept(lhs.swap(rhs)));   // free function in namespace sgcl
```

Exchanges the trees, counts and comparators; no element is touched, and every iterator keeps pointing at its element, now in the other map.

```cpp
sorted_map<int, int> a = {{1, 1}}, b = {{2, 2}};
auto it = a.begin();
swap(a, b);                  // it still points at {1, 1}, which is in b now
bool moved = it == b.begin();   // true
```

### extract

```cpp
node_type extract(iterator pos);
node_type extract(const_iterator pos);
node_type extract(const key_type& key);
```

Unlinks the node and hands it over in a node handle, the element untouched: the handle is the owner now, and destroys the element if it dies unused. The key form returns an empty handle when the key is absent. See [node_type](#node_type-the-node-handle).

```cpp
sorted_map<int, string> m = {{1, "a"}, {2, "b"}};
auto nh = m.extract(1);           // m holds {2, "b"}; nh holds {1, "a"}
nh.key() = 7;                     // the key may change outside a map
m.insert(std::move(nh));          // {2, "b"}, {7, "a"}; no string was copied
```

### merge

```cpp
template<class Traits2> void merge(detail::RbTree<Traits2>& source);    // any sorted_map or sorted_multimap<Key, T, C2>
template<class Traits2> void merge(detail::RbTree<Traits2>&& source);
```

Relinks the nodes of `source` whose keys are not yet here, in `source`'s order, into this map; a node whose key is already here stays in `source`. No element is copied or destroyed, and every iterator follows its node. `source` may be a `sgcl::sorted_map` or `sgcl::sorted_multimap` with the same `Key` and `T` and any comparator. Merging a map into itself does nothing.

```cpp
sorted_map<int, int> a = {{1, 1}, {3, 3}};
sorted_multimap<int, int> b = {{2, 2}, {3, 30}, {3, 31}};
a.merge(b);                       // a: 1 2 3;  b keeps both 3s
```

### count, find, contains

```cpp
size_type count(const key_type& key) const;
iterator find(const key_type& key);
const_iterator find(const key_type& key) const;
bool contains(const key_type& key) const;
template<class K> size_type count(const K& key) const;                 // when Compare::is_transparent
template<class K> iterator find(const K& key);                          //   "
template<class K> const_iterator find(const K& key) const;              //   "
template<class K> bool contains(const K& key) const;                    //   "
```

O(log n), reading raw pointers only. `count` is 0 or 1. The `K` overloads exist for a transparent comparator, which `std::less` of a [string](../core/string.md) is: a literal, a `sgcl::string_view` or a `std::string_view` looks up a `sgcl::string` key without building one.

```cpp
sorted_map<string, int> m = {{"apple", 1}};
bool has = m.contains("apple");    // no string is built for the literal
string line = "apple pie";
string_slice key = line.as_slice(0, 5);   // a piece of another string, a slice holding its object
auto it = m.find(key);             // it->second == 1, nothing built either
```

### equal_range, lower_bound, upper_bound

```cpp
std::pair<iterator, iterator> equal_range(const key_type& key);
std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const;
iterator lower_bound(const key_type& key);
const_iterator lower_bound(const key_type& key) const;
iterator upper_bound(const key_type& key);
const_iterator upper_bound(const key_type& key) const;
template<class K> ... equal_range(const K& key);      // when Compare::is_transparent, same set of overloads
template<class K> ... lower_bound(const K& key);      //   "
template<class K> ... upper_bound(const K& key);      //   "
```

As in `std::map`: `lower_bound` is the first element not less than `key`, `upper_bound` the first greater, `equal_range` both (an empty or a one-element range).

```cpp
sorted_map<int, char> m = {{10, 'a'}, {20, 'b'}, {30, 'c'}};
auto from = m.lower_bound(15);    // 20
auto to = m.upper_bound(25);      // 30
for (auto it = from; it != to; ++it) { /* 20 only */ }
```

### The mixins

`sorted_map` carries [mixin::enumerable](../core/mixin/enumerable.md) (`exists`, `all`, `count_of`, `find_if`, `for_each`, `index_of` over the pairs; `contains`, `min` and `max` are the map's own, by the key and as the ends of the order), [mixin::equatable](../core/mixin/equatable.md), [mixin::comparable](../core/mixin/comparable.md), the bidirectional category, and [mixin::lookup](../core/mixin/lookup.md): the reads by the key that `std::map` makes a program write by hand.

```cpp
sorted_map<string, int> ports = {{"http", 80}, {"https", 443}};
assert(*ports.get("http") == 80 && !ports.get("ftp") && ports.value_or("ftp", 21) == 21 && ports.contains_key("https"));
if (int* p = ports.try_get("http")) {
    *p = 8080;
}
assert(ports.exists([](const auto& kv) { return kv.second > 1000; }) && ports.min().first == "http");
```

### Comparisons

```cpp
friend bool operator==(const map& lhs, const map& rhs);
friend auto operator<=>(const map& lhs, const map& rhs);
```

Element-wise, as for `std::map`: `==` compares sizes first and then the elements in order; `<=>` is lexicographical with the synthesized three-way comparison (`<=>` of the element when it has one, else a `std::weak_ordering` built from `<`), so `!=`, `<`, `<=`, `>` and `>=` follow.

```cpp
sorted_map<int, int> a = {{1, 1}, {2, 2}}, b = {{1, 1}, {2, 3}};
bool less = a < b;                                // true
bool same = a == b;                               // false
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
    using key_type = Key;
    using mapped_type = T;
    node_type() noexcept;
    node_type(node_type&&) noexcept;
    node_type& operator=(node_type&&) noexcept;
    ~node_type();
    [[nodiscard]] bool empty() const noexcept;
    explicit operator bool() const noexcept;
    key_type& key() const noexcept;        // writable: the node is out of any map
    mapped_type& mapped() const noexcept;
};
```

Owns one unlinked node, as `std::map::node_type` does: movable, not copyable; the element is destroyed when the handle dies without having been inserted (the node's memory is the collector's). The handle holds the node through a `tracked_ptr`, so it lives on a stack or inside a managed object, like the map. There is no `swap` member.

### std::erase_if

```cpp
namespace sgcl {
    template<class Key, class T, class Compare, class Pred>
    typename sorted_map<Key, T, Compare>::size_type erase_if(sorted_map<Key, T, Compare>& c, Pred pred);
}
namespace std { using sgcl::erase_if; }
```

Erases every element for which `pred(*it)` is true and returns how many; each erased element is destroyed at once.

```cpp
sorted_map<int, int> m = {{1, 1}, {2, 2}, {3, 3}, {4, 4}};
auto n = std::erase_if(m, [](const auto& p) { return p.first % 2 == 0; });   // 2; m is 1 3
```

### Deduction guides

```cpp
template<std::input_iterator InputIt, class Compare = std::less<Key>>   // Key and T from the iterator's pair
map(InputIt, InputIt, Compare = Compare()) -> sorted_map<Key, T, Compare>;
template<class Key, class T, class Compare = std::less<Key>>
map(std::initializer_list<std::pair<Key, T>>, Compare = Compare()) -> sorted_map<Key, T, Compare>;
```

```cpp
sorted_map m = {std::pair{1, 2.0}, std::pair{2, 3.0}};     // sorted_map<int, double>: the pairs spelled out
sorted_map from_range(m.begin(), m.end());                  // sorted_map<int, double>
sorted_map greater({std::pair{1, 2}}, std::greater<int>());     // sorted_map<int, int, std::greater<int>>
```

From an iterator pair or an initializer list, as for `std::map`; an initializer list of a map spells its pairs out (`std::pair{1, 2.0}`), since a braced pair alone names no type.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

struct Account {
    string owner;
    int balance;
    tracked_ptr<Account> linked;     // a pointer inside an element: traced through the node
};

// The map object lives inside a managed object: the nodes hang off it and
// die with it, elements included, when the Bank is collected.
struct Bank {
    sorted_map<string, tracked_ptr<Account>> accounts;
};

int main() {
    tracked_ptr bank = make_tracked<Bank>();
    for (const char* name : {"carol", "alice", "bob"}) {
        // operator[] inserts a null tracked_ptr; the object is made afterwards
        bank->accounts[name] = make_tracked<Account>(name, 100);
    }
    bank->accounts["alice"]->linked = bank->accounts["bob"];
    bank->accounts["bob"]->linked = bank->accounts["alice"];   // a cycle: collected like any other

    // A map on the stack, keys in order; the iterator follows the node
    sorted_map<string, int> balances;
    for (auto& [name, account] : bank->accounts) {             // alice bob carol
        balances.emplace(name, account->balance);
    }
    auto bob = balances.find("bob");
    balances.erase("alice");                                   // the int and the string die here
    balances.insert_or_assign("carol", 250);
    std::cout << bob->first << " still there, " << balances.size() << " balances\n";

    // Erasing from the bank drops the node; alice and bob keep each other
    // reachable only through their cycle, which the collector breaks
    bank->accounts.erase("alice");
    bank->accounts.erase("bob");
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    std::cout << bank->accounts.size() << " account left, "
              << collector::get_live_object_count() << " live objects\n";
    return balances.size() == 2 && bank->accounts.size() == 1 ? 0 : 1;
}
```

The output:

```
bob still there, 2 balances
1 account left, 10 live objects
```

## See also

- [sorted_multimap](sorted_multimap.md) for equal keys, [sorted_set](sorted_set.md) and [sorted_multiset](sorted_multiset.md) for keys alone, [map](map.md) for a hash table
- [tracked_ptr](../core/tracked_ptr.md), [unique_ptr](../core/unique_ptr.md), [make_tracked](../core/make_tracked.md)
- [README: Containers](README.md#containers), [README: The rules](../core/README.md#the-rules), [README: Stack roots](../../garbage_collector/overview.md#stack-roots)
- `examples/example.cpp`
