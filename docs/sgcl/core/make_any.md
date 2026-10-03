[sgcl](../README.md) › [core](README.md)

# sgcl::make_any\<T\>

```cpp
#include "sgcl/core/any.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, class... A>
    any make_any(A&&... a) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, A...>);    // (1)
    template<class T, class U, class... A>
    any make_any(std::initializer_list<U> il, A&&... a) noexcept(                               // (2)
        std::is_nothrow_constructible_v<std::decay_t<T>, std::initializer_list<U>&, A...>);
}
```

An [any](any.md) holding a `std::decay_t<T>` constructed in place, placed as the [constructor](any/any.md) places
it: a pointer word in the word, a small value without pointers in the buffer, anything else in a managed node of its
own.

1. `any(std::in_place_type<T>, std::forward<A>(a)...)`.
2. `any(std::in_place_type<T>, il, std::forward<A>(a)...)`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the value is constructed from |
| `il` | the initializer list the value is constructed from |

## Return value

The new `any`.

## Complexity

Constant: one managed allocation for a value in a node, none for the others.

## Exceptions

What the constructor of `std::decay_t<T>` throws; none when it is noexcept.

## Notes

With `using namespace sgcl` and a `std` type among the arguments, the call needs its namespace,
`sgcl::make_any<T>(...)`: argument-dependent lookup finds `std::make_any` as well, and the two are ambiguous.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string>

using namespace sgcl;

int main() {
    any numbers = make_any<vector<int>>({1, 2, 3});
    any text = make_any<string>(3, 'x');
    std::string name = "std";
    any copied = sgcl::make_any<std::string>(name);  // a std argument: the namespace written

    println("{} {} {}", any_cast<vector<int>&>(numbers), any_cast<string&>(text),
            any_cast<std::string&>(copied));
}
```

Output:

```text
[1, 2, 3] xxx std
```

## See also

- [any](any.md): the class
- [emplace](any/emplace.md): a value constructed in place in an existing `any`
