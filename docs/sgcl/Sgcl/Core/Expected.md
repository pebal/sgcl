# Sgcl::Expected

```cpp
#include "sgcl/Sgcl/Core/Expected.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class E>
    class Expected;
    template<class E>
    class Unexpected;
    using UnexpectType = sgcl::unexpect_t;
    inline constexpr UnexpectType Unexpect{};
    template<class E>
    class BadExpectedAccess;
    template<class E> Unexpected<std::decay_t<E>> MakeUnexpected(E&& e);
}
```

The same class in the `sgcl` interface: [expected](../../core/expected.md).

`Expected<T, E>` is `std::expected` (C++23) for a value or an error that holds a tracked pointer. `std::expected` keeps the two in a union, where a `Ptr` value shares its word with the error's data (the offset leaves the collector's pointer map by elimination: [README: Pointer maps](../../../garbage_collector/overview.md#pointer-maps)). Here they lie in a [`Variant`](Variant.md): a pointer word (`Ptr`, [`WeakPtr`](WeakPtr.md)) in a word of its own, a value or an error that may hold pointers among its data in a place of its own, the pointer-free ones in the shared data storage. The library is C++20, so `Unexpected`, `Unexpect`, `UnexpectType` and `BadExpectedAccess` are the library's own, with the interfaces of the `std` ones.

The interface is that of `std::expected` under the interface's names: the constructors (from a value, from an `Unexpected`, `in_place`, `Unexpect`), the assignments, `Emplace`, `Swap`, `operator->`, `operator*`, `operator bool`, `HasValue`, `Value` (`BadExpectedAccess<E>` carrying the error when there is none), `Error`, `ValueOr`, `ErrorOr`, the monadic `AndThen`, `OrElse`, `Transform`, `TransformError`, the comparisons with an `Expected`, a value and an `Unexpected`; `Expected<void, E>` for a success without a value. Not `constexpr`, and the `Expected` of pointer-free types is what `std::expected` is for.

The `Expected` has no word of its own. Where it may live is decided by its value and its error: with a `Ptr` inside, where a `Ptr` may.

## Rules

- The value and the error follow their own rules where the `Expected` lives ([The rules](../../core/README.md#the-rules), 1).
- A value replaced by an error, or the other way round, is destroyed: a pointer value's object is unreferenced from then on.
- `Value()` without a value throws a `BadExpectedAccess<E>` that carries a copy of the error. An exception object lives in unmanaged memory, where a tracked pointer may not ([The rules](../../core/README.md#the-rules), 1), so the exception holds its copy in a managed object through a `RootPtr`: an error type with a `Ptr` or a `String` in it is thrown and caught like any other, the error alive for as long as the exception is, its copies included. One managed allocation per throw.
- Never without both a value and an error: an assignment or `Emplace` whose construction throws leaves the old value or the old error (the standard's rule: a temporary first, or the old one moved out and put back), `Emplace` takes only a construction that cannot throw, and `Swap` is constrained as `std::expected::swap` is.
- An `Expected` of other types converts: the value or the error, whichever it holds, explicit where the value's or the error's conversion is. An `Expected<bool, E>` made from one converts the value, never `HasValue()` (LWG 3836).
- Thread safety is that of `std::expected` ([The rules](../../core/README.md#the-rules), 6).

## Members

```cpp
using ValueType = T;
using ErrorType = E;
using InnerType = sgcl::expected<T, E>;

Expected();                                                          // a value-initialized T
Expected(const Expected&);
Expected(Expected&&);
template<class U = T> Expected(U&& v);
template<class G> Expected(const Unexpected<G>&);
template<class G> Expected(Unexpected<G>&&);
template<class... A> explicit Expected(std::in_place_t, A&&...);
template<class... A> explicit Expected(UnexpectType, A&&...);
Expected(InnerType e);

Expected& operator=(const Expected&);
Expected& operator=(Expected&&);
template<class... A> T& Emplace(A&&...);
void Swap(Expected&);

const T* operator->() const noexcept;  T* operator->() noexcept;
const T& operator*() const& noexcept;  T& operator*() & noexcept;
explicit operator bool() const noexcept;
bool HasValue() const noexcept;
const T& Value() const&;  T& Value() &;  T&& Value() &&;              // BadExpectedAccess<E> without one
const E& Error() const& noexcept;  E& Error() & noexcept;
template<class U> T ValueOr(U&&) const&;  template<class U> T ValueOr(U&&) &&;
template<class G> E ErrorOr(G&&) const&;

template<class F> auto AndThen(F&& f);          // f(value) -> Expected<U, E>; the error passed through; & and const&
template<class F> auto OrElse(F&& f);           // f(error) -> Expected<T, G>; the value passed through
template<class F> auto Transform(F&& f);        // Expected<f(value), E>
template<class F> auto TransformError(F&& f);   // Expected<T, f(error)>

bool operator==(const Expected&, const Expected&);
bool operator==(const Expected&, const T2&);
bool operator==(const Expected&, const Unexpected<G>&);
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

`Expected<void, E>` has no value: `Emplace()` and `Value()` return `void`, `AndThen` and `Transform` call `f` with no argument, `OrElse`'s `f` returns an `Expected<void, G>`.

`Unexpected<E>` holds the error (`Error()` in the three reference forms, `Swap`, `==`, `Inner()`, a deduction guide: `Unexpected("text")` is an `Unexpected<const char*>`; `MakeUnexpected(e)` the same by a function); `BadExpectedAccess<E>` derives from `std::exception` and carries the error (`Error()`).

```cpp
struct Node { int value; };
using Result = Expected<Ptr<Node>, String>;

auto find = [](int key) -> Result {
    if (key < 0) {
        return MakeUnexpected(String("negative key"));
    }
    return Ptr(Make<Node>(key));   // the pointer in a word of its own, the string elsewhere
};

Result a = find(1), b = find(-1);
assert(a && (*a)->value == 1 && !b && b.Error() == "negative key");
assert(b.ValueOr(nullptr) == nullptr);
Expected<int, String> v = a.AndThen([](const Ptr<Node>& n) -> Expected<int, String> { return n->value * 2; });
assert(*v == 2 && b.Transform([](const Ptr<Node>& n) { return n->value; }).Error() == "negative key");
try {
    b.Value();
} catch (const BadExpectedAccess<String>& e) {
    assert(e.Error() == "negative key");
}
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A lookup that either finds an object or says why not; the caller
// chains the steps and reads one error at the end.
struct User {
    String name;
    Ptr<User> manager;
};

Expected<Ptr<User>, String> Find(const SortedDictionary<String, Ptr<User>>& users, const String& name) {
    if (const Ptr<User>* user = users.Find(name)) {
        return *user;
    }
    return MakeUnexpected("no user " + name);
}

Expected<Ptr<User>, String> ManagerOf(const Ptr<User>& user) {
    if (!user->manager) {
        return MakeUnexpected(user->name + " has no manager");
    }
    return user->manager;
}

int main() {
    SortedDictionary<String, Ptr<User>> users;
    users["ann"] = Make<User>(User{"ann"});
    users["bob"] = Make<User>(User{"bob", users["ann"]});
    for (auto name : {"bob", "ann", "eve"}) {
        auto result = Find(users, name).AndThen(ManagerOf).Transform([](const Ptr<User>& m) { return m->name; });
        std::cout << name << ": " << (result ? *result : result.Error()) << "\n";
    }
    // bob: ann / ann: ann has no manager / eve: no user eve
    return 0;
}
```

The output:

```
bob: ann
ann: ann has no manager
eve: no user eve
```

## See also

- [Variant](Variant.md): the storage; [Any](Any.md), [Function](Function.md): the other `std` interfaces made safe for tracked pointers
- [Ptr](Ptr.md), [WeakPtr](WeakPtr.md)
- README: [variant, any, function and expected](../../core/README.md#variant-any-function-and-expected), [Pointer maps](../../../garbage_collector/overview.md#pointer-maps), [The rules](../../core/README.md#the-rules)
