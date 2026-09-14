# sgcl::unordered_set

```cpp
#include "sgcl/unordered_set.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>, template<class> class Ptr = tracked_ptr>
    class unordered_set;
}
```

`sgcl::unordered_set<Key, Hash, KeyEqual>` is `std::unordered_set` over managed nodes: the same hash table as [unordered_map](unordered_map.md), holding keys alone. The interface is the one of `std::unordered_set` (constructors, `insert`, `emplace`, `erase`, `extract`, `merge`, node handles, the lookups with transparent hash and equality, forward iterators, the bucket interface, `load_factor`/`max_load_factor`/`rehash`/`reserve`, `hash_function`/`key_eq`, `swap`, `==`, deduction guides, `std::erase_if`), and so is the behaviour: unique keys, an element destroyed the moment it is erased, iterators valid across a rehash and until their element is erased.

`Ptr`, the last parameter, is the kind of the word by which the container holds its memory: `tracked_ptr` by default, so that the container lives where a `tracked_ptr` may (on a stack or inside a managed object), or [`gc::tracked_ptr`](gc/tracked_ptr.md), so that it lives anywhere, at the cost of a `gc::tracked_ptr` on each access to that word; `gc::unordered_set` ([gc/gc.h](README.md#the-gc-namespace)) names the latter. The nodes and buffers are the same managed objects either way, and the elements are a choice apart; an element type that names a `tracked_type` (`gc::tracked_ptr<T>` names `sgcl::tracked_ptr<T>`) is stored as that type, one word in the same mode, and handed out as the type it was given, so a container of `gc::tracked_ptr`s costs what one of `sgcl::tracked_ptr`s does ([the gc namespace](README.md#the-gc-namespace)).

What differs from `std` is where the memory lives. The set holds two `tracked_ptr`s (the bucket array and a sentinel node), the counts, the hasher and the equality, so it lives where a `tracked_ptr` may live; the elements are nodes on the managed heap, forming one chain linked by tracked pointers and traced from the sentinel, so a `unordered_set<tracked_ptr<T>>` is a set of traced pointers (`std::hash<sgcl::tracked_ptr<T>>` hashes the address) and a cycle through a set is collected like any other. Nothing is freed by hand: an `erase` destroys the element and unlinks the node, the collector reclaims the node later. The hash of each key is cached in its node. The bucket count is 0 or a power of two, and the table grows when the size reaches `bucket_count() * max_load_factor()`, doubling at least, to eight buckets at the least. Iterators are one raw node pointer each, trivially copyable, storable anywhere. As in `std`, `iterator` and `const_iterator` are one type, yielding `const Key&`: a key is never modified in place (the cached hash and the bucket would no longer match); `extract` it and insert it back. Lookups and iteration pay no write barrier; insertions, erasures and rehashes store tracked pointers and pay the barrier on each link they relink ([README: Containers](../README.md#containers)).

## Rules

- An `unordered_set` holds tracked pointers, so it lives on a stack or inside a managed object: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame ([The rules](../README.md#the-rules), 1). The same holds for a node handle.
- The elements may be, or hold, tracked pointers: the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the set is destroyed, exactly as in `std`. The one exception is a set dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector thread.
- An iterator, a reference or a pointer to an element is valid while the element is in the set, across insertions, rehashes, erasures of other elements, `swap`, `merge` and a move of the set. An iterator to an erased element is invalid as in `std`.
- A key cannot be modified through an iterator (they yield `const Key&`); `extract` it, change `value()` of the handle and insert it back.
- A `tracked_ptr` may point at an element (a node is a managed object); it keeps the node alive, not the element.
- Thread safety is that of `std::unordered_set`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../README.md#the-rules), 6).

## Members

### Types

```cpp
using key_type = Key;
using value_type = Key;
using hasher = Hash;
using key_equal = KeyEqual;
using size_type = size_t;
using difference_type = ptrdiff_t;
using reference = value_type&;
using const_reference = const value_type&;
using pointer = value_type*;
using const_pointer = const value_type*;
using const_iterator = /* forward, one raw node pointer, yields const Key& */;
using iterator = const_iterator;
using const_local_iterator = /* forward, stops at the end of its bucket */;
using local_iterator = const_local_iterator;
using node_type = /* the node handle, below */;
struct insert_return_type { iterator position; bool inserted; node_type node; };
static constexpr bool unique = true;
```

`iterator` converts to `const_iterator`, `local_iterator` to `const_local_iterator`, not back.

### Constructors

```cpp
unordered_set();
explicit unordered_set(size_type bucket_count, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
template<std::input_iterator InputIt>
unordered_set(InputIt first, InputIt last, size_type bucket_count = 0, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
unordered_set(std::initializer_list<value_type> ilist, size_type bucket_count = 0, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
unordered_set(const unordered_set& other);
unordered_set(unordered_set&& other);
```

The default constructor allocates nothing (`bucket_count() == 0`). A bucket count is rounded up to a power of two. The range constructor, given a forward range, sizes the table for the distance first; duplicates are dropped. A copy reproduces `other`'s bucket count, order and `max_load_factor`; a move takes the table over and leaves `other` empty. A constructor or hasher that throws destroys the elements built so far.

```cpp
gc::unordered_set<int> primes = {5, 3, 2, 7, 3};                      // 2 3 5 7, in some order
gc::unordered_set<int> sized(100);                                     // 128 buckets
std::vector<std::string> words = {"b", "a"};
gc::unordered_set from_range(words.begin(), words.end());              // deduced: unordered_set<std::string>
gc::unordered_set<int> taken = std::move(primes);                      // primes is empty now
```

### Destructor

```cpp
~unordered_set();
```

Destroys the elements when the set dies on a stack or inside a managed object destroyed by hand; in a sweep it leaves the nodes to the same sweep, which destroys the elements. The nodes, the bucket array and the sentinel are reclaimed by the collector in both cases.

### operator=

```cpp
unordered_set& operator=(const unordered_set& other);
unordered_set& operator=(unordered_set&& other);
unordered_set& operator=(std::initializer_list<value_type> ilist);
```

Copy assignment builds a copy of `other` and swaps it in; move assignment clears this set (destroying its elements at once) and takes the table over; the list form builds a new table with this set's hasher, equality and `max_load_factor` and swaps it in.

```cpp
gc::unordered_set<int> a = {1, 2}, b;
b = a;
b = {5, 6};                  // the old elements die here
a = std::move(b);            // a is {5, 6}, b is empty
```

### Iterators

```cpp
iterator begin() noexcept;                const_iterator begin() const noexcept;    const_iterator cbegin() const noexcept;
iterator end() noexcept;                  const_iterator end() const noexcept;      const_iterator cend() const noexcept;
```

Forward iterators over one chain of nodes; `end()` is a null iterator. The order is the chain's and changes with a rehash. An iterator is a raw node pointer: copying and advancing it costs a load, and it may be kept in unmanaged memory (a `std::vector<iterator>`) while its element is in the set.

```cpp
gc::unordered_set<std::string> s = {"a", "b"};
std::string joined;
for (const auto& key : s) {
    joined += key;                         // "ab" or "ba"
}
std::vector<decltype(s)::const_iterator> kept;   // iterators in unmanaged memory: fine
kept.push_back(s.find("a"));
```

### empty, size, max_size

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
size_type max_size() const noexcept;
```

`size()` is a stored count, O(1).

### clear

```cpp
void clear() noexcept;
```

Destroys every element at once and unlinks every node; the bucket array, the hasher, the equality and `max_load_factor` stay.

```cpp
gc::unordered_set<std::string> s = {"a", "b"};
s.clear();                     // both strings are destroyed here
bool gone = s.empty();         // true
```

### insert

```cpp
std::pair<iterator, bool> insert(const value_type& value);
std::pair<iterator, bool> insert(value_type&& value);
template<class P> requires std::is_constructible_v<value_type, P&&> std::pair<iterator, bool> insert(P&& value);
iterator insert(const_iterator hint, const value_type& value);
iterator insert(const_iterator hint, value_type&& value);
template<class P> requires std::is_constructible_v<value_type, P&&> iterator insert(const_iterator hint, P&& value);
template<std::input_iterator InputIt> void insert(InputIt first, InputIt last);
void insert(std::initializer_list<value_type> ilist);
insert_return_type insert(node_type&& nh);
iterator insert(const_iterator hint, node_type&& nh);
```

The single-element forms return the element with the key and whether it was inserted; nothing is built when the key is already there (the `P&&` forms go through `emplace`, which builds the key first and destroys it again on a duplicate). The hint is ignored. The table grows before the node is linked when the size has reached the threshold; an existing key never rehashes. The node-handle forms link the node of `nh` without copying the element: on success `nh` is empty afterwards; on a duplicate the returned `node` (or `nh`) keeps it and `position` is the element in the way. An empty handle inserts nothing.

```cpp
gc::unordered_set<std::string> s;
auto [it, inserted] = s.insert("a");                  // inserted: true
inserted = s.insert("a").second;                      // false
s.insert(s.end(), "z");                               // the hint is ignored
s.insert({"b", "c"});
gc::unordered_set<std::string> other = {"q"};
auto r = s.insert(other.extract("q"));                // relinked, no copy: r.inserted is true
```

### emplace, emplace_hint

```cpp
template<class... A> std::pair<iterator, bool> emplace(A&&... a);
template<class... A> iterator emplace_hint(const_iterator hint, A&&... a);
```

Builds the key from `a...` in a new node before it is looked up, as in `std`; if it is already there the new element is destroyed and the existing one returned. A single `value_type` argument is inserted without the detour. The hint is ignored. A hasher or equality that throws destroys the new element and leaves the set as it was.

```cpp
gc::unordered_set<std::string> s;
s.emplace(3, 'x');                          // "xxx"
auto [it, fresh] = s.emplace("xxx");        // fresh: false
s.emplace_hint(s.end(), "zzz");
```

### erase

```cpp
iterator erase(iterator pos);
iterator erase(const_iterator pos);
iterator erase(const_iterator first, const_iterator last);
size_type erase(const key_type& key);
template<class K> size_type erase(K&& key);   // when Hash and KeyEqual are transparent, and K is not an iterator
```

Destroys the element at once, unlinks the node (the collector reclaims it later) and returns the iterator after it. The key forms return 0 or 1. Erasing during an iteration: `it = s.erase(it)`. The bucket count never shrinks on an erase.

```cpp
gc::unordered_set<int> s = {1, 2, 3, 4};
s.erase(2);
for (auto it = s.begin(); it != s.end();) {
    it = *it % 2 ? s.erase(it) : std::next(it);   // 1 and 3 go
}
auto rest = s.size();                              // 1
```

### swap

```cpp
void swap(unordered_set& other) noexcept(std::is_nothrow_swappable_v<Hash> && std::is_nothrow_swappable_v<KeyEqual>);
friend void swap(unordered_set& lhs, unordered_set& rhs) noexcept(noexcept(lhs.swap(rhs)));
```

Exchanges the tables, counts, load factors, hashers and equalities; no element is touched, and every iterator keeps pointing at its element, now in the other set.

```cpp
gc::unordered_set<int> a = {1}, b = {2};
auto it = a.begin();
swap(a, b);                       // it still points at 1, which is in b now
bool moved = it == b.find(1);     // true
```

### extract

```cpp
node_type extract(const_iterator pos);
node_type extract(const key_type& key);
template<class K> node_type extract(K&& key);   // when Hash and KeyEqual are transparent, and K is not an iterator
```

Unlinks the node and hands it over in a node handle, the element untouched; the handle destroys the element if it dies unused. The key forms return an empty handle when the key is absent. This is the way to change a key. See [node_type](#node_type-the-node-handle).

```cpp
gc::unordered_set<std::string> s = {"a", "b"};
auto nh = s.extract("a");         // s holds "b"
nh.value() += "!";                // the key may change outside a set
s.insert(std::move(nh));          // "a!" and "b"; no string was copied
```

### merge

```cpp
template<class Traits2> void merge(detail::HashTable<Traits2>& source);    // any gc::unordered_set or unordered_multiset<Key, H2, E2>
template<class Traits2> void merge(detail::HashTable<Traits2>&& source);
```

Relinks the nodes of `source` whose keys are not yet here into this set, rehashing them with this set's hasher; a node whose key is already here stays in `source`. No element is copied or destroyed; iterators follow their nodes. `source` may be a `sgcl::unordered_set` or `sgcl::unordered_multiset` with the same `Key` and any hasher and equality. Merging a set into itself does nothing.

```cpp
gc::unordered_set<int> a = {1, 3};
gc::unordered_multiset<int> b = {2, 3, 3};
a.merge(b);                       // a: 1 2 3;  b keeps both 3s
```

### count, find, contains, equal_range

```cpp
size_type count(const key_type& key) const;
iterator find(const key_type& key);
const_iterator find(const key_type& key) const;
bool contains(const key_type& key) const;
std::pair<iterator, iterator> equal_range(const key_type& key);
std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const;
template<class K> size_type count(const K& key) const;                     // when Hash::is_transparent and KeyEqual::is_transparent
template<class K> iterator find(const K& key);                              //   "
template<class K> const_iterator find(const K& key) const;                  //   "
template<class K> bool contains(const K& key) const;                        //   "
template<class K> std::pair<iterator, iterator> equal_range(const K& key);  //   "  (and the const form)
```

O(1) on average, reading raw pointers only; the cached hash is compared before the key. `count` is 0 or 1. The `K` overloads exist when both `Hash` and `KeyEqual` declare `is_transparent`.

```cpp
struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
};
gc::unordered_set<std::string, StringHash, std::equal_to<>> s = {"apple"};
std::string_view key = "apple";
bool has = s.contains(key);        // no std::string is built
```

### Bucket interface

```cpp
size_type bucket_count() const noexcept;
size_type max_bucket_count() const noexcept;
size_type bucket_size(size_type n) const;
size_type bucket(const key_type& key) const;
template<class K> size_type bucket(const K& key) const;      // when Hash and KeyEqual are transparent
local_iterator begin(size_type n);              const_local_iterator begin(size_type n) const;   const_local_iterator cbegin(size_type n) const;
local_iterator end(size_type n);                const_local_iterator end(size_type n) const;     const_local_iterator cend(size_type n) const;
```

As in `std`. `bucket_count()` is 0 or a power of two; `bucket(key)` is the hash masked by `bucket_count() - 1` (0 while there are no buckets). A local iterator walks the nodes of one bucket and stops at its end; for an `n` beyond `bucket_count()` the range is empty.

```cpp
gc::unordered_set<int> s = {1, 2, 3};
size_t n = s.bucket(2);
size_t in_bucket = 0;
for (auto it = s.begin(n); it != s.end(n); ++it) {
    ++in_bucket;
}
bool same = in_bucket == s.bucket_size(n);        // true
```

### Hash policy

```cpp
float load_factor() const noexcept;
float max_load_factor() const noexcept;
void max_load_factor(float z);
void rehash(size_type count);
void reserve(size_type count);
```

`load_factor()` is `size() / bucket_count()` (0 with no buckets); `max_load_factor()` defaults to 1.0. `max_load_factor(z)` takes effect on the next insertion (a value that is not positive, or not a number, is ignored). `rehash(count)` makes the bucket count the smallest power of two not below `count` and not below `size() / max_load_factor()`; it may shrink the table, and `rehash(0)` on an empty set leaves it without buckets. `reserve(count)` is `rehash` for `count` elements. A rehash relinks the nodes, hashes nothing and invalidates no iterator.

```cpp
gc::unordered_set<int> s;
s.reserve(1000);                                  // 1024 buckets: no rehash while inserting 1000 elements
for (int i = 0; i < 1000; ++i) {
    s.insert(i);
}
bool fits = s.load_factor() <= s.max_load_factor();   // true
```

### hash_function, key_eq

```cpp
hasher hash_function() const;
key_equal key_eq() const;
```

Copies of the hasher and the equality.

### Comparisons

```cpp
friend bool operator==(const unordered_set& lhs, const unordered_set& rhs);
```

Equal sizes and every element of `lhs` found in `rhs` (found by key, then compared with `==`), whatever the bucket counts and orders. `!=` follows; there is no ordering.

```cpp
gc::unordered_set<int> a = {1, 2}, b(1000);
b.insert({2, 1});
bool same = a == b;                               // true
```

### node_type (the node handle)

```cpp
class node_type {
public:
    using key_type = Key;
    using value_type = Key;
    node_type() noexcept;
    node_type(node_type&&) noexcept;
    node_type& operator=(node_type&&);
    ~node_type();
    bool empty() const noexcept;
    explicit operator bool() const noexcept;
    value_type& value() const;             // writable: the node is out of any set
    void swap(node_type& other) noexcept;
    friend void swap(node_type& lhs, node_type& rhs) noexcept;
};
```

Owns one unlinked node: movable, not copyable; the element is destroyed when the handle dies without having been inserted. The handle holds the node through a `tracked_ptr`, so it lives on a stack or inside a managed object. It is the same handle type as `sgcl::unordered_multiset<Key>::node_type`.

### erase_if_impl, std::erase_if

```cpp
template<class Pred> size_type erase_if_impl(Pred& pred);   // member: what the free function calls

namespace sgcl {
    template<class Key, class Hash, class KeyEqual, class Pred>
    size_t erase_if(unordered_set<Key, Hash, KeyEqual>& c, Pred pred);
}
namespace std { using sgcl::erase_if; }
```

Erases every element for which `pred(*it)` is true and returns how many.

```cpp
gc::unordered_set<int> s = {1, 2, 3, 4};
auto n = std::erase_if(s, [](int x) { return x % 2 == 0; });   // 2; s holds 1 and 3
```

### Deduction guides

```cpp
template<std::input_iterator InputIt, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>   // Key from the iterator
unordered_set(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual()) -> unordered_set<Key, Hash, KeyEqual>;
template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
unordered_set(std::initializer_list<Key>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual()) -> unordered_set<Key, Hash, KeyEqual>;
```

```cpp
std::vector<std::string> src = {"a", "b"};
gc::unordered_set from_range(src.begin(), src.end());      // unordered_set<std::string>
gc::unordered_set from_list = {1, 2, 3};                   // unordered_set<int>
```

## Example

```cpp
#include "gc/gc.h"
#include <iostream>
#include <string>

struct Node {
    std::string name;
    gc::unordered_set<gc::tracked_ptr<Node>> peers;       // a set of traced pointers inside a managed object
};

int main() {
    // A graph whose edges are sets: each node is reachable from its peers
    gc::tracked_ptr a = gc::make_tracked<Node>("a");
    gc::tracked_ptr b = gc::make_tracked<Node>("b");
    gc::tracked_ptr c = gc::make_tracked<Node>("c");
    a->peers.insert(b);
    b->peers.insert(a);                         // a cycle
    b->peers.insert(c);
    bool again = b->peers.insert(c).second;     // false: a duplicate, nothing is inserted

    // A set of values on the stack, each name once
    gc::unordered_set<std::string> names;
    for (const auto& peer : b->peers) {         // pointers hash by address: any order
        names.insert(peer->name);
    }
    names.insert("b");
    std::cout << names.size() << " names, " << names.bucket_count() << " buckets\n";

    // Dropping the stack roots: a and b keep each other alive only through
    // their sets, which the collector sees as a cycle
    a = b = c = nullptr;
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    gc::collector::force_collect(true);
    std::cout << gc::collector::get_live_object_count() << " live objects\n";     // the nodes, buckets and sentinel of `names`
    return !again && names.size() == 3 ? 0 : 1;
}
```

## See also

- [unordered_multiset](unordered_multiset.md) for equal keys, [unordered_map](unordered_map.md) and [unordered_multimap](unordered_multimap.md) for key-value pairs, [set](set.md) for an ordered tree
- [tracked_ptr](tracked_ptr.md), [make_tracked](make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
