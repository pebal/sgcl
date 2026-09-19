# SGCL

## What it is
SGCL is a C++20 application framework: one library, header-only, with no dependency beyond the standard library, that means to give a C++ program what Qt gives it and what Go's standard library gives a Go program, from the pointer up to the network and, in time, the screen. The name is from where it started, a Smart Garbage Collection Library, and the collector is still the foundation: every part of the framework is built on objects that live as long as anything reaches them, cycles included, allocated and passed around without reference counts, with a collector that runs concurrently with the program and never stops it. That foundation is what lets the rest be written the way Go and Java write it and C++ could not: lock-free structures with the textbook algorithms and no reclamation scheme, coroutines whose frames are managed objects, channels that threads and tasks share, closures that capture the objects they work on, and a declarative user interface whose view trees are rebuilt rather than patched.

What it keeps from C++ is the rest: deterministic destruction where it is wanted (`unique_ptr`), objects that never move, stack objects and raw pointers as they are, containers with the interfaces of `std`, values and pointers as C++ has them, and no runtime beyond the headers themselves. Everything is in one namespace, `sgcl` (the standard library's style, `tracked_ptr`, `make_tracked`, `vector`, `channel`, `task`), and once more behind an object-oriented face, `Sgcl` (`Ptr`, `Make`, `List`, `Channel`, `Task`), for a program written in the style of C# or Java; the two are one library and mix freely ([Two interfaces](#two-interfaces) below).

## The engine
The collector is a concurrent, non-moving, generational mark-and-sweep with a Dijkstra insertion barrier. The program never stops for it: there is no stop-the-world phase, no safepoint a thread has to reach, no handshake in the hot path, no allocation that waits for a cycle and no write barrier that does more than store a byte. A mutator thread runs at the same speed whether the collector is idle or in the middle of a cycle; the collector and its helpers take cores of their own and the memory that accumulates between two cycles. In numbers, on an Apple M2 Ultra: a pointer copy is 1.4 ns onto the stack and 1.8 ns into an object (`shared_ptr`: 5 ns alone, 100–300 ns on a shared object), an allocation 4 ns (malloc: 21 ns), and the tails of a mutator's latency are the scheduler's, not the collector's.

- **No reference counts**: a `tracked_ptr` is one word, copied with a store and a byte of state; the objects it points to may form any graph, cycles included.
- **One word, one place**: a `tracked_ptr` lives in managed objects and on stacks, where the collector looks; `root_ptr` is the root for everywhere else, a global or a `std` container, over a cell of a managed block.
- **Deterministic where it matters**: `unique_ptr` destroys its object at scope exit, on the thread that owns it; a `tracked_ptr` hands the destructor to the collector.
- **Precise heap, conservative stacks**: the heap is traced through a map per type of the words that may hold a pointer, which the collector builds by elimination as it goes, so constructors register nothing; the stacks are scanned while the threads keep running.
- **Weak pointers** one word wide, cleared by the cycle that finds the object unreachable; `expiry_queue` hands an object found unreachable to a function of your choice, alive one last time.
- **Dynamic type**: `type()`, `is<U>()` and `as<U>()` on any pointer, including `tracked_ptr<void>`, without virtual functions.
- **Diagnostics**: cycle counters and phase times, live objects and bytes by type, what holds an object and what it retains, LLDB formatters; read without stopping the collector.
- **Memory under control**: a committed-memory ceiling (90% of the cgroup or physical limit by default), a collection forced before it and `std::bad_alloc` instead of the OOM killer past it.

The chapter [docs/garbage_collector/](docs/garbage_collector/README.md) has the rest: [the engine in short](docs/garbage_collector/overview.md), [how it works](docs/garbage_collector/how-it-works.md) phase by phase, [next to the alternatives](docs/garbage_collector/alternatives.md) (`shared_ptr`, Go, Java with ZGC, in one table), [the benchmarks](docs/garbage_collector/benchmarks.md) and [the diagnostics](docs/garbage_collector/diagnostics.md).

## Modules
The framework is modules, one directory and one header each, every module depending only on those before it in the table; `#include "sgcl/sgcl.h"` brings them all in, `#include "sgcl/async/async.h"` one of them with what it needs. Each has a README that is its guide, what the classes are, the rules, what to reach for, before it lists the classes, and a page per class with every member and an example that compiles. The Go column names the counterpart in Go's standard library, the measure the framework is written against.

| module | Go | what it holds |
|---|---|---|
| [core](docs/sgcl/core/README.md) | runtime | the collector; `tracked_ptr`, `unique_ptr`, `root_ptr`, `weak_ptr`, `make_tracked`; `variant`, `any`, `function`, `expected` that keep the pointers apart from the data; `string`, immutable, one word, shared by copying, with `split`, `join`, `trim`, `replace`, `parse`, and `string_view`, a piece of it that holds the object; `range`; the dynamic type; the diagnostics; `config` |
| [containers](docs/sgcl/containers/README.md) | container/* | `vector`, `array`, `deque`, `list`, `forward_list`, `stack`, `queue`, the maps and sets, ordered and unordered, with the interfaces of `std` and their nodes and buffers managed; `ordered_map` and `ordered_set` in insertion order (Java's `LinkedHashMap`); `weak_map`, `weak_set`, `expiry_queue` |
| [concurrent](docs/sgcl/concurrent/README.md) | sync | `atomic<tracked_ptr>` and `atomic_ref` with compare-exchange and no ABA; `concurrent_queue` (Michael–Scott), `concurrent_stack` (Treiber), `concurrent_map` and `concurrent_set` (a skip list), `concurrent_unordered_map` and `concurrent_unordered_set` (a split-ordered list): the textbook algorithms with no reclamation scheme in them, because the collector is one; `copy_on_write` |
| [async](docs/sgcl/async/README.md) | goroutines, chan, context, time | coroutines whose frames are managed objects (`task`, `generator`, `async_generator`), a pool of workers that runs the tasks (`spawn`, `go`, `yield`), `channel` and `select` as Go has them, `sleep`, `after`, `tick`, `timeout`, `stop_token` for cancellation with deadlines, `when_all` and `when_any`, `mutex`, `semaphore`, `event`, `wait_group`, `once` that park a task without a thread, `readable` and `writable` on a file descriptor: the reactor |

What comes next, in this order and each on the ones before it: `io` (files, directories, async streams, the serialization of object graphs), `net` (TCP, UDP, DNS, HTTP/1.1 and HTTP/2 over the reactor and the scheduler), `text` (a linear-time regexp, templates, formatting), `time`, `encoding` (JSON into managed object graphs, XML, CSV, base64), `compress`, `hash` and `crypto` (with TLS 1.3), `codec` (the images decoded here, the video through the platform), `math`, `db`, and `ui`: a reactive state, a view as a function of it, a diff, a flex layout and events on the scheduler, over a small renderer of its own per platform.

## Two interfaces
The whole library exists twice, and the two are one implementation. The namespace `sgcl` is the standard library's style: snake_case, `tracked_ptr`, `make_tracked`, `vector`, `unordered_map`, `channel`, iterators and algorithms as `std` has them, so that a C++ program adopts it a class at a time, a `sgcl::vector` where a `std::vector` was, and so that what the framework offers reads as what it is, the standard library with a collector under it. The namespace `Sgcl` (`sgcl/Sgcl/Sgcl.h`; the framework's name in the case of its style) is the same library behind an object-oriented face: PascalCase types and methods, `Ptr`, `Make`, `List`, `Dictionary`, `String`, `Channel`, `Task`, `list.Add(x)`, `dictionary.Find(key)` (a pointer, null when absent), `ch.Send(v)`, `Spawn(task())`, for a program written in the style of C# or Java rather than of the standard library, and for the kind of program an application framework is for, where the code is read more than it is written and `Add`, `Contains`, `Join` are what the reader expects.

Why two: a framework's interface is the thing its users live in, and the two audiences want different things from it, the C++ programmer the standard library's conventions and the programmer coming from a managed language the ones they know; one style is not a compromise for both. What keeps the two from being two libraries is the implementation: every `Sgcl` class holds the one `sgcl` object (`Inner()`), every method is an inline forward, so a `List` costs what a `vector` costs, byte for byte and nanosecond for nanosecond; the semantics, the rules and the diagnostics are the same, values and pointers are as in C++ (a `List<T>` is a value, a `Ptr<List<T>>` is shared), nothing is checked that `sgcl` does not check, and the two mix freely in one program, a `List<T>` holding an `sgcl::vector<T>` and a `Ptr<T>` being an `sgcl::tracked_ptr<T>`. The headers of `Sgcl` bring the namespace in, so nothing is prefixed. Every class has its page in both references, written side by side: [docs/sgcl/](docs/sgcl/README.md) and [docs/sgcl/Sgcl/](docs/sgcl/Sgcl/README.md).

## Examples
Every example twice, in `sgcl` and in `Sgcl`. The pointers and the containers, in one file (`examples/example.cpp` has the long version):

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <memory>
#include <vector>

struct Node {
    int value;
    sgcl::vector<sgcl::tracked_ptr<Node>> edges;   // any graph, cycles included
};

int main() {
    // make_tracked returns a unique_ptr: destroyed at scope exit, deterministically
    sgcl::unique_ptr unique = sgcl::make_tracked<int>(42);
    auto also_unique = sgcl::make_tracked<int>(2);   // the same type, deduced

    // A tracked_ptr hands the object to the collector: destroyed when unreachable
    sgcl::tracked_ptr tracked = sgcl::make_tracked<int>(24);
    tracked = std::move(unique);                   // the 42 now belongs to the collector

    // A cycle, collected like anything else
    sgcl::tracked_ptr a = sgcl::make_tracked<Node>(1);
    sgcl::tracked_ptr b = sgcl::make_tracked<Node>(2);
    a->edges.push_back(b);
    b->edges.push_back(a);
    a = b = nullptr;                               // garbage, no leak

    // Base classes and the dynamic type
    struct Shape { virtual ~Shape() = default; };
    struct Circle : Shape { double r = 1; };
    sgcl::tracked_ptr<Shape> shape = sgcl::make_tracked<Circle>();
    if (shape.is<Circle>()) {
        sgcl::tracked_ptr<Circle> circle = shape.as<Circle>();
        std::cout << "a circle of radius " << circle->r << '\n';
    }
    sgcl::tracked_ptr<void> any = shape;             // type() still knows: Circle

    // An alias into a member keeps the whole object
    sgcl::tracked_ptr node = sgcl::make_tracked<Node>(7);
    sgcl::tracked_ptr<int> value(&node->value);
    node = nullptr;
    std::cout << *value << '\n';                   // 7, the Node lives on

    // Containers with the interfaces of std, their nodes and buffers managed
    sgcl::unordered_map<std::string, sgcl::tracked_ptr<Node>> index;
    sgcl::list numbers = {1, 2, 3};
    sgcl::vector<sgcl::tracked_ptr<Node>> nodes(10);

    // A tracked_ptr lives on a stack or inside a managed object; anywhere
    // else (a std container, a global, new memory) the root is a root_ptr
    // ([The pointer and its places](docs/sgcl/core/README.md#the-pointer-and-its-places)).
    std::vector<sgcl::root_ptr<Node>> kept = {value.as<Node>()};    // fine: a cell roots the Node
    static sgcl::root_ptr<Node> root = nodes[0];                    // fine: a global
    auto holder = new sgcl::root_ptr<Node>(nodes[1]);               // fine: a root in new memory
    // std::vector<sgcl::tracked_ptr<Node>> edges;                  // not allowed: never scanned, the object is lost
    // static sgcl::tracked_ptr<Node> root;                         // not allowed: a global is neither a stack nor an object
    delete holder;
}
```

The same in `Sgcl`:

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>
#include <memory>
#include <vector>

struct Node {
    int value;
    List<Ptr<Node>> edges;                         // any graph, cycles included
};

int main() {
    // Make returns a UniquePtr: destroyed at scope exit, deterministically
    UniquePtr unique = Make<int>(42);
    auto alsoUnique = Make<int>(2);                // the same type, deduced

    // A Ptr hands the object to the collector: destroyed when unreachable
    Ptr tracked = Make<int>(24);
    tracked = std::move(unique);                   // the 42 now belongs to the collector

    // A cycle, collected like anything else
    Ptr a = Make<Node>(1);
    Ptr b = Make<Node>(2);
    a->edges.Add(b);
    b->edges.Add(a);
    a = b = nullptr;                               // garbage, no leak

    // Base classes and the dynamic type
    struct Shape { virtual ~Shape() = default; };
    struct Circle : Shape { double r = 1; };
    Ptr<Shape> shape = Make<Circle>();
    if (shape.Is<Circle>()) {
        Ptr<Circle> circle = shape.As<Circle>();
        std::cout << "a circle of radius " << circle->r << '\n';
    }
    Ptr<void> any = shape;                         // Type() still knows: Circle

    // An alias into a member keeps the whole object
    Ptr node = Make<Node>(7);
    Ptr<int> value(&node->value);
    node = nullptr;
    std::cout << *value << '\n';                   // 7, the Node lives on

    // Containers with the names of a collection library, their nodes and buffers managed
    Dictionary<std::string, Ptr<Node>> index;
    LinkedList numbers = {1, 2, 3};
    List<Ptr<Node>> nodes(10);

    // A Ptr lives on a stack or inside a managed object; anywhere else
    // (a std container, a global, new memory) the root is a RootPtr
    std::vector<RootPtr<Node>> kept = {value.As<Node>()};   // fine: a cell roots the Node
    static RootPtr<Node> root = nodes[0];                   // fine: a global
    auto holder = new RootPtr<Node>(nodes[1]);              // fine: a root in new memory
    // std::vector<Ptr<Node>> edges;                        // not allowed: never scanned, the object is lost
    // static Ptr<Node> root;                               // not allowed: a global is neither a stack nor an object
    delete holder;
}
```

Tasks and a channel, the shape of a Go program: a coroutine on the scheduler receives managed objects from a thread and sends results back, waiting on either side without holding a thread, and nobody frees anything ([channel](docs/sgcl/async/channel.md), [coroutine](docs/sgcl/async/coroutine.md)):

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

struct Job {
    int id;
};

sgcl::task<> worker(sgcl::channel<sgcl::tracked_ptr<Job>>& jobs, sgcl::channel<int>& results) {
    while (auto job = co_await jobs.async_receive()) {   // suspends while jobs is empty; empty once jobs is closed and drained
        co_await results.async_send((*job)->id * 2);     // suspends while results is full
    }
    results.close();                                     // the stream ends downstream
}

int main() {
    sgcl::channel<sgcl::tracked_ptr<Job>> jobs(8);
    sgcl::channel<int> results(8);
    sgcl::task w = sgcl::spawn(worker(jobs, results));   // runs on the pool of workers whenever a job comes
    sgcl::thread producer([&] {
        for (int i : sgcl::range(100)) {
            jobs.send(sgcl::make_tracked<Job>(i));       // waits when the buffer of eight is full
        }
        jobs.close();
    });
    long sum = 0;
    for (int r : results) {                              // until results is closed
        sum += r;
    }
    producer.join();
    w.join();
    std::cout << sum << '\n';                            // 9900
}
```

The same in `Sgcl` ([Channel](docs/sgcl/Sgcl/Async/Channel.md), [Task](docs/sgcl/Sgcl/Async/Coroutine.md)):

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Job {
    int id;
};

Task<> Worker(Channel<Ptr<Job>>& jobs, Channel<int>& results) {
    while (auto job = co_await jobs.AsyncReceive()) {    // suspends while jobs is empty; None once jobs is closed and drained
        co_await results.AsyncSend((*job)->id * 2);      // suspends while results is full
    }
    results.Close();                                     // the stream ends downstream
}

int main() {
    Channel<Ptr<Job>> jobs(8);
    Channel<int> results(8);
    Task w = Spawn(Worker(jobs, results));               // runs on the pool of workers whenever a job comes
    Thread producer([&] {
        for (int i : Range(100)) {
            jobs.Send(Make<Job>(i));                     // waits when the buffer of eight is full
        }
        jobs.Close();
    });
    long sum = 0;
    for (int r : results) {                              // until results is closed
        sum += r;
    }
    producer.Join();
    w.Join();
    std::cout << sum << '\n';                            // 9900
}
```

The frame of a coroutine is heap memory too. A `root_ptr` among its parameters, locals or promise is fine in any frame; a `tracked_ptr` is allowed only when the promise derives from `managed_frame`, which `task` and `generator` do (`Task` and `Generator` in `Sgcl`), and a managed frame costs nothing at each use of the pointer ([Coroutines](docs/sgcl/async/README.md#coroutines)):

```cpp
std::generator<sgcl::root_ptr<Node>> chain(int count);       // a plain frame: each root_ptr in it roots its Node through a cell
sgcl::generator<sgcl::tracked_ptr<Node>> chain(int count);   // a managed frame: the frame itself is a managed object, no cells
// std::generator<sgcl::tracked_ptr<Node>> chain(int count); // not allowed: a plain frame is never scanned
```

## The rules, in short
1. A `tracked_ptr`, and every type that holds one (a container, a `string`, a `channel`, a `task`), lives on a stack or inside a managed object, never in unmanaged memory: a global, a `std` container, a lambda copied to the heap, a plain coroutine frame. For those places there is `root_ptr`, which lives anywhere.
2. A `tracked_ptr` never shares its word with data: no `union` with a value, no `std::variant`, `std::any` or `std::function` holding one; `sgcl::variant`, `any`, `function` and `expected` keep the pointers apart, and `optional`, `pair` and `tuple` are safe as they are.
3. A raw pointer to a managed object is not a reference the collector honours: the object lives as long as a `tracked_ptr` or a `unique_ptr` keeps it.

The rules in full, with what each costs and what breaking one looks like: [docs/sgcl/core/README.md](docs/sgcl/core/README.md#the-rules).

## Documentation
[docs/](docs/README.md) is the reference and the guide: a README per module ([core](docs/sgcl/core/README.md), [containers](docs/sgcl/containers/README.md), [concurrent](docs/sgcl/concurrent/README.md), [async](docs/sgcl/async/README.md); the same under the `Sgcl` names in [docs/sgcl/Sgcl/](docs/sgcl/Sgcl/README.md)), a page per class with every member, its signature as declared in the header, the rules that apply and an example that compiles, and the chapter on [the garbage collector](docs/garbage_collector/README.md). [docs/garbage_collector/diagnostics.md](docs/garbage_collector/diagnostics.md) is where to start when the memory grows, an object lives too long or dies too early, or a cycle costs more than it should.

## Dependencies and usage
C++20 and nothing else: no external library, no runtime to link. For LLDB, `command script import <sgcl>/lldb/sgcl.py` (in `~/.lldbinit`) shows the pointers and containers as they are ([docs/diagnostics.md](docs/garbage_collector/diagnostics.md#in-the-debugger)). Copy the `sgcl` directory into your include path and `#include "sgcl/sgcl.h"`, or add this tree with CMake and link the `sgcl` interface target. The library is four modules, one directory each and each a header of its own for a program that wants only that much: `sgcl/core/core.h` (the collector and the pointers), `sgcl/containers/containers.h`, `sgcl/concurrent/concurrent.h` and `sgcl/async/async.h`, each depending only on those before it; `Sgcl/` mirrors them (`sgcl/Sgcl/Core/Core.h`...). The tests need googletest in `external/` and build one program per module (`tests_core`, `tests_containers`, `tests_concurrent`, `tests_async`, `tests_Sgcl`; `ctest -R async` runs one); the benchmarks build with the tree, and their Go and Java counterparts need only a Go and a JDK to run `benchmarks/compare.sh`.

## Compilers and platforms
Written for clang, gcc and MSVC on macOS, Linux and Windows; the current version has been built and tested on Apple Silicon (macOS, Apple clang) only, the other platforms are pending. On Windows, gcc's handling of thread-local destructors makes it a poor choice; clang and MSVC are fine. On macOS every access to a thread-local variable is a call into the dynamic loader, which is what the registration check in a `tracked_ptr` constructor costs there (about a nanosecond); Linux and Windows read a segment register.

## License
Apache License 2.0, see [LICENSE](LICENSE). Contributions are accepted under the same terms (section 5 of the license), with no separate agreement.
