[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::expected\<T, E\>::value, operator U

```cpp
const T& value() const&;                                                                        // (1)
T& value() &;                                                                                   // (2)
const T&& value() const&&;                                                                      // (3)
T&& value() &&;                                                                                 // (4)
template<class U>
requires (!std::same_as<std::remove_cvref_t<U>, bool>) && std::is_convertible_v<const T&, U>
operator U() const&;                                                                            // (5)
template<class U>
requires (!std::same_as<std::remove_cvref_t<U>, bool>) && std::is_convertible_v<T&&, U>
operator U() &&;                                                                                // (6)
```

The value; without one, `bad_expected_access<E>` carrying a copy of the error (moved from an rvalue `expected`).

- (1–4) A reference to the value, an rvalue reference from an rvalue `expected`.
- (5–6) The conversion: the value, converted to `U`, wherever a `T` or anything a `T` converts to is wanted. Takes
  part only when `U` is not `bool`, not a number when `T` is `bool`, not an `expected`, and not a type that takes
  the `expected` whole (one constructible from `std::in_place_t` and the `expected`, an `optional<expected<T, E>>`).

`expected<void, E>` has `void value() const&` and `void value() &&`, which return nothing and throw on an error, and
no conversion.

## Parameters

None.

## Return value

- (1–4) A reference to the value.
- (5–6) The value as a `U`.

## Complexity

Constant, plus the conversion of the value (5–6).

## Exceptions

`bad_expected_access<E>` when there is no value, and what the copy or the move of the error into it throws; (5–6)
what the conversion of the value throws.

## Notes

The conversion is the library's departure from `std::expected`, which has none: a result is passed on as it comes,
and the rule of thumb is that in `auto` there is the `expected`, in a named type the value. When there is no value,
the conversion throws as `value()` does, and the exception's `what()` is the error's `message()` ("open log.gz: No
such file or directory"), so code with nothing to do on an error needs no test. What the conversion does not do,
and where `*` (or `value()`) is still written:

- a deduced template sees the `expected` itself;
- an overload set of types that convert to each other (a `string`, a `slice<const char>`, a `std::string_view`) is
  ambiguous;
- arithmetic: `n + 1` for an `expected<int, E>` is ambiguous, `*n + 1` is not;
- a `variant` of the value's type: `variant<string, int> v = *e`;
- `bool`: never, so that `if (e)` asks whether there is a value, an `expected<bool, E>` included, and an
  `expected<bool, E>` converts to no number either;
- another `expected` takes the value or the error as they are (`expected<long, E> l = expected<int, E>(…)` carries an
  error on), and an `optional<expected<T, E>>` takes the `expected` whole.

Each of these is an error at compile time, never a quiet change of meaning.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Error {
    string text;
    string message() const {
        return text;
    }
};

expected<string, Error> read_name(bool found) {
    if (!found) {
        return unexpected(Error{"open name.txt: No such file or directory"});
    }
    return string("ann");
}

void greet(const string& name) {
    println("hello {}", name);
}

int main() {
    string name = read_name(true);  // a named type: the value
    greet(read_name(true));  // a parameter of the value's type: the value
    println("{}", read_name(true).value());

    auto result = read_name(false);  // auto: the expected itself
    if (!result) {
        println("{}", result.error().text);
    }
    try {
        greet(read_name(false));
    } catch (const bad_expected_access<Error>& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
hello ann
ann
open name.txt: No such file or directory
open name.txt: No such file or directory
```

Where the conversion stops:

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

template<class X>
concept Addable = requires(X x) { x + 1; };

int main() {
    using Number = expected<int, string>;
    println("{} {}", std::is_convertible_v<Number, long>, Addable<Number>);
    using Flag = expected<bool, string>;
    println("{} {}", std::is_convertible_v<Number, bool>, std::is_convertible_v<Flag, int>);

    expected<int, string> failed = unexpected("no number");
    expected<long, string> wider = failed;  // the error carried on, nothing thrown
    optional<expected<int, string>> kept = failed;  // the expected whole
    println("{} {}", wider.error(), kept->error());
}
```

Output:

```text
true false
false false
no number no number
```

## See also

- [operator->, operator*](operator_deref.md): the value, checked, under the operators
- [value_or](value_or.md): the value, or another one when there is none
- [bad_expected_access](../bad_expected_access.md): the exception and its `what()`
- [sgcl::expected\<T, E\>](../expected.md)
