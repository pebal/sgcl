[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::operator[]

```cpp
/*(1)*/ mapped_type& operator[](const key_type& key)
            noexcept(std::is_nothrow_copy_constructible_v<Key> &&
                     std::is_nothrow_default_constructible_v<T>);
/*(2)*/ mapped_type& operator[](key_type&& key)
            noexcept(std::is_nothrow_move_constructible_v<Key> &&
                     std::is_nothrow_default_constructible_v<T>);
```

Returns a reference to the value under `key`, inserting an element first when the map does not hold the key: its
key from `key` and a value-initialized mapped value, built in place as [try_emplace](try_emplace.md) builds it.
For a `tracked_ptr` mapped type the inserted value is a null pointer, for a number 0.

1. The key is copied into the node.
2. The key is moved into the node.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |

## Return value

A reference to the mapped value of the element under `key`.

## Complexity

Logarithmic in the size of the map.

## Exceptions

What the copy (1) or the move (2) of `Key` and the default constructor of `T` throw; none when they are
noexcept.

If an exception is thrown, nothing is inserted.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Account {
    int balance;
};

int main() {
    sorted_map<string, int> counts;
    for (const char* word : {"to", "be", "or", "not", "to", "be"}) {
        ++counts[word];  // a new word is inserted as 0 first
    }
    println("{}", counts);

    sorted_map<int, tracked_ptr<Account>> accounts;
    println("{}", accounts[7] == nullptr);
    accounts[7] = make_tracked<Account>(100);
    println("{} {}", accounts[7]->balance, accounts.size());
}
```

Output:

```text
{"be": 2, "not": 1, "or": 1, "to": 2}
true
100 1
```

## See also

- [at](at.md): the value under a key, with bounds checking
- [try_emplace](try_emplace.md): inserts an element with a value of its own when the key is absent
- [insert_or_assign](insert_or_assign.md): inserts an element or assigns to its value
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
