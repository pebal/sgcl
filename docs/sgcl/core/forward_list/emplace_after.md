[sgcl](../../README.md) › [core](../README.md) › [forward_list](README.md)

# sgcl::forward_list\<T\>::emplace_after

```cpp
template<class... A>
iterator emplace_after(const_iterator pos, A&&... a)
    noexcept(std::is_nothrow_constructible_v<T, A...>);
```

Constructs an element after `pos` from the arguments `a`, forwarded to the constructor of `T`, in a node of its own;
[before_begin()](before_begin.md) puts it at the front. The node is made and the element constructed in one step,
before anything is linked: an argument may refer to an element of this list.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the element after which the new one goes, or `before_begin()` |
| `a` | the arguments of the constructor of `T` |

## Return value

An iterator to the new element.

## Complexity

Constant.

## Exceptions

What the constructor of `T` throws; none when it is noexcept. If an exception is thrown, the list is as it was
before the call.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    forward_list<string> words = {"b"};
    auto it = words.emplace_after(words.before_begin(), 3, 'a');  // string(3, 'a')
    words.emplace_after(it, "between");
    println("{}", words);
}
```

Output:

```text
["aaa", "between", "b"]
```

## See also

- [insert_after](insert_after.md): inserts elements after a position
- [emplace_front](emplace_front.md): constructs an element in place at the beginning
- [sgcl::forward_list\<T\>](README.md)
