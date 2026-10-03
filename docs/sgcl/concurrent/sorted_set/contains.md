[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](README.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::contains

```cpp
bool contains(const Key& key) const noexcept;                    // (1)
template<class K> bool contains(const K& key) const noexcept;    // (2)
```

Checks whether the set holds a key equivalent to `key`, with the search of [find](find.md).

- (2) Takes part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look for |

## Return value

`true` when the set holds a key equivalent to `key`, `false` otherwise.

## Complexity

Logarithmic in the size of the set, expected.

## Exceptions

None.

## Notes

Wait-free, and the search writes nothing. The answer is of the moment the search reached the bottom list: another
thread may insert or erase the key right after. A thread that inserts a key unless it is there asks
[insert](insert.md), whose answer is that of the insertion itself.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    concurrent::sorted_set<string> users = {"ada", "grace"};
    println("{} {}", users.contains("ada"), users.contains("linus"));

    std::string_view name = "grace";
    println("{}", users.contains(name));  // no string made for the search
}
```

Output:

```text
true false
true
```

## See also

- [find](find.md): finds the key
- [count](count.md): the number of keys equivalent to a key
- [sgcl::concurrent::sorted_set\<Key, Compare\>](README.md)
