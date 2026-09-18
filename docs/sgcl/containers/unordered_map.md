# sgcl::unordered_map

```cpp
#include "sgcl/containers/unordered_map.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class unordered_map;
}
```

The same class in the `Sgcl` interface: [Dictionary](../Sgcl/Containers/Dictionary.md).

`sgcl::unordered_map<Key, T, Hash, KeyEqual>` is `std::unordered_map` over managed nodes. The interface is the one of `std::unordered_map`: constructors, `insert`, `emplace`, `try_emplace`, `insert_or_assign`, `operator[]`, `at`, `erase`, `extract`, `merge`, node handles, the lookups with transparent hash and equality, forward iterators (`std::ranges` algorithms work), the bucket interface with local iterators, `load_factor`/`max_load_factor`/`rehash`/`reserve`, `hash_function`/`key_eq`, `swap`, `==`, deduction guides, `std::erase_if`. The behaviour is the one of `std::unordered_map` too: unique keys, an element destroyed the moment it is erased, iterators that stay valid across a rehash and until their element is erased.

What differs is where the memory lives and a few details of the layout. The map object holds two `tracked_ptr`s (the bucket array, a managed array of node pointers, and a sentinel node that precedes the first element), the counts, the hasher and the equality, so it lives where a `tracked_ptr` may live. Every element is a node on the managed heap and the nodes form one chain linked by tracked pointers, as in libstdc++: a bucket points at the node before its first node, so the whole chain is traced from the sentinel, an iterator is one raw node pointer, and an iteration is a load per step. A `unordered_map<Key, tracked_ptr<T>>`, or one inside a managed object, is traced like any other managed data, and a cycle through it is collected like any other cycle. Nothing is freed by hand: an `erase` destroys the element and unlinks the node, the collector reclaims the node later. The hash of each key is cached in its node, so a rehash hashes nothing and a lookup compares hashes before keys. The bucket count is always a power of two (a lookup masks the hash; `rehash` and the constructors round up), the table starts with no bucket array at all, and it grows when the size reaches `bucket_count() * max_load_factor()`, doubling at least, to eight buckets at the least. A lookup, an iteration and an iterator copy read raw pointers only and pay no write barrier; an insertion, an erasure and a rehash store tracked pointers and pay the barrier on each link they relink ([README: Containers](README.md#containers)).

## Rules

- An `unordered_map` holds tracked pointers, so it lives on a stack or inside a managed object: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame ([The rules](../core/README.md#the-rules), 1). The same holds for a node handle (`node_type`), which holds its node through a `tracked_ptr`.
- The elements may hold tracked pointers (`sgcl::unordered_map<int, sgcl::tracked_ptr<T>>`, or a `tracked_ptr` key: `std::hash<sgcl::tracked_ptr<T>>` hashes the address): the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the map is destroyed, exactly as in `std`. The one exception is a map dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector thread ([README: Threads](../async/README.md#threads)).
- An iterator, a reference or a pointer to an element is valid while the element is in the map, across insertions, rehashes, erasures of other elements, `swap`, `merge` and a move of the map (it follows the node). An iterator to an erased element is invalid as in `std`; it keeps the node's memory mapped but not the element. Iterators are trivially copyable and may live anywhere, a `std::vector<iterator>` included: the map roots every node it links.
- A `tracked_ptr` may point at an element of the map, or a member of one (a node is a managed object, [The rules](../core/README.md#the-rules), 4); it keeps the node alive, not the element.
- Thread safety is that of `std::unordered_map`: concurrent readers, or one writer, with the program's own synchronization ([The rules](../core/README.md#the-rules), 6).

## Members

### Types

```cpp
using key_type = Key;
using mapped_type = T;
using value_type = std::pair<const Key, T>;
using hasher = Hash;
using key_equal = KeyEqual;
using size_type = size_t;
using difference_type = ptrdiff_t;
using reference = value_type&;
using const_reference = const value_type&;
using pointer = value_type*;
using const_pointer = const value_type*;
using iterator = /* forward, one raw node pointer */;
using const_iterator = /* forward, one raw node pointer */;
using local_iterator = /* forward, stops at the end of its bucket */;
using const_local_iterator = /* forward, stops at the end of its bucket */;
using node_type = /* the node handle, below */;
struct insert_return_type { iterator position; bool inserted; node_type node; };
static constexpr bool unique = true;
```

`iterator` converts to `const_iterator`, `local_iterator` to `const_local_iterator`, not back. An `iterator` is one word; a local iterator adds the bucket index and the mask.

### Constructors

```cpp
unordered_map();
explicit unordered_map(size_type bucket_count, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
template<std::input_iterator InputIt>
unordered_map(InputIt first, InputIt last, size_type bucket_count = 0, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
unordered_map(std::initializer_list<value_type> ilist, size_type bucket_count = 0, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
unordered_map(const unordered_map& other);
unordered_map(unordered_map&& other);
```

The default constructor allocates nothing (`bucket_count() == 0`). A bucket count is rounded up to a power of two; 0 means no array yet. The range constructor, given a forward range, sizes the table for the distance first; a key seen twice keeps its first value. An element of the range that is a `std::pair<Key, U>` is hashed and looked up where it is and copied once, into its node (a duplicate copies nothing); an element of any other type is converted to a `value_type` first, once. A copy reproduces `other`'s bucket count, order and `max_load_factor`; a move takes the table over and leaves `other` empty. An element constructor or hasher that throws while a constructor runs destroys the elements built so far.

```cpp
sgcl::unordered_map<sgcl::string, int> ages = {{"ann", 31}, {"bob", 27}};
sgcl::unordered_map<int, int> sized(100);                                         // 128 buckets, no elements
sgcl::vector<sgcl::pair<int, int>> src = {{2, 20}, {1, 10}};
sgcl::unordered_map from_range(src.begin(), src.end());                           // deduced: unordered_map<int, int>
sgcl::unordered_map<sgcl::string, int> taken = std::move(ages);                    // ages is empty now
```

### Destructor

```cpp
~unordered_map();
```

Destroys the elements when the map dies on a stack or inside a managed object destroyed by hand. In a sweep (the map inside a dying managed object) it does nothing: the nodes are garbage of the same sweep and destroy their elements when the sweep reaches them. The nodes, the bucket array and the sentinel are reclaimed by the collector in both cases.

### operator=

```cpp
unordered_map& operator=(const unordered_map& other);
unordered_map& operator=(unordered_map&& other);
unordered_map& operator=(std::initializer_list<value_type> ilist);
```

Copy assignment builds a copy of `other` and swaps it in (the old elements die when the temporary does); self-assignment is a no-op. Move assignment clears this map, destroying its elements at once, and takes the table over. The list form builds a new table with this map's hasher, equality and `max_load_factor` and swaps it in.

```cpp
sgcl::unordered_map<int, int> a = {{1, 1}}, b;
b = a;                       // copies
b = {{5, 5}, {6, 6}};        // the old elements die here
a = std::move(b);            // a holds 5 and 6, b is empty
```

### Iterators

```cpp
iterator begin() noexcept;                const_iterator begin() const noexcept;    const_iterator cbegin() const noexcept;
iterator end() noexcept;                  const_iterator end() const noexcept;      const_iterator cend() const noexcept;
```

Forward iterators over one chain of nodes; `end()` is a null iterator. The order is the chain's, bucket by bucket, and changes with a rehash. An iterator is a raw node pointer: copying and advancing it costs a load, never a write barrier, and it may be kept in unmanaged memory for as long as its element is in the map.

```cpp
sgcl::unordered_map<sgcl::string, int> m = {{"a", 1}, {"b", 2}};
int sum = 0;
for (auto& [key, value] : m) {
    sum += value;                          // 3, in whichever order
}
std::vector<decltype(m)::iterator> kept;   // iterators in unmanaged memory: fine
kept.push_back(m.find("a"));
m.rehash(64);                              // kept[0] still points at "a"
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

Destroys every element at once and unlinks every node; the bucket array, the hasher, the equality and `max_load_factor` stay. The nodes are reclaimed by the collector.

```cpp
sgcl::unordered_map<int, sgcl::string> m = {{1, "a"}, {2, "b"}};
auto buckets = m.bucket_count();
m.clear();                                  // both strings are destroyed here
bool same = m.bucket_count() == buckets;    // true
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

As in `std::unordered_map`: the single-element forms return the element with the key and whether it was inserted; nothing is built when the key is already there (the `P&&` forms go through `emplace`, which builds the element first and destroys it again on a duplicate). The range form reads the key of each element where it is: a `std::pair<Key, U>` is hashed and looked up in the range and copied once, into its node, a duplicate copies nothing; an element of any other type is converted to a `value_type` first, once. The hint is ignored. The table grows before the node is linked when the size has reached the threshold; an existing key never rehashes. The node-handle forms link the node of `nh` without copying the element: on success `nh` is empty afterwards; on a duplicate key the returned `node` (or `nh`, for the hinted form) keeps it and `position` points at the element in the way. An empty handle inserts nothing (`position == end()`, `inserted == false`).

```cpp
sgcl::unordered_map<sgcl::string, int> m;
auto [it, inserted] = m.insert({"a", 1});                          // inserted: true
inserted = m.insert(std::pair<const char*, int>("a", 2)).second;   // false: "a" stays 1
m.insert({{"b", 2}, {"c", 3}});
sgcl::unordered_map<sgcl::string, int> other = {{"q", 17}};
auto r = m.insert(other.extract("q"));                             // relinked, no copy: r.inserted is true
```

### emplace, emplace_hint

```cpp
template<class... A> std::pair<iterator, bool> emplace(A&&... a);
template<class... A> iterator emplace_hint(const_iterator hint, A&&... a);
```

Builds the `value_type` from `a...` in a new node before the key is looked up, as in `std`; if the key is already there the new element is destroyed and the existing one returned (the node is garbage for the collector). A single `value_type` argument is inserted without the detour. The hint is ignored. A hasher or equality that throws destroys the new element and leaves the map as it was.

```cpp
sgcl::unordered_map<sgcl::string, sgcl::string> m;
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

Looks the key up first and builds the mapped value from `a...` in place only when the key is new: the mapped type need not be movable or copyable, and `a...` are not touched on a duplicate. The hint is ignored.

```cpp
struct Pinned {
    int value;
    explicit Pinned(int v) : value(v) {}
    Pinned(const Pinned&) = delete;
    Pinned& operator=(const Pinned&) = delete;
};
sgcl::unordered_map<int, Pinned> m;
m.try_emplace(1, 10);                                       // Pinned(10) built inside the node
auto [it, fresh] = m.try_emplace(1, 11);                    // fresh: false; no Pinned(11) was built
m.try_emplace(m.end(), 2, 20);                              // the hinted form returns an iterator
```

### insert_or_assign

```cpp
template<class M> std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj);
template<class M> std::pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj);
template<class M> iterator insert_or_assign(const_iterator hint, const key_type& key, M&& obj);
template<class M> iterator insert_or_assign(const_iterator hint, key_type&& key, M&& obj);
```

Inserts `{key, obj}` when the key is new (`true`), otherwise assigns `obj` to the mapped value in place (`false`). The hint is ignored.

```cpp
sgcl::unordered_map<sgcl::string, int> m;
m.insert_or_assign("a", 1);                          // inserted
auto [it, inserted] = m.insert_or_assign("a", 2);    // assigned: inserted is false, it->second is 2
```

### at, operator[]

```cpp
mapped_type& at(const key_type& key);
const mapped_type& at(const key_type& key) const;
template<class K> mapped_type& at(const K& key);               // when Hash and KeyEqual are transparent
template<class K> const mapped_type& at(const K& key) const;   //   "
mapped_type& operator[](const key_type& key);
mapped_type& operator[](key_type&& key);
```

`at` throws `std::out_of_range` when the key is absent, and takes a `K` for transparent hash and equality. `operator[]` inserts a value-initialized mapped value for a new key (built in place, through `try_emplace`) and returns a reference to it; for a `tracked_ptr` mapped type that is a null pointer.

```cpp
sgcl::unordered_map<sgcl::string, int> counts;
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
template<class K> size_type erase(K&& key);   // when Hash and KeyEqual are transparent, and K is not an iterator
```

Destroys the element at once, unlinks the node (the collector reclaims it later) and returns the iterator after it. The key forms return 0 or 1. Erasing during an iteration is done as with `std`: `it = m.erase(it)`. The bucket count never shrinks on an erase.

### take

```cpp
optional<mapped_type> take(const key_type& key);
template<class K> optional<mapped_type> take(const K& key);   // when Hash and KeyEqual are transparent
```

The mapped value under the key, moved out, and the element erased; `nullopt` when the key is absent. What Java's `remove(key)` and C#'s `Remove(key, out value)` hand back (`extract` gives the node, `erase` a count).

```cpp
sgcl::unordered_map<sgcl::string, int> m = {{"a", 1}};
sgcl::optional<int> taken = m.take("a");           // 1, and m is empty
sgcl::optional<int> none = m.take("a");            // nullopt
```

```cpp
sgcl::unordered_map<int, sgcl::string> m = {{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}};
m.erase(2);                                        // "b" is destroyed here
for (auto it = m.begin(); it != m.end();) {
    it = it->first % 2 ? m.erase(it) : std::next(it);   // 1 and 3 go
}
auto rest = m.size();                              // 1: {4, "d"}
```

### swap

```cpp
void swap(unordered_map& other) noexcept(std::is_nothrow_swappable_v<Hash> && std::is_nothrow_swappable_v<KeyEqual>);
friend void swap(unordered_map& lhs, unordered_map& rhs) noexcept(noexcept(lhs.swap(rhs)));
```

Exchanges the tables, counts, load factors, hashers and equalities; no element is touched, and every iterator keeps pointing at its element, now in the other map.

```cpp
sgcl::unordered_map<int, int> a = {{1, 1}}, b = {{2, 2}};
auto it = a.begin();
swap(a, b);                       // it still points at {1, 1}, which is in b now
bool moved = it == b.find(1);     // true
```

### extract

```cpp
node_type extract(const_iterator pos);
node_type extract(const key_type& key);
template<class K> node_type extract(K&& key);   // when Hash and KeyEqual are transparent, and K is not an iterator
```

Unlinks the node and hands it over in a node handle, the element untouched: the handle is the owner now, and destroys the element if it dies unused. The key forms return an empty handle when the key is absent. See [node_type](#node_type-the-node-handle).

```cpp
sgcl::unordered_map<int, sgcl::string> m = {{1, "a"}, {2, "b"}};
auto nh = m.extract(1);           // m holds {2, "b"}; nh holds {1, "a"}
nh.key() = 7;                     // the key may change outside a map
m.insert(std::move(nh));          // {2, "b"}, {7, "a"}; no string was copied
```

### merge

```cpp
template<class Traits2> void merge(detail::HashTable<Traits2>& source);    // any sgcl::unordered_map or unordered_multimap<Key, T, H2, E2>
template<class Traits2> void merge(detail::HashTable<Traits2>&& source);
```

Relinks the nodes of `source` whose keys are not yet here into this map, rehashing them with this map's hasher; a node whose key is already here stays in `source`. No element is copied or destroyed, and every iterator follows its node. `source` may be a `sgcl::unordered_map` or `sgcl::unordered_multimap` with the same `Key` and `T` and any hasher and equality; not an [ordered_map](ordered_map.md), whose nodes are of another shape (the call does not compile). Merging a map into itself does nothing.

```cpp
sgcl::unordered_map<int, int> a = {{1, 1}, {3, 3}};
sgcl::unordered_multimap<int, int> b = {{2, 2}, {3, 30}, {3, 31}};
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

O(1) on average, reading raw pointers only; the cached hash is compared before the key. `count` is 0 or 1; `equal_range` is an empty or a one-element range. The `K` overloads exist when both `Hash` and `KeyEqual` declare `is_transparent`, and let a `std::string_view` look up a `sgcl::string` key without building one.

```cpp
sgcl::unordered_map<sgcl::string, int> m = {{"apple", 1}};   // std::hash and std::equal_to of a string are transparent
bool has = m.contains("apple");    // no sgcl::string is built for the literal
sgcl::string line = "apple pie";
sgcl::string_view key = line.view(0, 5);   // a piece of another string, a view holding its object
auto it = m.find(key);             // it->second == 1, nothing built either
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

As in `std`. `bucket_count()` is 0 or a power of two; `bucket(key)` is the hash masked by `bucket_count() - 1` (0 while there are no buckets); `bucket_size(n)` walks the bucket. A local iterator walks the nodes of one bucket and stops at its end; for an `n` beyond `bucket_count()` the range is empty.

```cpp
sgcl::unordered_map<int, int> m = {{1, 1}, {2, 2}, {3, 3}};
size_t n = m.bucket(2);
size_t in_bucket = 0;
for (auto it = m.begin(n); it != m.end(n); ++it) {
    ++in_bucket;                                  // every element here hashes to bucket n
}
bool same = in_bucket == m.bucket_size(n);        // true
```

### Hash policy

```cpp
float load_factor() const noexcept;
float max_load_factor() const noexcept;
void max_load_factor(float z);
void rehash(size_type count);
void reserve(size_type count);
```

`load_factor()` is `size() / bucket_count()` (0 with no buckets); `max_load_factor()` defaults to 1.0. `max_load_factor(z)` sets it and takes effect on the next insertion (a value that is not positive, or not a number, is ignored). `rehash(count)` makes the bucket count the smallest power of two not below `count` and not below `size() / max_load_factor()`: it may shrink the table; `rehash(0)` on an empty table leaves it without buckets. `reserve(count)` is `rehash` for `count` elements. A rehash relinks the nodes in chain order, hashing nothing (the hash is cached) and invalidating no iterator.

```cpp
sgcl::unordered_map<int, int> m;
m.reserve(1000);                                  // 1024 buckets: no rehash while inserting 1000 elements
for (int i : sgcl::range(1000)) {
    m.emplace(i, i);
}
bool fits = m.load_factor() <= m.max_load_factor();   // true
m.max_load_factor(0.5f);                          // the table grows on the next insertion if overloaded
m.rehash(8);                                      // 2048 buckets: never below what the elements need
```

### hash_function, key_eq

```cpp
hasher hash_function() const;
key_equal key_eq() const;
```

Copies of the hasher and the equality.

```cpp
sgcl::unordered_map<int, int> m;
bool eq = m.key_eq()(1, 1);                       // true
size_t h = m.hash_function()(1);
```

### Comparisons

```cpp
friend bool operator==(const unordered_map& lhs, const unordered_map& rhs);
```

Equal sizes and, for every element of `lhs`, an element of `rhs` with an equal key and an equal value (`value_type == value_type`), whatever the bucket counts and orders. `!=` follows; there is no ordering.

```cpp
sgcl::unordered_map<int, int> a = {{1, 1}, {2, 2}};
sgcl::unordered_map<int, int> b(1000);
b.insert({{2, 2}, {1, 1}});
bool same = a == b;                               // true
```

### node_type (the node handle)

```cpp
class node_type {
public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    node_type() noexcept;
    node_type(node_type&&) noexcept;
    node_type& operator=(node_type&&);
    ~node_type();
    bool empty() const noexcept;
    explicit operator bool() const noexcept;
    key_type& key() const;                 // writable: the node is out of any map
    mapped_type& mapped() const;
    void swap(node_type& other) noexcept;
    friend void swap(node_type& lhs, node_type& rhs) noexcept;
};
```

Owns one unlinked node, as `std::unordered_map::node_type` does: movable, not copyable; the element is destroyed when the handle dies without having been inserted (the node's memory is the collector's). The handle holds the node through a `tracked_ptr`, so it lives on a stack or inside a managed object, like the map. It is the same handle type as `sgcl::unordered_multimap<Key, T>::node_type`.

### erase_if_impl, std::erase_if

```cpp
template<class Pred> size_type erase_if_impl(Pred& pred);   // member: what the free function calls

namespace sgcl {
    template<class Key, class T, class Hash, class KeyEqual, class Pred>
    size_t erase_if(unordered_map<Key, T, Hash, KeyEqual>& c, Pred pred);
}
namespace std { using sgcl::erase_if; }
```

Erases every element for which `pred(*it)` is true and returns how many; each erased element is destroyed at once.

```cpp
sgcl::unordered_map<int, int> m = {{1, 1}, {2, 2}, {3, 3}, {4, 4}};
auto n = std::erase_if(m, [](const auto& p) { return p.first % 2 == 0; });   // 2; m holds 1 and 3
```

### Deduction guides

```cpp
template<std::input_iterator InputIt, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>   // Key, T from the iterator's pair
unordered_map(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual()) -> unordered_map<Key, T, Hash, KeyEqual>;
template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
unordered_map(std::initializer_list<std::pair<Key, T>>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual()) -> unordered_map<Key, T, Hash, KeyEqual>;
```

From an iterator pair over pairs, or from an initializer list of `std::pair`s (the pairs must be spelled out: nested braces deduce nothing, as with `std`).

```cpp
sgcl::vector<sgcl::pair<sgcl::string, int>> src = {{"a", 1}};
sgcl::unordered_map from_range(src.begin(), src.end());      // unordered_map<sgcl::string, int>
sgcl::unordered_map from_list = {std::pair{1, 2.5}};         // unordered_map<int, double>
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

struct Node {
    int id;
    sgcl::tracked_ptr<Node> next;     // a pointer inside an element: traced through the node
};

// The map object lives inside a managed object: its nodes hang off it and
// die with it, elements included, when the Registry is collected.
struct Registry {
    sgcl::unordered_map<int, sgcl::tracked_ptr<Node>> by_id;
};

int main() {
    sgcl::tracked_ptr registry = sgcl::make_tracked<Registry>();
    for (int id : sgcl::range(100)) {
        // operator[] inserts a null tracked_ptr; the object is made afterwards
        registry->by_id[id] = sgcl::make_tracked<Node>(id);
    }
    registry->by_id[0]->next = registry->by_id[1];
    registry->by_id[1]->next = registry->by_id[0];             // a cycle: collected like any other

    // A map on the stack, keyed by pointer: std::hash<tracked_ptr> hashes the address
    sgcl::unordered_map<sgcl::tracked_ptr<Node>, sgcl::string> names;
    names.try_emplace(registry->by_id[0], "zero");
    names.emplace(registry->by_id[1], "one");
    auto one = names.find(registry->by_id[1]);
    names.rehash(256);                                         // `one` is still valid
    std::cout << one->second << " is node " << one->first->id << '\n';

    // Erasing from the registry destroys the tracked_ptr elements at once;
    // nodes 0 and 1 stay reachable through `names`, the rest is garbage
    std::erase_if(registry->by_id, [](const auto& p) { return p.first >= 2; });
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    sgcl::collector::force_collect(true);
    std::cout << registry->by_id.size() << " in the registry, "
              << sgcl::collector::get_live_object_count() << " live objects\n";
    return registry->by_id.size() == 2 && names.size() == 2 ? 0 : 1;
}
```

The output:

```
one is node 1
2 in the registry, 13 live objects
```

## See also

- [unordered_multimap](unordered_multimap.md) for equal keys, [unordered_set](unordered_set.md) and [unordered_multiset](unordered_multiset.md) for keys alone, [map](map.md) for an ordered tree
- [tracked_ptr](../core/tracked_ptr.md), [unique_ptr](../core/unique_ptr.md), [make_tracked](../core/make_tracked.md)
- [README: Containers](README.md#containers), [README: The rules](../core/README.md#the-rules), [README: Stack roots](../../garbage_collector/overview.md#stack-roots)
- `examples/example.cpp`
