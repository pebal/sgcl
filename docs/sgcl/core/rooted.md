# sgcl::rooted

```cpp
#include "sgcl/core/rooted.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class rooted;
}
```

`rooted<T>` is a value kept in a managed object of its own, held by a [`root_ptr`](root_ptr.md): for a value with tracked pointers inside (a `string`, a `slice`, a container, a `function`, an object with a `tracked_ptr` member) that has to lie where a `tracked_ptr` may not ([The rules](README.md#the-rules), 1) — an exception object, which the runtime allocates; a `std` container; a global; the closure a platform keeps, such as a block or a callback's context. The constructor makes the value in the managed heap, in place from its arguments or from a value moved in, and the value lives for as long as any `rooted` holding it does, through the copies the runtime makes of an exception included. A `rooted` is a handle, as a `root_ptr` is: a copy shares the object (a cell of its own, the same object), a move leaves the source empty, and it is never null otherwise. What it costs: a managed object and a root cell per `rooted`, a cell per copy, one indirection per access.

Against `root_ptr`, which it holds inside, `rooted` behaves the same and guarantees two things more. It is never null: it has no default constructor, so a member `rooted<Context> context;` does not compile without an initializer, where `root_ptr<Context> context;` compiles and holds null (`const` does not help: a `const root_ptr` member is default-initialized to null as well), and a function taking a `rooted<T>` gets a value, not a pointer to check. And the value is made by the constructor, in place from its arguments or from a value moved in, so the type says what the member holds — `rooted<T> r(args...)` against `root_ptr<T> r = make_tracked<T>(args...)`, a pointer that a later assignment may empty. Where a null is a state of the program (a handle table's free entry, a global replaced at run time, a root made from a `tracked_ptr` that is null), `root_ptr` is the type.

## Rules

- A `rooted` lives anywhere, as a `root_ptr` does; in a managed object or on a stack it is pointless, since the value itself may lie there.
- The value is reachable while any `rooted` holding it exists; the last one dropped and every other reference gone, the next cycle collects it. `T` is one object, made by `make_tracked<T>`: not an array, not a reference.
- A copy shares the value, a copy of the value is `rooted<T>(*r)`. A `rooted` moved from holds nothing: it is destroyed or assigned to, and reading it is an assertion in debug builds.
- Threads share a `rooted` the way they share a `root_ptr` ([The rules](README.md#the-rules), 6); it may be destroyed on any thread.
- An exception that carries a value with tracked pointers keeps it in a `rooted` member, or is thrown as a `rooted<E>` whole; `bad_expected_access<E>` carries its error this way ([expected](expected.md)). An exception whose only payload is a message needs none: `runtime_error(msg.c_str())` copies the text.

## Members

```cpp
using value_type = T;

template<class... A> explicit rooted(std::in_place_t, A&&... a);   // the value made in place: rooted<T> r(std::in_place, args...)
template<class U = T> rooted(U&& value);                          // the value copied or moved in; rooted r(value) deduces T
rooted(const rooted&) noexcept;                                   // shares the value: a cell of its own
rooted(rooted&&) noexcept;                                        // the source empty
rooted& operator=(const rooted&) noexcept;
rooted& operator=(rooted&&) noexcept;
~rooted();                                                        // the cell given back; the value collected once unreferenced

T* get() const noexcept;                                          // null only for a rooted moved from
T& operator*() const noexcept;
T* operator->() const noexcept;
tracked_ptr<T> ptr() const noexcept;                              // the value as a tracked_ptr, for code that lives where one may
void swap(rooted&) noexcept;
```

The free function `swap`.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <stdexcept>

using namespace sgcl;

// The place a parse failed, with the line it failed on: a string, which
// is a tracked pointer, so it cannot be a member of the exception itself
struct Context {
    string line;
    size_t column;
};

struct parse_error : runtime_error {
    parse_error(const string& what, Context c)
    : runtime_error(what.c_str())
    , context(std::move(c)) {
    }
    rooted<Context> context;                       // in a managed object, alive as long as the exception
};

int parse(const string& line) {
    for (size_t i : range(line.size())) {
        if (line[i] < '0' || line[i] > '9') {
            throw parse_error("not a digit", Context{line, i});
        }
    }
    return std::stoi(line.c_str());
}

int main() {
    try {
        std::cout << parse("12x4") << "\n";
    } catch (const parse_error& e) {
        std::cout << e.what() << " at " << e.context->column << " in \"" << e.context->line << "\"\n";
    }
    std::cout << parse("1234") << "\n";
    return 0;
}
```

The output:

```
not a digit at 2 in "12x4"
1234
```

## See also

- [root_ptr](root_ptr.md): the root under it, for an object made elsewhere or none; [make_tracked](make_tracked.md); [expected](expected.md): `bad_expected_access<E>` over a `rooted<E>`; [thread](thread.md): the closure of a thread in a managed node the same way
- [The rules](README.md#the-rules)
- `tests/core/rooted.cpp`: every behaviour above, checked.
