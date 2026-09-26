# SGCL

## What it is
SGCL is a C++20 application framework: one library, header-only, with no dependency beyond the standard library, that means to give a C++ program what Qt gives it and what Go's standard library gives a Go program, from the pointer up to the network and, in time, the screen. The name is from where it started, a Smart Garbage Collection Library, and the collector is still the foundation: every part of the framework is built on objects that live as long as anything reaches them, cycles included, allocated and passed around without reference counts, with a collector that runs concurrently with the program and never stops it. That foundation is what lets the rest be written the way Go and Java write it and C++ could not: lock-free structures with the textbook algorithms and no reclamation scheme, coroutines whose frames are managed objects, channels that threads and tasks share, closures that capture the objects they work on, and a declarative user interface whose view trees are rebuilt rather than patched.

What it keeps from C++ is the rest: deterministic destruction where it is wanted (`unique_ptr`), objects that never move, stack objects and raw pointers as they are, containers with the interfaces of `std`, values and pointers as C++ has them, and no runtime beyond the headers themselves. Everything is in one namespace, `sgcl`, in the standard library's style: `tracked_ptr`, `make_tracked`, `vector`, `channel`, `task`.

## The engine
The collector is a concurrent, non-moving, generational mark-and-sweep with a Dijkstra insertion barrier. The program never stops for it: there is no stop-the-world phase, no safepoint a thread has to reach, no handshake in the hot path, no allocation that waits for a cycle and no write barrier that does more than store a byte. A mutator thread runs at the same speed whether the collector is idle or in the middle of a cycle; the collector and its helpers take cores of their own and the memory that accumulates between two cycles. In numbers, on an Apple M2 Ultra: a pointer copy is 1.4 ns onto the stack and 1.8 ns into an object (`shared_ptr`: 5 ns alone, 100–300 ns on a shared object), an allocation 4 ns (malloc: 21 ns), and the tails of a mutator's latency are the scheduler's, not the collector's.

- **No reference counts**: a `tracked_ptr` is one word, copied with a store and a byte of state; the objects it points to may form any graph, cycles included.
- **One word, one place**: a `tracked_ptr` lives in managed objects and on stacks, where the collector looks; `root_ptr` is the root for everywhere else, a global or a `std` container, over a cell of a managed block.
- **Deterministic where it matters**: `unique_ptr` destroys its object at scope exit, on the thread that owns it; a `tracked_ptr` hands the destructor to the collector.
- **Precise heap, conservative stacks**: the heap is traced through a map per type of the words that may hold a pointer, which the collector builds by elimination as it goes, so constructors register nothing; the stacks are scanned while the threads keep running.
- **Weak pointers** one word wide, cleared by the cycle that finds the object unreachable; `expiry_queue` hands an object found unreachable to a function of your choice, alive one last time.
- **Dynamic type**: `type()`, `is<U>()` and `as<U>()` on any pointer, including `tracked_ptr<void>`, without virtual functions.
- **Diagnostics**: cycle counters and phase times, live objects and bytes by type, what holds an object and what it retains, LLDB formatters; read without stopping the collector.
- **Memory under control**: a committed-memory ceiling (90% of the cgroup or physical limit by default), a collection forced before it and `bad_alloc` instead of the OOM killer past it.

The chapter [docs/garbage_collector/](docs/garbage_collector/README.md) has the rest: [the engine in short](docs/garbage_collector/overview.md), [how it works](docs/garbage_collector/how-it-works.md) phase by phase, [next to the alternatives](docs/garbage_collector/alternatives.md) (`shared_ptr`, Go, Java with ZGC, in one table), [the benchmarks](docs/garbage_collector/benchmarks.md) and [the diagnostics](docs/garbage_collector/diagnostics.md).

## Modules
The framework is modules, one directory and one header each, every module depending only on those before it in the table; `#include "sgcl/sgcl.h"` brings them all in, `#include "sgcl/async/async.h"` one of them with what it needs. Each has a README that is its guide, what the classes are, the rules, what to reach for, before it lists the classes, and a page per class with every member and an example that compiles. The Go column names the counterpart in Go's standard library, the measure the framework is written against.

| module | Go | what it holds |
|---|---|---|
| [core](docs/sgcl/core/README.md) | runtime, container/*, sync/atomic | the collector; `tracked_ptr`, `unique_ptr`, `root_ptr`, `weak_ptr`, `make_tracked`; `variant`, `any`, `function`, `expected` that keep the pointers apart from the data; `string`, immutable, one word, shared by copying, with `split`, `join`, `trim`, `replace`, `parse`, and `slice`, a piece of a string (of any contiguous managed storage) that holds the object, Go's slice, a span when the memory is unmanaged; `range`; the mixins (`mixin::enumerable`, `mixin::ordered`, `mixin::lookup`...) that give every container its `contains`, `sort`, `get` and declare what it is, and the requirements (`req::enumerable`, `req::ordered`...) a parameter asks for; the dynamic type; the diagnostics; `config`; the containers: `vector`, `array`, `dynamic_array`, `deque`, `list`, `forward_list`, `stack`, `queue`, the maps and sets, ordered and unordered, with the interfaces of `std` and their nodes and buffers managed; `ordered_map` and `ordered_set` in insertion order (Java's `LinkedHashMap`); `weak_map`, `weak_set`, `expiry_queue`; `atomic<tracked_ptr>` and `atomic_ref` with compare-exchange and no ABA; `clock`, the library's time in one place; `managed_frame` and `frame_ptr`, a coroutine's frame on the managed heap, and `generator` on them. Everything in `sgcl::` itself is here; every other module is a namespace of its own |
| [immutable](docs/sgcl/immutable/README.md) | — | the immutable containers in `sgcl::immutable` (Clojure, Scala), `immutable::vector`, `immutable::list`, `immutable::map`, `immutable::set`: every operation a new version that shares all but the path it changed with the old one, which stays as it was — the state of a program as a value, compared by its root, kept as its history, read by any thread while another builds the next |
| [txt](docs/sgcl/txt/README.md) | unicode, x/text | what a human expects of text and a byte does not give: the properties of a code point (the general category, the script, `is_alpha`, `is_emoji`, the value of a digit), `columns` — the cells a code point and a text take on a terminal — and the boundaries: `graphemes` (what a reader calls a character, which a code point is not), `words`, `sentences`, `line_breaks`, the cursor moves over graphemes, `wrap` and `truncate`; normalization (UAX #15: the four forms as tags, and comparison and hashing that do not care which one a text arrived in); the full case mappings, where a letter may become two ("straße" is "STRASSE"), where a Greek sigma depends on its place in the word and where three languages spell an i their own way; searching, with a prepared pattern or blind to case or to the way a text was written; and the encodings, which is the part whose clearest use is the case where there is no Unicode yet — UTF-16 at the edge of a Windows call, bytes in iso-8859-2 out of an HTTP header. and the bidirectional algorithm, which tells whoever draws a line what order to put it in when the text runs both ways at once; and collation, the order a reader expects rather than the order the bytes fall in, in the root order of the standard or in the own order of 88 languages; and `format`, the pattern of `std::format` read where the program is compiled, whose field over a text is measured in the columns it takes and not in its bytes — which is why it is here and not in core. Its own namespace, `sgcl::txt`; the tables are generated from the UCD, and every algorithm is written from the annex and held to the UCD's own test files |
| [math](docs/sgcl/math/README.md) | math/big, math/rand/v2 | `math::big_integer`, a whole number of any size with the manners of an `int` (Karatsuba, Toom-3, Burnikel–Ziegler, conversions by divide and conquer, number theory: `pow`, `mod_pow`, `gcd`, `mod_inverse`, `sqrt`, `is_probable_prime`, `factorial`, `binomial`) (`a * 2 + 1`, `/` and `%` as C++ divides, `mod` never negative, bits in two's complement, text in bases 2 to 36, the literal `_big`), sixteen bytes and nothing allocated for a value within `int64_t`, a larger one immutable and shared by copying as a `string` is; `math::rational`, a fraction of two `big_integer`s always in lowest terms (Go's `big.Rat`), exact arithmetic that never rounds; `math::random`, ChaCha8Rand, the stream of Go's `ChaCha8` from the same key, with `next_int`, `next_double`, `next_normal`, `shuffle`, `pick`, `permutation` and a generator of the standard's for everything else. Its own namespace, `sgcl::math` |
| [concurrent](docs/sgcl/concurrent/README.md) | sync | `concurrent::queue` (Michael–Scott), `concurrent::stack` (Treiber), `concurrent::sorted_map` and `concurrent::sorted_set` (a skip list), `concurrent::map` and `concurrent::set` (a split-ordered list): the textbook algorithms with no reclamation scheme in them, because the collector is one; `copy_on_write` (`atomic` and `atomic_ref`, which they stand on, are core's) |
| [async](docs/sgcl/async/README.md) | goroutines, chan, context, time | coroutines whose frames are managed objects (`task`, `async::generator`), a pool of workers that runs the tasks (`spawn`, `go`, `yield`), `channel` and `select` as Go has them, `sleep`, `after`, `tick`, `timeout`, `stop_token` for cancellation with deadlines, `when_all` and `when_any`, `mutex`, `semaphore`, `event`, `wait_group`, `once` that park a task without a thread, `readable` and `writable` on a file descriptor: the reactor |
| [io](docs/sgcl/io/README.md) | os, io, bufio, path/filepath, os/exec | streams as requirements of one primitive each with everything else mixed in (`io::req::reader`, `writer`, `seeker`, `closer`, nothing virtual) and `io::reader`/`io::writer`, handles of one word over any of them (`read_all`, `copy`, `write`, each with an `async_` form for a task), `buffered_reader` that hands out lines as views into a managed block, `file` over any descriptor (a regular file's async reads on the blocking pool, a pipe's or a socket's on the reactor), `read_file`/`write_file`, the file system (`stat`, `mkdir_all`, `read_dir`, `walk_dir`), `path`, the process (`args`, `getenv`, `stdin`/`stdout`/`stderr`), a child process (`command` with the fields of `exec.Cmd`, `posix_spawn`, its exit waited for on the reactor: 1.1 ms a run against Go's 1.9); every operation returns `expected<T, io::error>`, with the code, the operation and the path |
| [time](docs/sgcl/time/README.md) | time | `sgcl::duration` (in core, what the timers take): nanoseconds in 64 bits with Go's text both ways, `"1h30m0.5s"`, the units as methods, arithmetic that saturates rather than wraps, into and from `std::chrono`; `time::date`, a date with no time of day and no zone, carried like Go's `time.Date`, the ISO week, `add_months` cut to the month's end, ISO 8601 read and written; `time::zone`, UTC, a fixed offset, a zone of the system's tz database by name, one from a TZif file or a POSIX TZ string, the local one; `time::datetime`, an instant and the zone it is seen in (Go's `time.Time`), with the calendar's arithmetic across a change of the clock and `time::now()`; the formats known by name (`rfc3339`, `http`, `email`, `iso8601`) and patterns of `%` written and read; `stopwatch` on the library's clock, which a test's manual clock moves. Its own namespace, `sgcl::time` |
| [encoding](docs/sgcl/encoding/README.md) | encoding/base64, base32, hex, ascii85, pem, binary, json, csv, xml | the formats data leaves a program in, each a type in `sgcl::encoding` named as the format is: `base64` (the four alphabets of Go and one's own), `base32`, `hex` with `dump`, `ascii85`, `pem` (RFC 7468 with the headers of RFC 1421), `big_endian`, `little_endian`, `varint`; strict by default, `lenient()` for MIME; the codecs as streams (`encoder_to`, `decoder_from`); one `encoding::error` with the offset, the line and the column; JSON (an immutable value of 24 bytes, a resumable reader of tokens, a writer), CSV as Go reads it, XML with namespaces and no DTD, and a program's own types described once by their fields for all three |
| [net](docs/sgcl/net/README.md) | net, net/netip | `sgcl::net`, the first stage: IP addresses and networks as values of 32 bytes that allocate nothing (`net::ip_address`, `net::ip_network`, `net::endpoint`, text as RFC 5952 writes it), `net::tcp::connect` with happy eyeballs (RFC 8305), `listen`, `accept`, `net::udp`, `net::unix_domain`, `net::dns` through the system's resolver, `net::url` as WHATWG parses it, HTTP/1.1 in `net::http` (a client with a pool and a server with Go 1.22's routes, the framing held against request smuggling); a connection is a handle of one word (`net::connection`), every call that waits in two forms (`c.read(b)` on a thread, `co_await c.async_read(b)` in a task), deadlines absolute as Go's, a `close()` from another task that no read in progress can outlive onto a reused descriptor. TLS and HTTP/2 come next |
| [hash](docs/sgcl/hash/README.md) | hash, hash/crc32, hash/crc64, hash/adler32, hash/fnv, hash/maphash | checksums and hashes that are not cryptographic: every algorithm one type with the same methods (`update`, `value`, `digest`, `reset`, `of`, `copy_from`) — `crc32`, `crc32c`, `crc64`, `crc64_iso`, folded by carry-less multiplication on arm64, `adler32`, the six FNVs, XXH3 (64 and 128 bits) and SipHash-2-4, `maphash` seeded per process; `combine` to hash a buffer in pieces on several tasks; the shape `crypto` will give SHA-2 and the rest. Its own namespace, `sgcl::hash` |

What comes next, in this order and each on the ones before it: `compress`, `crypto` (with TLS 1.3, on `hash` and `math`), the rest of `net` (TLS and HTTP/2, which take their ciphers from `crypto` and gzip from `compress`), the serialization of object graphs in `encoding`, `codec` (the images decoded here, the video through the platform), `db`, `lua` — Lua 5.4 as the embedded scripting language, its own compiler and virtual machine, its values on the managed heap and its coroutines awaiting the library's operations — and `ui`: a reactive state, a view as a function of it, a diff, a flex layout and events on the scheduler, over a small renderer of its own per platform.

## Examples
The pointers and the containers, in one file (`examples/example.cpp` has the long version):

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <memory>
#include <vector>

using namespace sgcl;

struct Node {
    int value;
    vector<tracked_ptr<Node>> edges;   // any graph, cycles included
};

int main() {
    // make_tracked returns a unique_ptr: destroyed at scope exit, deterministically
    unique_ptr unique = make_tracked<int>(42);
    auto also_unique = make_tracked<int>(2);   // the same type, deduced

    // A tracked_ptr hands the object to the collector: destroyed when unreachable
    tracked_ptr tracked = make_tracked<int>(24);
    tracked = std::move(unique);                   // the 42 now belongs to the collector

    // A cycle, collected like anything else
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    a->edges.push_back(b);
    b->edges.push_back(a);
    a = b = nullptr;                               // garbage, no leak

    // Base classes and the dynamic type
    struct Shape { virtual ~Shape() = default; };
    struct Circle : Shape { double r = 1; };
    tracked_ptr<Shape> shape = make_tracked<Circle>();
    if (shape.is<Circle>()) {
        tracked_ptr<Circle> circle = shape.as<Circle>();
        std::cout << "a circle of radius " << circle->r << '\n';
    }
    tracked_ptr<void> any = shape;             // type() still knows: Circle

    // An alias into a member keeps the whole object
    tracked_ptr node = make_tracked<Node>(7);
    tracked_ptr<int> value(&node->value);
    node = nullptr;
    std::cout << *value << '\n';                   // 7, the Node lives on

    // Containers with the interfaces of std, their nodes and buffers managed
    map<std::string, tracked_ptr<Node>> index;
    list numbers = {1, 2, 3};
    vector<tracked_ptr<Node>> nodes(10);

    // A tracked_ptr lives on a stack or inside a managed object; anywhere
    // else (a std container, a global, new memory) the root is a root_ptr
    // ([The pointer and its places](docs/sgcl/core/README.md#the-pointer-and-its-places)).
    std::vector<root_ptr<Node>> kept = {value.as<Node>()};    // fine: a cell roots the Node
    static root_ptr<Node> root = nodes[0];                    // fine: a global
    auto holder = new root_ptr<Node>(nodes[1]);               // fine: a root in new memory
    // std::vector<tracked_ptr<Node>> edges;                  // not allowed: never scanned, the object is lost
    // static tracked_ptr<Node> root;                         // not allowed: a global is neither a stack nor an object
    delete holder;
}
```

Tasks and a channel, the shape of a Go program: a coroutine on the scheduler receives managed objects from a thread and sends results back, waiting on either side without holding a thread, and nobody frees anything ([channel](docs/sgcl/async/channel.md), [coroutine](docs/sgcl/async/coroutine.md)):

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

struct Job {
    int id;
};

async::task<> worker(async::channel<tracked_ptr<Job>>& jobs, async::channel<int>& results) {
    while (auto job = co_await jobs.receive()) {   // suspends while jobs is empty; empty once jobs is closed and drained
        co_await results.send((*job)->id * 2);     // suspends while results is full
    }
    results.close();                                     // the stream ends downstream
}

int main() {
    async::channel<tracked_ptr<Job>> jobs(8);
    async::channel<int> results(8);
    async::task w = async::spawn(worker(jobs, results));   // runs on the pool of workers whenever a job comes
    thread producer([&] {
        for (int i : range(100)) {
            jobs.send(make_tracked<Job>(i)).wait();       // waits when the buffer of eight is full
        }
        jobs.close();
    });
    long sum = 0;
    for (int r : results) {                              // until results is closed
        sum += r;
    }
    producer.join();
    w.wait();
    std::cout << sum << '\n';                            // 9900
}
```

The frame of a coroutine is heap memory too. A `root_ptr` among its parameters, locals or promise is fine in any frame; a `tracked_ptr` is allowed only when the promise derives from `managed_frame`, which `task` and `generator` do, and a managed frame costs nothing at each use of the pointer ([Coroutines](docs/sgcl/async/README.md#coroutines)):

```cpp
std::generator<root_ptr<Node>> chain(int count);       // a plain frame: each root_ptr in it roots its Node through a cell
generator<tracked_ptr<Node>> chain(int count);   // a managed frame: the frame itself is a managed object, no cells
// std::generator<tracked_ptr<Node>> chain(int count); // not allowed: a plain frame is never scanned
```

## The rules, in short
1. A `tracked_ptr`, and every type that holds one (a container, a `string`, a `channel`, a `task`), lives on a stack or inside a managed object, never in unmanaged memory: a global, a `std` container, a lambda copied to the heap, a plain coroutine frame. For those places there is `root_ptr`, which lives anywhere.
2. A `tracked_ptr` never shares its word with data: no `union` with a value, no `std::variant`, `std::any` or `std::function` holding one; `sgcl::variant`, `any`, `function` and `expected` keep the pointers apart, and `optional`, `pair` and `tuple` are safe as they are.
3. A raw pointer to a managed object is not a reference the collector honours: the object lives as long as a `tracked_ptr` or a `unique_ptr` keeps it.

The rules in full, with what each costs and what breaking one looks like: [docs/sgcl/core/README.md](docs/sgcl/core/README.md#the-rules).

## Documentation
[docs/](docs/README.md) is the reference and the guide: a README per module ([core](docs/sgcl/core/README.md), [immutable](docs/sgcl/immutable/README.md), [txt](docs/sgcl/txt/README.md), [concurrent](docs/sgcl/concurrent/README.md), [async](docs/sgcl/async/README.md), [io](docs/sgcl/io/README.md)), a page per class with every member, its signature as declared in the header, the rules that apply and an example that compiles, and the chapter on [the garbage collector](docs/garbage_collector/README.md). [docs/garbage_collector/diagnostics.md](docs/garbage_collector/diagnostics.md) is where to start when the memory grows, an object lives too long or dies too early, or a cycle costs more than it should.

## Dependencies and usage
C++20 and nothing else: no external library, no runtime to link. For LLDB, `command script import <sgcl>/lldb/sgcl.py` (in `~/.lldbinit`) shows the pointers and containers as they are ([docs/diagnostics.md](docs/garbage_collector/diagnostics.md#in-the-debugger)). Copy the `sgcl` directory into your include path and `#include "sgcl/sgcl.h"`, or add this tree with CMake and link the `sgcl` interface target. The library is eleven modules, one directory each and each a header of its own for a program that wants only that much: `sgcl/core/core.h` (the collector, the pointers, the containers, the atomics and the clock: everything in `sgcl::` itself), `sgcl/immutable/immutable.h`, `sgcl/txt/txt.h`, `sgcl/math/math.h`, `sgcl/concurrent/concurrent.h`, `sgcl/async/async.h`, `sgcl/io/io.h`, `sgcl/time/time.h`, `sgcl/encoding/encoding.h`, `sgcl/hash/hash.h` and `sgcl/net/net.h`, each depending only on those before it (`txt` stands third because it needs core and nothing else, not because it was written last, and `math` fourth because it needs no more than `txt`); a directory other than core is a namespace of its own. The tests need googletest in `external/` and build one program per module, core's containers in a program of their own (`tests_core`, `tests_containers`, `tests_immutable`, `tests_concurrent`, `tests_async`, `tests_io`, `tests_txt`, `tests_math`, `tests_time`, `tests_encoding`, `tests_hash`, `tests_net`; `ctest -R async` runs one); the benchmarks build with the tree, and their Go and Java counterparts need only a Go and a JDK to run `benchmarks/compare.sh`.

## Compilers and platforms
Written for clang, gcc and MSVC on macOS, Linux and Windows; the current version has been built and tested on Apple Silicon (macOS, Apple clang) only, the other platforms are pending. On Windows, gcc's handling of thread-local destructors makes it a poor choice; clang and MSVC are fine. On macOS every access to a thread-local variable is a call into the dynamic loader, which is what the registration check in a `tracked_ptr` constructor costs there (about a nanosecond); Linux and Windows read a segment register.

## License
Apache License 2.0, see [LICENSE](LICENSE). Contributions are accepted under the same terms (section 5 of the license), with no separate agreement.
