# Sgcl::OrderedDictionary

```cpp
#include "sgcl/Sgcl/Containers/OrderedDictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    class OrderedDictionary;
}
```

The same class in the `sgcl` interface: [ordered_map](../../containers/ordered_map.md).

`OrderedDictionary<Key, Value, Hash, Equal>` is a [Dictionary](Dictionary.md) iterated in the order its entries were added: Java's `LinkedHashMap`, .NET's `OrderedDictionary`, the `dict` of Python. The same table, every entry on one more list in the order of adding: a range-for walks that list, oldest entry first, `First()` is the oldest entry and `Last()` the newest, a copy comes out in the same order, `Remove` takes an entry out of the order, `Set` or `Add` of a key that is present leaves it where it was, `MoveToLast` and `MoveToFirst` move an entry to the end or the start of the order, `RemoveFirst` and `RemoveLast` drop the oldest or the newest. Everything else is `Dictionary`: `Add`, `Set`, `operator[]`, `Find` (a pointer to the value, null when absent, no `String` made for a `string_view` or a literal), `ContainsKey`, `Remove`, `RemoveAll`, the hash policy. A rehash never touches the order; the price is two words more per entry.

What it is for: a dictionary whose order is part of the data or of the behaviour: an object read from JSON and written back with its keys as they came, a configuration, a registry that reports in the order of registration, and a cache with an eviction order (`MoveToLast` on a hit, `RemoveFirst` when full: the example below). `Dictionary` is the right one when the order is nothing; [SortedDictionary](SortedDictionary.md) when it is the order of the keys.

## Rules

Those of [Dictionary](Dictionary.md#rules), and for the order: an iterator stays valid across `MoveToLast` and `MoveToFirst` of any entry; `end(d)` is the sentinel, so the newest entry is `std::prev(end(d))`.

## Members

Every member of [Dictionary](Dictionary.md#members), with these differences and additions.

### Types

```cpp
using KeyType = Key;  using ValueType = Value;  using PairType = std::pair<const Key, Value>;
using InnerType = sgcl::ordered_map<Key, Value, Hash, Equal>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // bidirectional, one raw node pointer
using ConstIterator = InnerType::const_iterator;
```

### begin, end

The order of adding, oldest first, for a range-for (`begin(d)`, `end(d)`: the free functions of the interface); both ways (`std::prev(end(d))` is the newest), unchanged by a rehash.

```cpp
OrderedDictionary<String, int> d;
d["c"] = 1;
d["a"] = 2;
d["b"] = 3;
d.Set("a", 4);                                // present: the value changes, the place does not
String keys;
for (const auto& [key, value] : d) {
    keys = keys + key;                        // "cab", always
}
```

### First, Last

```cpp
PairType& First() noexcept;   const PairType& First() const noexcept;   // the oldest entry
PairType& Last() noexcept;    const PairType& Last() const noexcept;    // the newest
```

The dictionary must not be empty.

### RemoveFirst, RemoveLast

```cpp
bool RemoveFirst();   // the oldest entry removed: whether there was one
bool RemoveLast();    // the newest
```

### MoveToLast, MoveToFirst

```cpp
void MoveToLast(ConstIterator pos) noexcept;    // the entry becomes the newest
void MoveToFirst(ConstIterator pos) noexcept;   // the oldest
template<class K = Key> bool MoveToLast(const K& key) noexcept;    // by the key: whether it was there
template<class K = Key> bool MoveToFirst(const K& key) noexcept;
```

Moves the entry to the end (the start) of the order, as if added last (first) from now on; nothing else changes, the iterator stays valid, O(1). By an iterator (from `FindEntry`), or by the key, with a `K` other than the key type looking up without building a key when the hash and the equality are transparent (a `string_view` for a `String`).

```cpp
OrderedDictionary<String, int> d = {{"a", 1}, {"b", 2}, {"c", 3}};
d.MoveToLast("a");                            // b c a
d.MoveToFirst(d.FindEntry("c"));              // c b a
assert(d.First().first == "c" && d.Last().first == "a");
```

### Add, Set, Emplace, operator[]

As in [Dictionary](Dictionary.md#add-emplace): a new key goes to the end of the order; a present key stays where it is, whatever happens to its value.

### Remove, Take, RemoveAt, RemoveRange, RemoveAll

As in [Dictionary](Dictionary.md#remove-removeat-removerange): the entry leaves the order too (`Take` hands its value back); `RemoveAt` returns the iterator after it in the order; `RemoveRange` is a range of the order.

### Copy, Swap, comparison

A copy reproduces the order; `Swap` and a move carry it along; `==` compares the contents and ignores the order.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A cache of `capacity` entries that drops the least recently used one:
// an OrderedDictionary, the oldest first, touched entries moved to the end
struct Cache {
    size_t capacity;
    OrderedDictionary<String, String> entries;

    const String* Get(std::string_view key) {
        auto it = entries.FindEntry(key);         // a string_view: no String made for the lookup
        if (it == end(entries)) {
            return nullptr;
        }
        entries.MoveToLast(it);                   // the newest now
        return &it->second;
    }

    void Put(String key, String value) {
        if (!entries.Add(key, value)) {
            entries.Set(key, value);
            entries.MoveToLast(key);
        } else if (entries.Count() > capacity) {
            entries.RemoveFirst();                // the oldest goes
        }
    }
};

int main() {
    Cache cache{2};
    cache.Put("a", "1");
    cache.Put("b", "2");
    cache.Get("a");                               // a is newer than b now
    cache.Put("c", "3");                          // full: b, the oldest, is dropped
    for (const auto& [key, value] : cache.entries) {
        std::cout << key << "=" << value << " ";
    }
    std::cout << "\n" << (cache.Get("b") ? "b kept" : "b evicted") << "\n";
    return 0;
}
```

The output:

```
a=1 c=3 
b evicted
```

## See also

- [OrderedSet](OrderedSet.md) for values alone in the order of adding, [Dictionary](Dictionary.md) for the same dictionary without the order, [SortedDictionary](SortedDictionary.md) for the order of the keys
- [String](../Core/String.md): a key looked up by a `string_view` or a literal
- [README: Containers](README.md#containers), [README: The rules](../Core/README.md#the-rules)
