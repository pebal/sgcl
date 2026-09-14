# gc::tracked_ptr

```cpp
#include "gc/tracked_ptr.h"   // or "gc/gc.h", or "sgcl/sgcl.h"

namespace gc {
    template<class T>
    class tracked_ptr;
}
```

`gc::tracked_ptr<T>` is an [`sgcl::tracked_ptr`](../tracked_ptr.md) that may live anywhere. Inside a managed object or on a stack it *is* a `tracked_ptr<T>`: one word, the write barrier, no count, the same cost. In any other memory, where a `tracked_ptr` may not live (`new`/`malloc` memory, a `std` container, a global, a `thread_local`, a lambda copied to the heap, the frame of a plain coroutine) the word is instead the address of a *cell*: a word of a managed *block* of cells, a cache line of them (sixteen on a 128-byte line, eight on a 64-byte one), which is a root by its state, so the cell roots the object wherever the `gc::tracked_ptr` itself is. The cell is taken by the constructor from the thread's allocator, which makes a block once per line of cells and hands its cells out in order, and is given back by the destructor, on whatever thread that runs; it belongs to that one pointer in between: no store allocates, so two threads storing into the same pointer race on one atomic word, as on an `sgcl::tracked_ptr`, never on the making of a cell; no move takes a cell from another pointer, so a thread reading through a pointer another thread moves from reads a cell that lives as long as its pointer. A block is freed by the collector in the cycle that finds every cell of it given back, once its allocator has let go of it (the last cell handed out, or the thread gone); a cell given back marks itself free by holding its own address, so a block holds no word of data beside its cells. The mode is decided once, by the address of the `gc::tracked_ptr` when it is constructed, and read back from the word (the sign bit marks a cell); the collector never sees the cell form, since it only exists in memory the collector does not read.

What it costs, against `sgcl::tracked_ptr` (one thread, `benchmarks/gc_tracked_ptr.cpp`): a copy onto the stack 2.0 ns against 1.3 (a thread-local read for the stack bounds), a store into a member of a managed object 1.8 against 1.7 (the test of the mode), a dereference 0.60 against 0.44 (the test of the sign). In unmanaged memory every construction takes a cell, a null included, and every store afterwards is a store with the barrier into it, 2.2 ns; a construction and destruction together, a cell taken and given back, 7.4 ns (13.4 when every cell was a managed object of its own). A `std::vector<gc::tracked_ptr<T>>` of a million elements is a million cells in 62,500 blocks of one line, 8 bytes per pointer; its reallocation takes a million and gives back a million, and the blocks given back go with the next cycle. A long-lived pointer pins its block, one line, for as long as it lives.

The intended use is code that cannot follow rule 1, above all code that is not written against this library: a `gc.new<T>` arena for [cppfront](https://github.com/hsutter/cppfront), a plugin holding a managed object in its own structures, a `std::function` whose closure lands on the heap. Code that can keep its pointers in managed objects and on stacks keeps using `tracked_ptr`.

## Rules

- A `gc::tracked_ptr` lives anywhere. Inside a managed object or on a stack it follows every rule of `tracked_ptr` (no `union` with data, no `std::variant`, no small-buffer `std::function` or `std::any` holding one: [The rules](../README.md#the-rules), 2); in unmanaged memory any placement is fine, as the cell is an ordinary managed object.
- The mode is that of the address at construction, so a `gc::tracked_ptr` must be constructed where it lives: no `memcpy` of a `gc::tracked_ptr` from a stack into a `std::vector` (a `std` container move-constructs a type with a non-trivial move, which is what `gc::tracked_ptr` has).
- It addresses a managed object or a part of it, never an element of a container's buffer, never an object a `unique_ptr` owns; a destructor reads its `gc::tracked_ptr` members only through `if_alive()`; a `gc::tracked_ptr` written by one thread and read by another needs the program's own synchronization. As for `tracked_ptr`, and with the same debug assertions.
- A `gc::tracked_ptr` in unmanaged memory owns its cell for its whole life: a `gc::tracked_ptr` that outlives the thread or the program phase that made it keeps its object exactly as long as itself, no longer (the thread gone, its block lives on until the last cell of it is given back), and a pointer moved from keeps its cell and its value, as a moved-from `sgcl::tracked_ptr` does.

## Members

### element_type, tracked_type

```cpp
using element_type = T;
using tracked_type = sgcl::tracked_ptr<T>;
```

`tracked_type` is what the pointer is inside managed memory. The containers of both namespaces read it: an element type that names a `tracked_type` is stored in the container's buffer or node as that type, one word in the tracked mode, and handed out as the element type it was given, so that a `gc::vector<gc::tracked_ptr<T>>` costs what an `sgcl::vector<sgcl::tracked_ptr<T>>` does and `v[i]` is still a `gc::tracked_ptr<T>&` ([the gc namespace](../README.md#the-gc-namespace)).

### Constructors

```cpp
tracked_ptr() noexcept;
tracked_ptr(std::nullptr_t) noexcept;

template<class U, std::enable_if_t<std::is_convertible_v<U*, element_type*>, int> = 0>
explicit tracked_ptr(U* p) noexcept;

tracked_ptr(const tracked_ptr& p) noexcept;
template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr(const tracked_ptr<U>& p) noexcept;

tracked_ptr(tracked_ptr&& p) noexcept;
template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr(tracked_ptr<U>&& p) noexcept;

template<class U, std::enable_if_t<std::is_convertible_v<typename sgcl::tracked_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr(const sgcl::tracked_ptr<U>& p) noexcept;

template<class U, std::enable_if_t<std::is_convertible_v<typename sgcl::unique_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr(sgcl::unique_ptr<U>&& u) noexcept;
```

As the constructors of `sgcl::tracked_ptr`, plus one from an `sgcl::tracked_ptr<U>`. Every constructor first decides the mode from its own address (the heap's range, then the thread's stack, which registers the thread on first contact). In unmanaged memory every constructor takes the pointer's cell from the thread's allocator, a null included. A move is a copy, as for `sgcl::tracked_ptr`: the source keeps its value and its cell.

```cpp
struct Node { int value = 7; gc::tracked_ptr<Node> next; };

gc::tracked_ptr node = sgcl::make_tracked<Node>();   // on the stack: a tracked_ptr
node->next = sgcl::make_tracked<Node>();          // inside a managed object: a tracked_ptr
std::vector<gc::tracked_ptr<Node>> kept;
kept.push_back(node);                             // a std container: a cell, the Node's root
```

### Destructor

```cpp
~tracked_ptr() noexcept;
```

A `tracked_ptr`'s destructor where the word is one; in unmanaged memory the cell is given back at once (one store), and the object lives on only if something else reaches it.

### operator=

```cpp
tracked_ptr& operator=(std::nullptr_t) noexcept;
tracked_ptr& operator=(const tracked_ptr& p) noexcept;
template<class U> tracked_ptr& operator=(const tracked_ptr<U>& p) noexcept;
tracked_ptr& operator=(tracked_ptr&& p) noexcept;
template<class U> tracked_ptr& operator=(tracked_ptr<U>&& p) noexcept;
template<class U> tracked_ptr& operator=(const sgcl::tracked_ptr<U>& p) noexcept;
template<class U> tracked_ptr& operator=(sgcl::unique_ptr<U>&& u) noexcept;
```

The assignments of `tracked_ptr`, each with `U*` convertible to `T*`. Every store goes into the pointer's own word, the cell's in unmanaged memory; a move assignment is a copy.

### operator sgcl::tracked_ptr<T>

```cpp
operator sgcl::tracked_ptr<element_type>() const noexcept;
```

The pointer as a `tracked_ptr`: a copy, which the caller puts where a `tracked_ptr` may live. Lets a `gc::tracked_ptr` go wherever a `tracked_ptr<T>` is expected; a function template deducing `U` from a `tracked_ptr<U>` (the constructor of [`weak_ptr`](../weak_ptr.md), for one) needs the conversion spelled out.

```cpp
gc::tracked_ptr node = sgcl::make_tracked<Node>();
sgcl::tracked_ptr<Node> t = node;
sgcl::weak_ptr<Node> w(t);                     // not w(node): U is deduced from the argument
```

### get, operator*, operator->, operator bool

```cpp
element_type* get() const noexcept;
T& operator*() const noexcept;
T* operator->() const noexcept;
explicit operator bool() const noexcept;
```

As for `tracked_ptr`; `get()` is a load and a test of the sign of the word, and one more load through a cell.

### reset, swap

```cpp
void reset() noexcept;
void reset(element_type* p) noexcept;
void swap(tracked_ptr& p) noexcept;
```

As for `tracked_ptr`. `reset()` in unmanaged memory keeps the cell.

### if_alive, type, is, as

```cpp
tracked_ptr if_alive() const noexcept;
const std::type_info& type() const noexcept;
template<class U> bool is() const noexcept;
template<class U> tracked_ptr<U> as() const noexcept;
```

As for `tracked_ptr`.

## atomic, atomic_ref, weak_ptr

[`atomic<gc::tracked_ptr<T>>`](../atomic.md) and [`atomic_ref<gc::tracked_ptr<T>>`](../atomic_ref.md) are the atomics of a `gc::tracked_ptr`: the operations of their `tracked_ptr` counterparts on the word the `gc::tracked_ptr` resolves to, so a global shared pointer is `static sgcl::atomic<gc::tracked_ptr<Config>> current;` with no `unique_ptr` around it (`gc::atomic` is `sgcl::atomic`). [`gc::weak_ptr<T>`](../weak_ptr.md), which is `sgcl::weak_ptr<T, gc::tracked_ptr>`, is the weak pointer that lives anywhere: a `gc::tracked_ptr` to the cell, `lock()` a `gc::tracked_ptr<T>`, deduced from a `gc::tracked_ptr` by `sgcl::weak_ptr w = node;`, and convertible to and from `sgcl::weak_ptr<T>`.

```cpp
static gc::atomic<gc::tracked_ptr<Config>> current;         // a global, no unique_ptr around it
current.store(gc::make_tracked<Config>());
gc::tracked_ptr<Config> config = current.load();            // a reader: held until dropped

std::vector<gc::weak_ptr<Node>> observers;                  // weak pointers in a std container
observers.push_back(node);
if (auto p = observers[0].lock()) { p->value = 1; }
```

## Free functions

```cpp
template<class T, class U> std::strong_ordering operator<=>(const tracked_ptr<T>&, const tracked_ptr<U>&) noexcept;
template<class T, class U> bool operator==(const tracked_ptr<T>&, const tracked_ptr<U>&) noexcept;
template<class T, class U> std::strong_ordering operator<=>(const tracked_ptr<T>&, const sgcl::tracked_ptr<U>&) noexcept;
template<class T, class U> bool operator==(const tracked_ptr<T>&, const sgcl::tracked_ptr<U>&) noexcept;
template<class T> std::strong_ordering operator<=>(const tracked_ptr<T>&, std::nullptr_t) noexcept;
template<class T> bool operator==(const tracked_ptr<T>&, std::nullptr_t) noexcept;
template<class T, class U> tracked_ptr<T> static_pointer_cast(const tracked_ptr<U>& p) noexcept;
template<class T, class U> tracked_ptr<T> const_pointer_cast(const tracked_ptr<U>& p) noexcept;
template<class T, class U> tracked_ptr<T> dynamic_pointer_cast(const tracked_ptr<U>& p) noexcept;
template<class T> std::ostream& operator<<(std::ostream& s, const tracked_ptr<T>& p);
```

Comparisons by address, with a `gc::tracked_ptr`, a `tracked_ptr` (either order, through the rewritten candidates of C++20) or `nullptr`; the casts return a `gc::tracked_ptr`. `std::hash<gc::tracked_ptr<T>>` hashes the address, and deduction guides let `gc::tracked_ptr node = sgcl::make_tracked<Node>()` deduce `gc::tracked_ptr<Node>`.
