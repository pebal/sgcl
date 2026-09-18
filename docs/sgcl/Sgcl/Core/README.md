# Sgcl::Core

The collector and the pointers behind the object-oriented face: what every other module of the interface is built on. `#include "sgcl/Sgcl/Core/Core.h"` brings the module in; it is a facade over [`sgcl/core/`](../../core/README.md), whose README is the guide of the module (what the classes are, the rules, what to reach for), the same under these names and depends on nothing else. The index of the whole interface is [`docs/sgcl/Sgcl/`](../README.md).

What the module holds: `String`, `StringView` and `StringBuilder` (an immutable string of one word, shared by copying, with `Split`, `Join`, `Trim`, `Replace`, `Parse` and `ToString`; a view that holds the object), `Ptr`, the pointer the collector follows, and the two pointers around it (`UniquePtr`, what `Make` returns, and `WeakPtr`, which does not keep its object alive), `RootPtr` for a root in unmanaged memory, `Make` as the one way an object enters the managed heap, the collector's own interface, the value types that keep a `Ptr` apart from data (`Variant`, `Any`, `Function`, `Expected`), `Range` (a pair of iterators as a range, and the integers of `Range(n)`), and the `std` types under the interface's names. The engine under it, its diagnostics and its constants have a chapter of their own, [the garbage collector](../../../garbage_collector/README.md).

| page | header | what it is |
|---|---|---|
| [Ptr](Ptr.md) | `Ptr.h` | the pointer the collector follows: one word, a write barrier, no count; aliases, `Type()`, `Is<U>()`, `As<U>()`, `IfAlive()`, `ToShared()` |
| [UniquePtr](UniquePtr.md) | `Ptr.h` | what `Make` returns: a `std::unique_ptr` to a managed object, deterministic until moved into a `Ptr` |
| [Make](Make.md) | `Ptr.h` | creates an object on the managed heap |
| [RootPtr](RootPtr.md) | `Ptr.h` | a root that lives anywhere (a global, a `std` container, a lambda on the heap): a cell of a managed block under it, the `Ptr` it holds its object by one step away; the pointer of an interpreter's handle table or a program's globals |
| [WeakPtr](WeakPtr.md) | `Ptr.h` | a pointer that does not keep its object alive, cleared by the cycle that finds the object unreachable |
| [Collector](Collector.md) | `Collector.h` | `Collect`, `Terminate`, statistics and phase times, live objects and bytes by type, the memory limit |
| [Variant](Variant.md) | `Variant.h` | `std::variant`'s interface with the tracked pointers in a word of their own, apart from the data of the other alternatives |
| [Any](Any.md) | `Any.h` | `std::any`'s interface with a tracked pointer in a word of its own and an object with pointers in a managed node of its own |
| [Function, MoveOnlyFunction](Function.md) | `Function.h` | `std::function` and `std::move_only_function` whose closure may capture tracked pointers: the closure in a managed node of its own |
| [Expected](Expected.md) | `Expected.h` | `std::expected`'s interface (C++23) over a variant: the value and the error laid out apart |
| [Range](Range.md) | `Range.h` | a pair of iterators as a range, what a lookup of a key with several values hands back, and the integers of `Range(n)`, `Range(first, last)`; for a range-for |
| [String](String.md) | `String.h` | an immutable string on the managed heap: one word, shared by copying, compared and hashed by its contents, no destructor; `Split`, `Join`, `Trim`, `Replace` as C# and Java have them; `ToString(number)` |
| [StringView](StringView.md) | `String.h` | a view of a String that holds the string's object: two words, `s.View(pos, n)` a substring with no copy and no lifetime to watch; the pieces of `Split` |
| [StringBuilder](StringBuilder.md) | `StringBuilder.h` | the scratch buffer a String is built in: `Append`, then `ToString()` once; a `std::string` under the interface's names, no tracked pointer, lives anywhere |
| [Optional, None, Pair](Types.md) | `Types.h` | the `std` types under the interface's names: they hold a tracked pointer correctly as they are, one value per place |
