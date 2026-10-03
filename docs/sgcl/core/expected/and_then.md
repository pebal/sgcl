[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::expected\<T, E\>::and_then

```cpp
/*(1)*/ template<class F> auto and_then(F&& f) & noexcept(/* see below */);
/*(2)*/ template<class F> auto and_then(F&& f) const& noexcept(/* see below */);
/*(3)*/ template<class F> auto and_then(F&& f) && noexcept(/* see below */);
/*(4)*/ template<class F> auto and_then(F&& f) const&& noexcept(/* see below */);
```

The next step of a computation that may fail: when there is a value, the result of `f` called with it (through
`std::invoke`), which must be an `expected` with the same error type `E`; when there is an error, that `expected`
type holding the error. `f` returns some `expected<U, E>`, and so does `and_then`.

- (1–2) The value and the error are passed and copied as lvalues.
- (3–4) They are passed and moved as rvalues.

For `expected<void, E>`, `f` is called with no argument.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the function called with the value; it returns an `expected` with the error type `E` |

## Return value

What `f` returns, or an `expected` of that type with the error.

## Complexity

Constant, plus the call of `f` or the copy or the move of the error.

## Exceptions

What `f` throws, and what the copy or the move of the error throws. The function is noexcept when the call of `f`
and the construction of the result from the error are.

## Notes

An `f` whose result is not an `expected` with the error type `E` is ill-formed, with a message that says so.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

expected<int, string> parse(const string& text) {
    if (text.empty()) {
        return unexpected("empty");
    }
    return int(text.size());
}

expected<int, string> half(int n) {
    if (n % 2 != 0) {
        return unexpected("odd");
    }
    return n / 2;
}

int main() {
    for (const char* text : {"four", "odd", ""}) {
        auto result = parse(text).and_then(half);
        println("{}", result ? string("ok") : result.error());
    }
}
```

Output:

```text
ok
odd
empty
```

## See also

- [transform](transform.md): a function of the value that cannot fail
- [or_else](or_else.md): the next step after an error
- [sgcl::expected\<T, E\>](../expected.md)
