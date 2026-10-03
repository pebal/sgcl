[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::expected\<T, E\>::transform

```cpp
template<class F> auto transform(F&& f) & noexcept(/* see below */);          // (1)
template<class F> auto transform(F&& f) const& noexcept(/* see below */);     // (2)
template<class F> auto transform(F&& f) && noexcept(/* see below */);         // (3)
template<class F> auto transform(F&& f) const&& noexcept(/* see below */);    // (4)
```

The value mapped by a function that cannot fail: when there is a value, an `expected<U, E>` holding what `f` returns
when called with it (through `std::invoke`); when there is an error, an `expected<U, E>` holding the error. `U` is
the result of `f` without `const`; an `f` that returns nothing gives an `expected<void, E>`.

- (1–2) The value and the error are passed and copied as lvalues.
- (3–4) They are passed and moved as rvalues.

For `expected<void, E>`, `f` is called with no argument.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the function called with the value |

## Return value

`expected<U, E>` with the result of `f`, or with the error.

## Complexity

Constant, plus the call of `f` or the copy or the move of the error.

## Exceptions

What `f` throws, and what the construction of the result from `f`'s result or from the error throws. The function
is noexcept when the call of `f` and those constructions are.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    expected<int, string> n = 21;
    expected<int, string> failed = unexpected("no number");

    auto twice = [](int x) { return x * 2; };
    println("{} {}", *n.transform(twice), failed.transform(twice).error());

    expected<string, string> text = n.transform([](int x) { return string(x, '*'); });
    println("{}", text->size());
}
```

Output:

```text
42 no number
21
```

## See also

- [and_then](and_then.md): a function of the value that may fail
- [transform_error](transform_error.md): the error mapped
- [sgcl::expected\<T, E\>](../expected.md)
