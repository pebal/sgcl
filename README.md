# SGCL

## What it is
SGCL is a C++20 application platform: one library, header-only, with no dependency beyond the standard library, that means to give a C++ program what Qt gives it and what Go's standard library gives a Go program, from the pointer up to the network and, in time, the screen. The name is from where it started, a Smart Garbage Collection Library, and the collector is still the foundation: every part of the platform is built on objects that live as long as anything reaches them, cycles included, allocated and passed around without reference counts, with a collector that runs concurrently with the program and never stops it. That foundation is what lets the rest be written the way Go and Java write it and C++ could not: lock-free structures with the textbook algorithms and no reclamation scheme, coroutines whose frames are managed objects, channels that threads and tasks share, closures that capture the objects they work on, and a declarative user interface whose view trees are rebuilt rather than patched.

What it keeps from C++ is the rest: deterministic destruction where it is wanted (`unique_ptr`), objects that never move, stack objects and raw pointers as they are, containers with the interfaces of `std`, values and pointers as C++ has them, and no runtime beyond the headers themselves. Everything is in one namespace, `sgcl`, in the standard library's style: `tracked_ptr`, `make_tracked`, `vector`, `channel`, `task`.

## The engine
The collector is a concurrent, non-moving, generational mark-and-sweep with a Dijkstra insertion barrier. The program never stops for it: there is no stop-the-world phase, no safepoint a thread has to reach, no handshake in the hot path, no allocation that waits for a cycle and no write barrier that does more than store a byte. A mutator thread runs at the same speed whether the collector is idle or in the middle of a cycle; the collector and its helpers take cores of their own and the memory that accumulates between two cycles. In numbers, on an Apple M2 Ultra: a pointer copy is 1.4 ns onto the stack and 1.8 ns into an object (`shared_ptr`: 4.7 ns alone, 100–300 ns on a shared object), an allocation 2.8 ns (malloc: 17 ns), and the tails of a mutator's latency are the scheduler's, not the collector's.

- **No reference counts**: a `tracked_ptr` is one word, copied with a store and a byte of state; the objects it points to may form any graph, cycles included.
- **One word, one place**: a `tracked_ptr` lives in managed objects and on stacks, where the collector looks; `root_ptr` is the root for everywhere else, a global or a `std` container, over a cell of a managed block.
- **Deterministic where it matters**: `unique_ptr` destroys its object at scope exit, on the thread that owns it; a `tracked_ptr` hands the destructor to the collector.
- **Precise heap, conservative stacks**: the heap is traced through a map per type of the words that may hold a pointer, which the collector builds by elimination as it goes, so constructors register nothing; the stacks are scanned while the threads keep running.
- **Weak pointers** one word wide, cleared by the cycle that finds the object unreachable; `expiry_queue` hands an object found unreachable to a function of your choice, alive one last time.
- **Dynamic type**: `type()`, `is<U>()` and `as<U>()` on any pointer, including `tracked_ptr<void>`, without virtual functions.
- **Diagnostics**: cycle counters and phase times, live objects and bytes by type, what holds an object and what it retains, LLDB formatters; read without stopping the collector.
- **Memory under control**: a committed-memory ceiling (90% of the cgroup or physical limit by default), a collection forced before it and the program ended with a diagnostic past it, instead of the OOM killer.

The chapter [docs/garbage_collector/](docs/garbage_collector/README.md) has the rest: [the engine in short](docs/garbage_collector/overview.md), [how it works](docs/garbage_collector/how-it-works.md) phase by phase, [next to the alternatives](docs/garbage_collector/alternatives.md) (`shared_ptr`, Go, Java with ZGC, in one table), [the benchmarks](docs/garbage_collector/benchmarks.md) and [the diagnostics](docs/garbage_collector/diagnostics.md).

## Modules
The platform is modules, one directory and one header each, listed alphabetically, core depending on nothing and every other module on core and the modules its README names; `#include "sgcl/sgcl.h"` brings them all in, `#include "sgcl/async.h"` one of them with what it needs. Each has a README that is its guide, what the classes are, the rules, what to reach for, before it lists the classes, and a page per class with every member and an example that compiles. The Go column names the counterpart in Go's standard library, the measure the platform is written against.

| module | Go | what it holds |
|---|---|---|
| [async](docs/sgcl/async/README.md) | goroutines<br>`chan`<br>`context`<br>`time` | coroutines whose frames are managed objects (`task`, `async::generator`), a pool of workers that runs the tasks (`spawn`, `go`, `yield`), `channel` and `select` as Go has them, `sleep`, `after`, `tick`, `timeout`, `stop_token` for cancellation with deadlines, `when_all` and `when_any`, `mutex`, `semaphore`, `event`, `wait_group`, `once` that park a task without a thread, `readable` and `writable` on a file descriptor: the reactor |
| [codec](docs/sgcl/codec/README.md) | `image/png`<br>`image/jpeg`<br>`image/gif` | images decoded and encoded here, from the specifications, on the processor's vector instructions where they pay: PNG both ways (every type and depth, Adam7, filters byte for byte with libpng), JPEG both ways (sequential and progressive decoding bit for bit with libjpeg-turbo, a baseline encoder byte for byte with cjpeg), GIF with its animations (`codec::frames`), WebP decoded (lossless and lossy with alpha and animation, pixel for pixel with libwebp); HEIC and AVIF on macOS through the platform's ImageIO (HEIC both ways); one `codec::load(path)` and `codec::decode(bytes, options)` for every format, `image.save(path)` in the format its extension names; `codec::image` a handle of one word with its pixels in a managed block, nine pixel formats, EXIF and ICC as bytes; RAW (DNG and the cameras of the last ten years) planned |
| [compress](docs/sgcl/compress/README.md) | `compress/flate`<br>`compress/gzip`<br>`compress/zlib`<br>`compress/bzip2`<br>`compress/lzw`<br>`archive/zip`<br>`archive/tar` | DEFLATE both ways (a compressor at zlib's sizes, a decoder within 1.3× of Apple's zlib and four times Go's), with zlib's dictionary and Adler-32 and gzip's header, CRC-32 and members; bzip2 read; LZW both bit orders; zip archives read and written (ZIP64, UTF-8 names, every time field Go reads, the checks against the central directory, a limit before a byte is decompressed) and tar (ustar, pax, GNU's long names); the whole of 7z (LZMA, LZMA2, PPMd, BCJ, BCJ2, Delta, Deflate64, 7zAES) with xz and lzma, held to 7-Zip, xz and bsdtar; every format one type with `compress`/`decompress` in memory and a `reader`/`writer` that are io streams. Its own namespace, `sgcl::compress` |
| [concurrent](docs/sgcl/concurrent/README.md) | `sync` | `concurrent::queue` (Michael–Scott), `concurrent::stack` (Treiber), `concurrent::sorted_map` and `concurrent::sorted_set` (a skip list), `concurrent::map` and `concurrent::set` (a split-ordered list): the textbook algorithms with no reclamation scheme in them, because the collector is one; `copy_on_write` (`atomic` and `atomic_ref`, which they stand on, are core's) |
| [core](docs/sgcl/core/README.md) | `runtime`<br>`container/*`<br>`sync/atomic` | the collector; `tracked_ptr`, `unique_ptr`, `root_ptr`, `weak_ptr`, `make_tracked`; `variant`, `any`, `function`, `expected` that keep the pointers apart from the data; `string`, immutable, one word, shared by copying, with `split`, `join`, `trim`, `replace`, `parse`, and `slice`, a piece of a string (of any contiguous managed storage) that holds the object, Go's slice, a span when the memory is unmanaged; `range`; the mixins (`mixin::enumerable`, `mixin::ordered`, `mixin::lookup`...) that give every container its `contains`, `sort`, `get` and declare what it is, and the requirements (`req::enumerable`, `req::ordered`...) a parameter asks for; the dynamic type; the diagnostics; `config`; the containers: `vector`, `array`, `dynamic_array`, `deque`, `list`, `forward_list`, `stack`, `queue`, the maps and sets, ordered and unordered, with the interfaces of `std` and their nodes and buffers managed; `ordered_map` and `ordered_set` in insertion order (Java's `LinkedHashMap`); `weak_map`, `weak_set`, `expiry_queue`; `atomic<tracked_ptr>` and `atomic_ref` with compare-exchange and no ABA; `clock`, the library's time in one place; `managed_frame` and `frame_ptr`, a coroutine's frame on the managed heap, and `generator` on them. Everything in `sgcl::` itself is here; every other module is a namespace of its own |
| [crypto](docs/sgcl/crypto/README.md) | `crypto/sha256`<br>`crypto/sha512`<br>`crypto/sha3`<br>`crypto/hmac`<br>`crypto/hkdf`<br>`crypto/pbkdf2`<br>`crypto/rand`<br>`crypto/subtle`<br>`crypto/aes`<br>`crypto/cipher`<br>`crypto/ecdh`<br>`crypto/ecdsa`<br>`crypto/ed25519`<br>`crypto/rsa`<br>`x/crypto/chacha20poly1305` | the digests (SHA-1, SHA-2, SHA-3 and SHAKE on the processor's SHA instructions), HMAC, HKDF, PBKDF2, the system's random bytes and comparison in constant time; AES-GCM on the AES and PMULL instructions and ChaCha20-Poly1305 (XChaCha too) on NEON, one interface for both (`seal`, `open`, a tag checked before a byte is decrypted), `nonce_counter`; X25519 and Ed25519, ECDH and ECDSA on P-256 and P-384 with hedged RFC 6979 nonces, and RSA (PKCS #1 v1.5 and PSS signatures, OAEP; blinded, CRT, checked after signing), their keys in PKCS #8 and SPKI as OpenSSL writes them; keys that hold their bytes in themselves, move-only, zeroed by their destructors; everything in constant time on the secrets and in plain C++ where the instructions are not. ML-KEM (FIPS 203) for the three parameter sets, faster than OpenSSL and Go; `crypto::random` as a per-thread ChaCha20 generator with fast key erasure seeded from the system. Written from the specifications and held to OpenSSL and Go, not yet independently audited. Its own namespace, `sgcl::crypto` |
| [encoding](docs/sgcl/encoding/README.md) | `encoding/base64`<br>`encoding/base32`<br>`encoding/hex`<br>`encoding/ascii85`<br>`encoding/pem`<br>`encoding/binary`<br>`encoding/json`<br>`encoding/csv`<br>`encoding/xml` | the formats data leaves a program in, each a type in `sgcl::encoding` named as the format is: `base64` (the four alphabets of Go and one's own), `base32`, `hex` with `dump`, `ascii85`, `pem` (RFC 7468 with the headers of RFC 1421), `big_endian`, `little_endian`, `varint`; strict by default, `lenient()` for MIME; the codecs as streams (`encoder_to`, `decoder_from`); one `encoding::error` with the offset, the line and the column; JSON (an immutable value of 24 bytes, a resumable reader of tokens, a writer), CSV as Go reads it, XML with namespaces and no DTD, and a program's own types described once by their fields for all three |
| [hash](docs/sgcl/hash/README.md) | `hash`<br>`hash/crc32`<br>`hash/crc64`<br>`hash/adler32`<br>`hash/fnv`<br>`hash/maphash` | checksums and hashes that are not cryptographic: every algorithm one type with the same methods (`update`, `value`, `digest`, `reset`, `of`, `copy_from`) — `crc32`, `crc32c`, `crc64`, `crc64_iso`, folded by carry-less multiplication on arm64, `adler32`, the six FNVs, XXH3 (64 and 128 bits) and SipHash-2-4, `maphash` seeded per process; `combine` to hash a buffer in pieces on several tasks; the shape [`crypto`](docs/sgcl/crypto/README.md) gives SHA-2 and the rest. Its own namespace, `sgcl::hash` |
| [immutable](docs/sgcl/immutable/README.md) | — | the immutable containers in `sgcl::immutable` (Clojure, Scala), `immutable::vector`, `immutable::list`, `immutable::map`, `immutable::set`: every operation a new version that shares all but the path it changed with the old one, which stays as it was — the state of a program as a value, compared by its root, kept as its history, read by any thread while another builds the next |
| [io](docs/sgcl/io/README.md) | `os`<br>`io`<br>`bufio`<br>`path/filepath`<br>`os/exec` | streams as requirements of one primitive each with everything else mixed in (`io::req::reader`, `writer`, `seeker`, `closer`, nothing virtual) and `io::reader`/`io::writer`, handles of one word over any of them (`read_all`, `copy`, `write`, each with an `async_` form for a task), `buffered_reader` that hands out lines as views into a managed block, `file` over any descriptor (a regular file's async reads on the blocking pool, a pipe's or a socket's on the reactor), `read_file`/`write_file`, the file system (`stat`, `mkdir_all`, `read_dir`, `walk_dir`), `path`, the process (`args`, `getenv`, `stdin`/`stdout`/`stderr`), a child process (`command` with the fields of `exec.Cmd`, `posix_spawn`, its exit waited for on the reactor: 1.1 ms a run against Go's 1.9); every operation returns `expected<T, io::error>`, with the code, the operation and the path |
| [math](docs/sgcl/math/README.md) | `math/big`<br>`math/rand/v2` | `math::big_integer`, a whole number of any size with the manners of an `int` (Karatsuba, Toom-3, Burnikel–Ziegler, conversions by divide and conquer, number theory: `pow`, `mod_pow`, `gcd`, `mod_inverse`, `sqrt`, `is_probable_prime`, `factorial`, `binomial`) (`a * 2 + 1`, `/` and `%` as C++ divides, `mod` never negative, bits in two's complement, text in bases 2 to 36, the literal `_big`), sixteen bytes and nothing allocated for a value within `int64_t`, a larger one immutable and shared by copying as a `string` is; `math::rational`, a fraction of two `big_integer`s always in lowest terms (Go's `big.Rat`), exact arithmetic that never rounds; `math::random`, ChaCha8Rand, the stream of Go's `ChaCha8` from the same key, with `next_int`, `next_double`, `next_normal`, `shuffle`, `pick`, `permutation` and a generator of the standard's for everything else. Its own namespace, `sgcl::math` |
| [net](docs/sgcl/net/README.md) | `net`<br>`net/netip` | `sgcl::net`, the first stage: IP addresses and networks as values of 32 bytes that allocate nothing (`net::ip_address`, `net::ip_network`, `net::endpoint`, text as RFC 5952 writes it), `net::tcp::connect` with happy eyeballs (RFC 8305), `listen`, `accept`, `net::udp`, `net::unix_domain`, `net::dns` through the system's resolver, `net::url` as WHATWG parses it, HTTP/1.1 in `net::http` (a client with a pool and a server with Go 1.22's routes, the framing held against request smuggling); a connection is a handle of one word (`net::connection`), every call that waits in two forms (`c.read(b)` on a thread, `co_await c.async_read(b)` in a task), deadlines absolute as Go's, a `close()` from another task that no read in progress can outlive onto a reused descriptor; TLS 1.3 in `net::tls` (client and server, X25519MLKEM768 first, the handshake done before the connection is handed out, interop with OpenSSL and Go, RFC 8448 byte for byte, no independent audit), HTTPS in the client and the server, and HTTP/2 over TLS by ALPN and h2c beside HTTP/1.1 on one port with the same handler (every frame, HPACK, flow control, the defences against the known attacks, h2spec; requests per second above Go's net/http in every measured cell) |
| [slog](docs/sgcl/slog/README.md) | `log/slog` | structured logging as Go's `log/slog` — `slog::info("msg", "key", value)`, `logger` (made from `slog::options`: the output, text or JSON, the level, `source`, `utc`, `buffered`, sampling; `with`, `group`), text and JSON byte for byte as slog writes them, pairs checked by the compiler, a type described by its fields as a group, nothing managed per record, a batch per worker; `level_var`, `group`, `handler`, `memory`, `record`, `attr`, `value` |
| [time](docs/sgcl/time/README.md) | `time` | `sgcl::duration` (in core, what the timers take): nanoseconds in 64 bits with Go's text both ways, `"1h30m0.5s"`, the units as methods, arithmetic that saturates rather than wraps, into and from `std::chrono`; `time::date`, a date with no time of day and no zone, carried like Go's `time.Date`, the ISO week, `add_months` cut to the month's end, ISO 8601 read and written; `time::zone`, UTC, a fixed offset, a zone of the system's tz database by name, one from a TZif file or a POSIX TZ string, the local one; `time::datetime`, an instant and the zone it is seen in (Go's `time.Time`), with the calendar's arithmetic across a change of the clock and `time::now()`; the formats known by name (`rfc3339`, `http`, `email`, `iso8601`) and patterns of `%` written and read; `stopwatch` on the library's clock, which a test's manual clock moves. Its own namespace, `sgcl::time` |
| [txt](docs/sgcl/txt/README.md) | `unicode`<br>`x/text` | what a human expects of text and a byte does not give: the properties of a code point (the general category, the script, `is_alpha`, `is_emoji`, the value of a digit), `columns` — the cells a code point and a text take on a terminal — and the boundaries: `graphemes` (what a reader calls a character, which a code point is not), `words`, `sentences`, `line_breaks`, the cursor moves over graphemes, `wrap` and `truncate`; normalization (UAX #15: the four forms as tags, and comparison and hashing that do not care which one a text arrived in); the full case mappings, where a letter may become two ("straße" is "STRASSE"), where a Greek sigma depends on its place in the word and where three languages spell an i their own way; searching, with a prepared pattern or blind to case or to the way a text was written; and the encodings, which is the part whose clearest use is the case where there is no Unicode yet — UTF-16 at the edge of a Windows call, bytes in iso-8859-2 out of an HTTP header. and the bidirectional algorithm, which tells whoever draws a line what order to put it in when the text runs both ways at once; and collation, the order a reader expects rather than the order the bytes fall in, in the root order of the standard or in the own order of 88 languages; and `format`, the pattern of `std::format` read where the program is compiled, whose field over a text is measured in the columns it takes and not in its bytes — which is why it is here and not in core. Its own namespace, `sgcl::txt`; the tables are generated from the UCD, and every algorithm is written from the annex and held to the UCD's own test files |

What comes next, in this order and each on the ones before it: a TLS 1.2 client in `net::tls` beside 1.3 (for the servers that still stop at 1.2; the server stays at 1.3), the serialization of object graphs in `encoding`, `db` (an own document store and the wire protocols of PostgreSQL and Redis, nothing linked), then `audio` (the formats decoded and encoded here — WAV, FLAC, MP3, Opus and Vorbis from their specifications, on the processor's vector instructions — with playback and capture through the platform, as the video is) `ui`: a reactive state, a view as a function of it, a diff, a flex layout and events on the scheduler, over a small renderer of its own per platform. That is version 1.0.0, on macOS, Linux and Windows, with the public API stable from then on until 2.0.0. Version 1.1.0 adds HTTP/3, the chain levels of deflate brought to zlib's speed with vector instructions, the system's roots from the Keychain on macOS (today the bundle in `/etc/ssl/cert.pem`, so a root added by the user or an administrator is not seen), the one-task-per-connection model inside HTTP/2, `codec::raw` (DNG from its specification and the native formats of the cameras of the last ten years, for a raw development pipeline) and `lua`: Lua 5.4 as the embedded scripting language, its own compiler and virtual machine, its values on the managed heap, its coroutines awaiting the library's operations and its bindings over the whole library, the UI included.

## Examples
The pointers and the containers, in one file:

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
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
        println("a circle of radius {}", circle->r);
    }
    tracked_ptr<void> any = shape;             // type() still knows: Circle

    // An alias into a member keeps the whole object
    tracked_ptr node = make_tracked<Node>(7);
    tracked_ptr<int> value(&node->value);
    node = nullptr;
    println("{}", *value);                   // the Node lives on

    // Containers with the interfaces of std, their nodes and buffers managed
    map<std::string, tracked_ptr<Node>> index;
    list numbers = {1, 2, 3};
    vector<tracked_ptr<Node>> nodes(10);

    // A tracked_ptr lives on a stack or inside a managed object; anywhere
    // else (a std container, a global, new memory) the root is a root_ptr
    // ([The rules](docs/sgcl/core/README.md#the-rules)).
    std::vector<root_ptr<Node>> kept = {value.as<Node>()};    // fine: a cell roots the Node
    static root_ptr<Node> root = nodes[0];                    // fine: a global
    auto holder = new root_ptr<Node>(nodes[1]);               // fine: a root in new memory
    // std::vector<tracked_ptr<Node>> edges;                  // not allowed: never scanned, the object is lost
    // static tracked_ptr<Node> root;                         // not allowed: a global is neither a stack nor an object
    delete holder;
}
```

Output:

```text
a circle of radius 1
7
```

Tasks and a channel, the shape of a Go program: a coroutine on the scheduler receives managed objects from a thread and sends results back, waiting on either side without holding a thread, and nobody frees anything ([channel](docs/sgcl/async/channel/README.md), [task](docs/sgcl/async/task/README.md)):

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Job {
    int id;
};

async::task<> worker(async::channel<tracked_ptr<Job>> jobs, async::channel<int> results) {
    while (auto job = co_await jobs.receive()) {   // suspends while jobs is empty; empty once jobs is closed and drained
        co_await results.send((*job)->id * 2);     // suspends while results is full
    }
    results.close();                                     // the stream ends downstream
}

int main() {
    async::channel<tracked_ptr<Job>> jobs(8);
    async::channel<int> results(8);
    async::task w = async::spawn(worker(jobs, results));                 // runs on the pool of workers whenever a job comes
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
    println("{}", sum);
}
```

Output:

```text
9900
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
[docs/](docs/README.md) is the reference and the guide: a README per module ([the modules](docs/sgcl/README.md)), a page per class and a page per public method with its signature as declared in the header, the rules that apply and a program that compiles, and the chapter on [the garbage collector](docs/garbage_collector/README.md). [docs/garbage_collector/diagnostics.md](docs/garbage_collector/diagnostics.md) is where to start when the memory grows, an object lives too long or dies too early, or a cycle costs more than it should.

### Reading the code on the pages

A complete program on the pages opens with `using namespace sgcl;` after its includes and has no other directive: every other module's name is written with its module, `encoding::hex::encode(crypto::sha256::of("abc"))`, `async::task<> worker(async::channel<int> jobs)`, `net::http::server`, `io::open`, `txt::format`, `concurrent::queue`, as Go writes `hex.EncodeToString(sha256.Sum256(b))`. The module says where a name comes from, examples are copied into programs where modules meet, and one rule needs no list of exceptions. It also settles the names two modules share (`async::sleep` beside `sgcl::sleep` and the C library's, `crypto::random` beside `math::random`, `hash::mixin` beside `sgcl::mixin`) and io's `remove`, `rename`, `getenv`, `chdir` and `symlink`, functions of the C library too, which a bare call with a string literal would choose without a word; `println` is in `sgcl`, so a program that only prints names no io at all. The one directive kept is for literals, which cannot be written qualified: `using namespace math::literals;`, as `std::chrono_literals`. One collision the directive brings: a bare `time(nullptr)` of the C library is ambiguous with the namespace `sgcl::time` under `using namespace sgcl;` — write `std::time(nullptr)`. The examples use C++20 class template argument deduction (`tracked_ptr p = make_tracked<T>();`) and every `force_collect()` in them is optional, there to show the result at once.

A variable is not named after its type (`crypto::aes cipher(key)`, `net::http::server srv`, not `aes aes(key)`), and a value is named by its type where `auto` would need a `*`: `vector<byte> key = encoding::hex::decode("…")`, never `auto key = *encoding::hex::decode("…")`; a type that has a constructor from text takes the literal in it (`net::ip_address a("10.0.0.1")`), a type whose constructor from text would mean something else — json's, which makes a string value — takes the literal by its name and the conversion (`encoding::json doc = encoding::json::parse(R"({"a": 1})");`, never `.value()` or `*`), and `parse` is kept for text that comes from outside, an argument, a file, the network. The synopsis blocks stand inside the header's namespace as the headers do.

A call that may wait comes in two ways. In io, net, encoding and hash, which are synchronous as their counterparts in POSIX, `std` and Go are, the name does its work on the calling thread and returns the result (`f.read(b)`, `io::copy(w, r)`), and `async_` is the same for a task, which gives the worker back while it waits (`co_await f.async_read(b)`). In async, whose calls exist to wait in a task, an operation has one name and is carried out by `co_await ch.receive()` in a task or `ch.receive().wait()` on a thread; the name alone makes a description of the operation ([async::operation](docs/sgcl/async/README.md#waiting-operations), nodiscard) and does nothing. The Lockable members the standard names — a mutex's `lock()`, `try_lock()`, `unlock()` and their shared forms — stay blocking, for `std::lock_guard` and `std::shared_lock`; a task takes a mutex with `co_await m.scoped_lock()`.

## Dependencies and usage
C++20 and nothing else: no external library, no runtime to link. For LLDB, `command script import <sgcl>/lldb/sgcl.py` (in `~/.lldbinit`) shows the pointers and containers as they are ([docs/diagnostics.md](docs/garbage_collector/diagnostics.md#in-the-debugger)). Copy the `sgcl` directory into your include path and `#include "sgcl/sgcl.h"`, or add this tree with CMake and link the `sgcl` interface target. In a Release build the target also hides the symbols of the program that links it and drops their table, which takes a program of the library's size down by about half (`SGCL_HIDDEN_VISIBILITY`, on by default; turn it off for a shared library with an API of its own or a process with more than one image using sgcl: [config](docs/sgcl/core/config.md#release-builds-and-symbols)). The library is fifteen modules, one directory each and each a header of its own for a program that wants only that much: `sgcl/async.h`, `sgcl/codec.h`, `sgcl/compress.h`, `sgcl/concurrent.h`, `sgcl/core.h` (the collector, the pointers, the containers, the atomics and the clock: everything in `sgcl::` itself), `sgcl/crypto.h`, `sgcl/encoding.h`, `sgcl/hash.h`, `sgcl/immutable.h`, `sgcl/io.h`, `sgcl/math.h`, `sgcl/net.h`, `sgcl/slog.h`, `sgcl/time.h` and `sgcl/txt.h`; core depends on nothing, every other module on core and the modules its README names; a directory other than core is a namespace of its own. The tests need googletest in `external/` and build one program per module, core's containers in a program of their own (`tests_core`, `tests_containers`, `tests_immutable`, `tests_concurrent`, `tests_async`, `tests_io`, `tests_txt`, `tests_math`, `tests_time`, `tests_encoding`, `tests_hash`, `tests_compress`, `tests_crypto`, `tests_net`, `tests_http`, `tests_slog`, `tests_codec`, and `tests_hash_portable`, `tests_crypto_portable` and `tests_codec_portable`, the same vectors on the plain C++ road; `ctest -R async` runs one; the compress tests link zlib and libbz2 and the crypto tests OpenSSL 3, as their oracles, and `tests_crypto` is not built where OpenSSL is not found); the benchmarks build with the tree, and their Go and Java counterparts need only a Go and a JDK to run `benchmarks/compare.sh`.

## Compilers and platforms
Written for clang, gcc and MSVC on macOS, Linux and Windows; the current version has been built and tested on Apple Silicon (macOS, Apple clang) only, the other platforms are pending. On Windows, gcc's handling of thread-local destructors makes it a poor choice; clang and MSVC are fine. On macOS every access to a thread-local variable is a call into the dynamic loader, which is what the registration check in a `tracked_ptr` constructor costs there (about a nanosecond); Linux and Windows read a segment register.

## License
Apache License 2.0, see [LICENSE](LICENSE). Contributions are accepted under the same terms (section 5 of the license), with no separate agreement.
