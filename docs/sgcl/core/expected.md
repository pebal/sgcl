# sgcl::expected

```cpp
#include "sgcl/core/expected.h"   // or "sgcl/sgcl.h"

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

`sgcl::expected<T, E>` is `std::expected` (C++23) for a value or an error that holds a tracked pointer. `std::expected` keeps the two in a union, where a `tracked_ptr` value shares its word with the error's data (the offset leaves the collector's pointer map by elimination: [README: Pointer maps](../../garbage_collector/overview.md#pointer-maps)). Here they lie in a [`variant`](variant.md): a pointer word (`tracked_ptr` of either kind, [`weak_ptr`](weak_ptr.md)) in a word of its own, a value or an error that may hold pointers among its data in a place of its own, the pointer-free ones in the shared data storage. The library is C++20, so `unexpected`, `unexpect`, `unexpect_t` and `bad_expected_access` are the library's own, with the interfaces of the `std` ones.

The interface is that of `std::expected`: the constructors (from a value, from an `unexpected`, from an `expected<U, G>`, `in_place`, `unexpect`), the assignments, `emplace`, `swap`, `operator->`, `operator*`, `operator bool`, `has_value`, `value` (`bad_expected_access<E>` carrying the error when there is none), `error`, `value_or`, `error_or`, the monadic `and_then`, `or_else`, `transform`, `transform_error`, the comparisons with an `expected`, a value and an `unexpected`; `expected<void, E>` for a success without a value. Not `constexpr`, and the `expected` of pointer-free types is what `std::expected` is for.

The `expected` has no word of its own. Where it may live is decided by its value and its error: with a `tracked_ptr` inside, where a `tracked_ptr` may.

## Rules

- The value and the error follow their own rules where the `expected` lives ([The rules](README.md#the-rules), 1).
- A value replaced by an error, or the other way round, is destroyed: a pointer value's object is unreferenced from then on.
- An assignment or a `swap` never leaves the `expected` with neither a value nor an error, as `std::expected` does not: when the construction of the new one may throw, it is made in a temporary first if it moves without throwing, else the old one is moved out and put back on a throw. So an assignment is offered only when the value or the error moves without throwing (the constraints of `std::expected`), and `emplace` only for a construction that cannot throw; a value or an error that the standard library would refuse is refused here.
- `value()` without a value throws a `bad_expected_access<E>` that carries a copy of the error. An exception object lives in unmanaged memory, where a tracked pointer may not ([The rules](README.md#the-rules), 1), so the exception holds its copy in a managed object through a `root_ptr`: an error type with a `tracked_ptr` or a `string` in it is thrown and caught like any other, the error alive for as long as the exception is, its copies included. One managed allocation per throw.
- Thread safety is that of `std::expected` ([The rules](README.md#the-rules), 6).

## Members

```cpp
using value_type = T;
using error_type = E;
using unexpected_type = unexpected<E>;
template<class U> using rebind = expected<U, E>;

expected();                                                          // a value-initialized T
expected(const expected&);
expected(expected&&);
template<class U, class G> explicit(...) expected(const expected<U, G>&);
template<class U, class G> explicit(...) expected(expected<U, G>&&);
template<class U = T> explicit(!std::is_convertible_v<U, T>) expected(U&& v);
template<class G> explicit(...) expected(const unexpected<G>&);
template<class G> explicit(...) expected(unexpected<G>&&);
template<class... A> explicit expected(std::in_place_t, A&&...);        // and with an initializer_list
template<class... A> explicit expected(unexpect_t, A&&...);             // and with an initializer_list

expected& operator=(const expected&);
expected& operator=(expected&&);
template<class U = T> expected& operator=(U&& v);
template<class G> expected& operator=(const unexpected<G>&);
template<class G> expected& operator=(unexpected<G>&&);
template<class... A> T& emplace(A&&...);                               // and with an initializer_list
void swap(expected&);

const T* operator->() const noexcept;  T* operator->() noexcept;
const T& operator*() const& noexcept;  T& operator*() & noexcept;  T&& operator*() && noexcept;   // and const&&
explicit operator bool() const noexcept;
bool has_value() const noexcept;
const T& value() const&;  T& value() &;  T&& value() &&;              // bad_expected_access<E> without one; and const&&
const E& error() const& noexcept;  E& error() & noexcept;  E&& error() && noexcept;   // and const&&
template<class U> T value_or(U&&) const&;  template<class U> T value_or(U&&) &&;
template<class G = E> E error_or(G&&) const&;  template<class G = E> E error_or(G&&) &&;

template<class F> auto and_then(F&& f);          // f(value) -> expected<U, E>; the error passed through; &, const&, &&, const&&
template<class F> auto or_else(F&& f);           // f(error) -> expected<T, G>; the value passed through
template<class F> auto transform(F&& f);         // expected<f(value), E>
template<class F> auto transform_error(F&& f);   // expected<T, f(error)>

friend bool operator==(const expected&, const expected<T2, E2>&);
friend bool operator==(const expected&, const T2&);
friend bool operator==(const expected&, const unexpected<E2>&);
friend void swap(expected&, expected&);
```

`expected<void, E>` has no value: `emplace()`, `operator*()` and `value()` return `void`, `and_then` and `transform` call `f` with no argument, `or_else`'s `f` returns an `expected<void, G>`.

`unexpected<E>` holds the error (`error()` in the four reference forms, `swap`, `==`, a deduction guide: `unexpected("text")` is an `unexpected<const char*>`); `bad_expected_access<E>` derives from `bad_expected_access<void>`, which derives from `std::exception`, and carries the error (`error()`).

```cpp
struct Node { int value; };
using Result = expected<tracked_ptr<Node>, string>;

Result find(int key) {
    if (key < 0) {
        return unexpected("negative key");
    }
    return make_tracked<Node>(key);           // the pointer in a word of its own, the string elsewhere
}

Result a = find(1), b = find(-1);
assert(a && (*a)->value == 1 && !b && b.error() == "negative key");
assert(b.value_or(nullptr) == nullptr);
expected<int, string> v = a.and_then([](const tracked_ptr<Node>& n) -> expected<int, string> { return n->value * 2; });
assert(*v == 2 && b.transform([](const tracked_ptr<Node>& n) { return n->value; }).error() == "negative key");
try {
    b.value();
} catch (const bad_expected_access<string>& e) {
    assert(e.error() == "negative key");
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A lookup that either finds an object or says why not; the caller
// chains the steps and reads one error at the end.
struct User {
    string name;
    tracked_ptr<User> manager;
};

expected<tracked_ptr<User>, string> find(const sorted_map<string, tracked_ptr<User>>& users, const string& name) {
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
    sorted_map<string, tracked_ptr<User>> users;
    users["ann"] = make_tracked<User>(User{"ann"});
    users["bob"] = make_tracked<User>(User{"bob", users["ann"]});
    for (auto name : {"bob", "ann", "eve"}) {
        auto result = find(users, name).and_then(manager_of).transform([](const tracked_ptr<User>& m) { return m->name; });
        std::cout << name << ": " << (result ? *result : result.error()) << "\n";
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

- [variant](variant.md): the storage; [any](any.md), [function](function.md): the other `std` interfaces made safe for tracked pointers
- [tracked_ptr](tracked_ptr.md), [weak_ptr](weak_ptr.md)
- README: [variant, any, function and expected](README.md#variant-any-function-and-expected), [Pointer maps](../../garbage_collector/overview.md#pointer-maps), [The rules](README.md#the-rules)
- `tests/core/expected.cpp`: every behaviour above, checked.
