[sgcl](../../README.md) › [core](../README.md) › [expected](README.md)

# sgcl::expected\<T, E\>::or_else

```cpp
template<class F> auto or_else(F&& f) & noexcept(/* see below */);          // (1)
template<class F> auto or_else(F&& f) const& noexcept(/* see below */);     // (2)
template<class F> auto or_else(F&& f) && noexcept(/* see below */);         // (3)
template<class F> auto or_else(F&& f) const&& noexcept(/* see below */);    // (4)
```

The recovery from an error: when there is an error, the result of `f` called with it (through `std::invoke`), which
must be an `expected` with the same value type `T`; when there is a value, that `expected` type holding the value.
`f` returns some `expected<T, G>`, and so does `or_else`.

- (1–2) The value and the error are passed and copied as lvalues.
- (3–4) They are passed and moved as rvalues.

For `expected<void, E>`, `f` returns an `expected<void, G>`.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the function called with the error; it returns an `expected` with the value type `T` |

## Return value

What `f` returns, or an `expected` of that type with the value.

## Complexity

Constant, plus the call of `f` or the copy or the move of the value.

## Exceptions

What `f` throws, and what the copy or the move of the value throws. The function is noexcept when the call of `f`
and the construction of the result from the value are.

## Notes

An `f` whose result is not an `expected` with the value type `T` is ill-formed, with a message that says so.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

expected<int, string> from_config() {
    return unexpected("no port in the config");
}

int main() {
    auto port = from_config().or_else([](const string& why) -> expected<int, string> {
        println("{}: the default", why);
        return 8080;
    });
    println("{}", *port);
}
```

Output:

```text
no port in the config: the default
8080
```

## See also

- [transform_error](transform_error.md): the error mapped
- [and_then](and_then.md): the next step after a value
- [sgcl::expected\<T, E\>](README.md)
