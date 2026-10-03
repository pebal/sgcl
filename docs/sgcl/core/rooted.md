[sgcl](../README.md) › [core](README.md)

# sgcl::rooted\<T\>

```cpp
#include "sgcl/core/rooted.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    class rooted;
}
```

`sgcl::rooted<T>` is a value kept in a managed object of its own, held by a [root_ptr](root_ptr.md): for a value
with tracked pointers inside (a `string`, a `slice`, a container, a `function`, an object with a `tracked_ptr`
member) that has to lie where a `tracked_ptr` may not ([The rules](README.md#the-rules), 1): an exception object,
which the runtime allocates; a `std` container; a global; the closure a platform keeps, such as a block or a
callback's context. The constructor makes the value in the managed heap, in place from its arguments or from a
value moved in, and the value lives for as long as any `rooted` holding it does, through the copies the runtime
makes of an exception included. A `rooted` is a handle, as a `root_ptr` is: a copy shares the object (a cell of its
own, the same object), a move leaves the source empty, and it is never null otherwise.

Against `root_ptr`, which it holds inside, `rooted` behaves the same and guarantees two things more. It is never
null: it has no default constructor, so a member `rooted<Context> context;` does not compile without an
initializer, where `root_ptr<Context> context;` compiles and holds null (`const` does not help: a
`const root_ptr` member is default-initialized to null as well), and a function taking a `rooted<T>` gets a value,
not a pointer to check. And the value is made by the constructor, in place from its arguments or from a value
moved in, so the type says what the member holds: `rooted<T> r(args...)` against
`root_ptr<T> r = make_tracked<T>(args...)`, a pointer that a later assignment may empty. Where a null is a state of
the program (a handle table's free entry, a global replaced at run time, a root made from a `tracked_ptr` that is
null), `root_ptr` is the type. Go has neither: its collector scans every place a value can lie.

`rooted` is the root of every handle of the library: a value that holds its object by one tracked word (a `string`,
an `io::file`, an `io::buffer`, an `io::buffered_reader`, a `net::connection`), which lies on a stack, in a task or
in a managed object as itself and needs a root anywhere else. `rooted<io::file> log(io::open(p));` in a global or a
`std` container keeps a copy of the handle, the same file, in a managed object of its own: `log->write(...)`
writes to it, `*log` is the handle, to pass on as `const io::file&` or to test, `if (*log)`. A stream of io,
`io::reader` or `io::writer` (three words, not one), takes the same form.

## Rules

- A `rooted` lives in memory the collector does not trace: a global, a `static`, a `std` container, an exception
  object, the closure a platform keeps. Never in a managed object or in a task's frame, which is one: a root is
  never part of a cycle, and an object that reaches back to the `rooted` holding it (a channel's waiting task, a
  reader's source) would hold its own root, never collected. There the value itself lies; on a stack a `rooted` is
  pointless for the same reason.
- The value is reachable while any `rooted` holding it exists; the last one dropped and every other reference gone,
  the next cycle collects it.
- A copy shares the value; a copy of the value is `rooted<T>(*r)`. A `rooted` moved from holds nothing: it is
  destroyed or assigned to, and reading it is an assertion in debug builds.
- Threads share a `rooted` the way they share a `root_ptr` ([The rules](README.md#the-rules), 6); it may be
  destroyed on any thread.
- An exception that carries a value with tracked pointers keeps it in a `rooted` member, or is thrown as a
  `rooted<E>` whole; `bad_expected_access<E>` carries its error this way ([expected](expected.md)). An exception
  whose only payload is a message needs none: `runtime_error(msg.c_str())` copies the text.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the value: one object, made by `make_tracked<T>`. Not an array, not a reference: either is rejected at compile time. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](rooted/rooted.md) | makes the value, or shares another `rooted`'s |
| `(destructor)` | gives the cell back; the value is collected once nothing reaches it |
| [operator=](rooted/operator_assign.md) | shares another `rooted`'s value |

#### Modifiers

| Function | Description |
|---|---|
| [swap](rooted/swap.md) | exchanges the values of two `rooted`s |

#### Observers

| Function | Description |
|---|---|
| [get](rooted/get.md) | the address of the value |
| [operator\*, operator->](rooted/operator_deref.md) | the value |
| [ptr](rooted/ptr.md) | the value as a `tracked_ptr` |

## Non-member functions

| Function | Description |
|---|---|
| [swap](rooted/swap.md) | exchanges the values of two `rooted`s |

## Deduction guides

```cpp
template<class T>
rooted(T) -> rooted<T>;
```

## Complexity

Every operation is constant. What a `rooted` costs: a managed object and a root cell per `rooted`, a cell per copy,
one indirection per access.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <stdexcept>
#include <string>

using namespace sgcl;

// the place a parse failed, with the line it failed on: a string, which
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
    rooted<Context> context;  // in a managed object, alive as long as the exception
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
        println("{}", parse("12x4"));
    } catch (const parse_error& e) {
        println("{} at {} in \"{}\"", e.what(), e.context->column, e.context->line);
    }
    println("{}", parse("1234"));
}
```

Output:

```text
not a digit at 2 in "12x4"
1234
```

## See also

- [root_ptr](root_ptr.md): the root under it, for an object made elsewhere or none
- [make_tracked](make_tracked.md): creates a managed object
- [expected](expected.md): `bad_expected_access<E>` over a `rooted<E>`
- [thread](thread.md): the closure of a thread in a managed node the same way
- [README: The rules](README.md#the-rules)
- `tests/core/rooted.cpp`: every behaviour above, checked; `tests/io/rooted.cpp` and
  `NetSocket_Tests.ARootedConnection` in `tests/net/socket.cpp`: the handles of io and net in a root
