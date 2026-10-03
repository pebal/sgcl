[sgcl](../README.md) › [core](README.md)

# sgcl::expected\<T, E\>

```cpp
#include "sgcl/core/expected.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, class E>
    class expected;

    template<class E>
    class unexpected;

    struct unexpect_t;
    inline constexpr unexpect_t unexpect{};

    template<class E>
    class bad_expected_access;
}
```

`sgcl::expected<T, E>` is `std::expected` (C++23) for a value or an error that holds a tracked pointer.
`std::expected` keeps the two in a union, where a `tracked_ptr` value shares its word with the error's data (the
offset leaves the collector's pointer map by elimination:
[Pointer maps](../../garbage_collector/overview.md#pointer-maps)). Here they lie in a [variant](variant.md): a
pointer word (`tracked_ptr` of either kind, [weak_ptr](weak_ptr.md)) in a word of its own, a value or an error that
may hold pointers among its data in a place of its own, the pointer-free ones in the shared data storage. The
library is C++20, so [unexpected](unexpected.md), `unexpect`, `unexpect_t` and
[bad_expected_access](bad_expected_access.md) are the library's own, with the interfaces of the `std` ones.

The interface is that of `std::expected`, with two departures that make it simpler to use. The value goes wherever
a `T` is wanted, with no `*`: an `expected<T, E>` converts to its value, and to anything its value converts to, so
that a result is passed on as it comes, and the conversion without a value throws as `value()` does
([value, operator U](expected/value.md)). And `*` and `->` are checked: `std::expected`'s are undefined without a
value, these throw `bad_expected_access<E>` (DESIGN 220). The rest is `std::expected`'s, `expected<void, E>` for a
success without a value included. Nothing is `constexpr`, and the `expected` of pointer-free types is what
`std::expected` is for. A Go function returns `(T, error)`, two values side by side, and the caller checks the
error by hand; here the two are one value, which the monadic operations chain and which throws when its value is
taken without one.

## Rules

- The `expected` has no word of its own. Where it may live is decided by its value and its error: with a
  `tracked_ptr` inside, where a `tracked_ptr` may. The value and the error follow their own rules where the
  `expected` lives ([The rules](README.md#the-rules), 1).
- A value replaced by an error, or the other way round, is destroyed: a pointer value's object is unreferenced from
  then on.
- An assignment or a `swap` never leaves the `expected` with neither a value nor an error, as `std::expected` does
  not: when the construction of the new one may throw, it is made in a temporary first if it moves without
  throwing, else the old one is moved out and put back on a throw. So an assignment is offered only when the value
  or the error moves without throwing (the constraints of `std::expected`), and `emplace` only for a construction
  that cannot throw; a value or an error that the standard library would refuse is refused here.
- `value()`, `*`, `->` and the conversion without a value throw a `bad_expected_access<E>` that carries a copy of
  the error; its `what()` is the error's `message()` when it has one. An exception object lives in unmanaged
  memory, where a tracked pointer may not ([The rules](README.md#the-rules), 1), so the exception holds its copy in
  a managed object through a [rooted](rooted.md): an error type with a `tracked_ptr` or a `string` in it is thrown
  and caught like any other ([bad_expected_access](bad_expected_access.md)).
- Thread safety is that of `std::expected`: concurrent readers, or one writer, with the program's own
  synchronization ([The rules](README.md#the-rules), 6).

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the value: an object type that is not `std::in_place_t`, `unexpect_t` or an `unexpected`, or `void` ([Specializations](#specializations)). |
| `E` | The type of the error: an object type that is not an array, not `const` or `volatile`, and not an `unexpected`. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `error_type` | `E` |
| `unexpected_type` | `unexpected<E>` |
| `rebind<U>` | `expected<U, E>`, an alias template |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](expected/expected.md) | constructs the `expected` with a value or an error |
| `(destructor)` | destroys the value or the error |
| [operator=](expected/operator_assign.md) | assigns another `expected`, a value or an error |

#### Observers

| Function | Description |
|---|---|
| [operator->, operator*](expected/operator_deref.md) | the value, checked |
| [operator bool, has_value](expected/operator_bool.md) | checks whether there is a value |
| [value, operator U](expected/value.md) | the value, `bad_expected_access` without one; the conversion to the value |
| [error](expected/error.md) | the error |
| [value_or](expected/value_or.md) | the value, or another one when there is none |
| [error_or](expected/error_or.md) | the error, or another one when there is none |

#### Monadic operations

| Function | Description |
|---|---|
| [and_then](expected/and_then.md) | the result of a function of the value, which returns an `expected`, or the error |
| [transform](expected/transform.md) | an `expected` of the result of a function of the value, or of the error |
| [or_else](expected/or_else.md) | the value, or the result of a function of the error, which returns an `expected` |
| [transform_error](expected/transform_error.md) | the value, or an `expected` of the result of a function of the error |

#### Modifiers

| Function | Description |
|---|---|
| [emplace](expected/emplace.md) | constructs a value in place |
| [swap](expected/swap.md) | swaps the contents of two `expected` objects |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](expected/operator_cmp.md) | compares with an `expected`, a value or an `unexpected` |
| [swap](expected/swap2.md) | swaps the contents of two `expected` objects |

#### Helper classes and objects

| Name | Description |
|---|---|
| [unexpected](unexpected.md) | the error, wrapped so that the constructor of an `expected` takes it as one |
| [bad_expected_access](bad_expected_access.md) | the exception thrown when the value is asked for and there is an error |
| `unexpect_t`, `unexpect` | the tag that constructs an `expected` with an error in place, `expected(unexpect, args...)` |

## Specializations

`expected<void, E>` is a success without a value, or an error, over a `variant<monostate, E>`. Its interface is
that of the primary template without what a value needs: no `operator->`, no conversion and no `value_or`;
`emplace()` takes no argument and makes it a success; `operator*` returns `void` and checks nothing; `value()`
returns `void` and throws `bad_expected_access<E>` on an error; `and_then` and `transform` call their function with
no argument, `or_else`'s function returns an `expected<void, G>`. An assignment of an error whose construction
throws leaves a success behind, never nothing.

## Complexity

Every operation is constant, plus the operation of the value or the error it runs.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A lookup that either finds an object or says why not; the caller chains the steps and reads one
// error at the end
struct User {
    string name;
    tracked_ptr<User> manager;
};

using Users = sorted_map<string, tracked_ptr<User>>;

expected<tracked_ptr<User>, string> find(const Users& users, const string& name) {
    auto it = users.find(name);
    if (it == users.end()) {
        return unexpected("no user " + name);
    }
    return it->second;
}

expected<tracked_ptr<User>, string> manager_of(const tracked_ptr<User>& user) {
    if (!user->manager) {
        return unexpected(user->name + " has no manager");
    }
    return user->manager;
}

int main() {
    Users users;
    users["ann"] = make_tracked<User>("ann");
    users["bob"] = make_tracked<User>("bob", users["ann"]);
    for (auto name : {"bob", "ann", "eve"}) {
        auto name_of = [](const tracked_ptr<User>& m) { return m->name; };
        auto result = find(users, name).and_then(manager_of).transform(name_of);
        println("{}: {}", name, result ? *result : result.error());
    }
}
```

Output:

```text
bob: ann
ann: ann has no manager
eve: no user eve
```

## See also

- [variant](variant.md): the storage
- [any](any.md), [function](function.md): the other `std` interfaces made safe for tracked pointers
- [rooted](rooted.md): how the exception keeps the error
- [tracked_ptr](tracked_ptr.md), [weak_ptr](weak_ptr.md)
- [Pointer maps](../../garbage_collector/overview.md#pointer-maps), [README: The rules](README.md#the-rules)
