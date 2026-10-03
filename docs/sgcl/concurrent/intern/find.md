[sgcl](../../README.md) › [concurrent](../README.md) › [intern](../intern.md)

# sgcl::concurrent::intern\<T, Hash, KeyEqual\>::find

```cpp
/*(1)*/ handle find(const T& value) const noexcept;
/*(2)*/ template<class K> handle find(const K& value) const noexcept;
```

Returns the canonical object of `value` when one is alive, and never makes one.

1. Looks up `value`.
2. Looks up a value of another type, with no `T` built for the search: a `string_view` or a literal for a pool of
   strings. Takes part only when `Hash` and `KeyEqual` both have `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to look up |

## Return value

The canonical object, or a null handle when there is no live one: a null `tracked_ptr<const T>`, or the empty
string for a pool of strings.

## Complexity

Constant on average: a search of the hash set.

## Exceptions

None.

## Notes

Wait-free, and it writes nothing once the value's bucket has its dummy node, which the first lookup in a bucket
makes (lock-free, as in [set](../set/find.md)): a reader that asks whether a value is known costs the other threads
nothing. An entry whose object has died is not found, even before a sweep drops it.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::intern<int> numbers;
    tracked_ptr<const int> seven = numbers.of(7);
    println("{} {}", numbers.find(7) == seven, numbers.find(8) == nullptr);

    concurrent::intern<string> names;
    string ada = names.of("Ada");
    println("{} {}", names.find("Ada").object() == ada.object(), names.find("Grace").empty());
    println("{}", names.size());
}
```

Output:

```text
true true
true true
1
```

## See also

- [of](of.md): the canonical object, made when there is none
- [sgcl::concurrent::intern\<T, Hash, KeyEqual\>](../intern.md)
