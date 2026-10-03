[sgcl](../../README.md) › [core](../README.md) › [expected](README.md)

# sgcl::expected\<T, E\>::transform_error

```cpp
template<class F> auto transform_error(F&& f) & noexcept(/* see below */);          // (1)
template<class F> auto transform_error(F&& f) const& noexcept(/* see below */);     // (2)
template<class F> auto transform_error(F&& f) && noexcept(/* see below */);         // (3)
template<class F> auto transform_error(F&& f) const&& noexcept(/* see below */);    // (4)
```

The error mapped: when there is an error, an `expected<T, G>` holding what `f` returns when called with it (through
`std::invoke`); when there is a value, an `expected<T, G>` holding the value. `G` is the result of `f` without
`const`.

- (1–2) The value and the error are passed and copied as lvalues.
- (3–4) They are passed and moved as rvalues.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the function called with the error |

## Return value

`expected<T, G>` with the value, or with the result of `f`.

## Complexity

Constant, plus the call of `f` or the copy or the move of the value.

## Exceptions

What `f` throws, and what the construction of the result from the value or from `f`'s result throws. The function
is noexcept when the call of `f` and those constructions are.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    expected<int, int> status = unexpected(404);
    expected<int, string> described = status.transform_error([](int code) {
        return code == 404 ? string("not found") : string("failed");
    });
    println("{}", described.error());
}
```

Output:

```text
not found
```

## See also

- [transform](transform.md): the value mapped
- [or_else](or_else.md): the recovery from an error
- [sgcl::expected\<T, E\>](README.md)
