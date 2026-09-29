# The sgcl reference

One page per public class or function of the library, each with every public member, its signature as declared in the header, and an example that compiles. The guide, the rules and the benchmarks are in the [main README](../../README.md); this is the reference to come back to.

Every program with `main` on a page is checked with what the page says it prints: `tools/run_blocks.py docs/sgcl/<module>/<page>.md` compiles and runs them (a server's program answers one request; a program that uses the network is given local servers in its place — `--serve URL=FILE`, `--echo URL`, `--tls HOST:PORT` — listed for the page in `tools/run_blocks.pages`, so every page is checked with no arguments and no network).

## Modules

The library is fifteen modules, one directory each in `sgcl/` and here, each depending only on those before it and each with a README of its own that is its guide (what the classes are, the rules, what to reach for) before it lists them:

| module | header | what it holds |
|---|---|---|
| [core](core/README.md) | `sgcl/core/core.h` | the collector, `tracked_ptr`, `unique_ptr`, `root_ptr`, `weak_ptr`, `make_tracked`, `variant`, `any`, `function`, `expected`, `range`, the configuration; the containers (the sequences, maps and sets, `weak_map`, `weak_set`, `expiry_queue`), `atomic`, `atomic_ref`, `clock`, the managed coroutine frame and `generator`: everything in `sgcl::` itself |
| [immutable](immutable/README.md) | `sgcl/immutable/immutable.h` | the immutable containers in `sgcl::immutable` (`immutable::vector`, `immutable::list`, `immutable::map`, `immutable::set`: every operation a new version sharing all but the path it changed; the state of a program as a value) |
| [txt](txt/README.md) | `sgcl/txt/txt.h` | the properties of a code point (the general category, the script, `is_alpha`, `is_emoji`, `columns`) the boundaries of UAX #29 and #14 (`graphemes`, `words`, `sentences`, `line_breaks`, `wrap`) the four normalization forms of UAX #15, the full case mappings with `locale`, searching blind to case or to the way a text was written, the encodings (UTF-16 and UTF-32, the single byte pages, what a header's charset says) and the bidirectional algorithm; collation (`collator`, UTS #10 with the CLDR tailorings), `format` with the pattern of `std::format`, identifiers, IDNA, percent-encoding, regular expressions and `stencil` templates; its own namespace, `sgcl::txt` |
| [math](math/README.md) | `sgcl/math/math.h` | `big_integer`, a whole number of any size with the manners of an `int`, sixteen bytes, a value within `int64_t` allocating nothing, fast multiplication, division and conversions, number theory; `rational`, exact fractions; `random`, ChaCha8Rand with Go's stream from the same key, `next_int`, `next_double`, `shuffle`, `pick`, `permutation`; its own namespace, `sgcl::math` |
| [concurrent](concurrent/README.md) | `sgcl/concurrent/concurrent.h` | the lock-free containers, `concurrent::bounded_queue`, `concurrent::priority_queue`, `concurrent::cache`, the concurrent weak containers, `intern`, `copy_on_write` |
| [async](async/README.md) | `sgcl/async/async.h` | coroutines, the scheduler, executors and strands, task-local values, `channel`, `select`, `broadcast`, timers and the clock, the signals of the process, `stop_token`, `when_all`/`when_any`, `task_group`, `timeout`, `mutex`, `shared_mutex`, `semaphore`, `event`, `wait_group`, `once`, `condition_variable`, `promise`, `spawn_blocking`, the reactor |
| [io](io/README.md) | `sgcl/io/io.h` | streams (whatever has `read` or `write`, a lambda too, checked by the concepts of `io::req`; `io::reader` and `io::writer` to hold any of them; the mixins over the primitive; `io::stdin`, `io::stdout`, `io::stderr` as objects), `buffered_reader` and `buffered_writer`, `file` over any descriptor with the pool and the reactor behind its async forms, the file system (`stat`, `mkdir_all`, `read_dir`, `walk_dir`), `path`, the process (`args`, `getenv`, the standard streams), a child process (`command`: the fields of `exec.Cmd`, its exit waited for on the reactor), files mapped into memory (`map`) and named shared memory (`shared_memory`); errors as `expected<T, io::error>`; its own namespace, `sgcl::io` |
| [time](time/README.md) | `sgcl/time/time.h` | dates, the zones of the system's tz database (TZif v1–v4, POSIX TZ), instants in a zone (`datetime`, Go's `time.Time`; `time::now()` follows a test's manual clock), the time elapsed, and their text: RFC 3339, HTTP, e-mail, ISO 8601 and `%` patterns both ways, `txt::format` of time and of `<chrono>` |
| [encoding](encoding/README.md) | `sgcl/encoding/encoding.h` | `sgcl::encoding`: a type per format — `base64`, `base32`, `hex` (with `dump`), `ascii85`, `pem`, `big_endian`, `little_endian`, `varint`; strict by default, `lenient()`; the codecs as `io` streams; `json` (an immutable value, `json::reader`, `json::writer`), `csv` (`reader`, `row`, `writer`), `xml` (reader, writer, an immutable tree, namespaces, no DTD), `field_list` (a program's types described once for every format: `describe(field_list&)`); `encoding::error` with the offset, line and column |
| [slog](slog/README.md) | `sgcl/slog/slog.h` | `sgcl::slog`: structured logging as Go's `log/slog` — `slog::info("msg", "key", value)`, `logger` (made from `slog::options`: the output, text or JSON, the level, `source`, `utc`, `buffered`, sampling; `with`, `group`), text and JSON byte for byte as slog writes them, pairs checked by the compiler, a type described by its fields as a group, nothing managed per record, a batch per worker; `level_var`, `group`, `handler`, `memory`, `record`, `attr`, `value` |
| [hash](hash/README.md) | `sgcl/hash/hash.h` | checksums and hashes that are not cryptographic, one type each with the same methods (`update`, `value`, `digest`, `reset`, `of`, `copy_from`, `of_file`): `crc32`, `crc32c`, `crc64`, `crc64_iso`, `adler32`, the six FNVs, `combine` for the CRCs and Adler-32; `xxh3_64` and `xxh3_128` (XXH3 with a seed, NEON on arm64), `maphash` (seeded once per process, Go's hash/maphash), `siphash` (SipHash-2-4, a mandatory key); `mixin::hasher` and `req::hasher`, which `crypto`'s digests implement; its own namespace, `sgcl::hash` |
| [crypto](crypto/README.md) | `sgcl/crypto/crypto.h` | `sgcl::crypto`: the digests (`sha1`, `sha256`, `sha512`, `sha3`, `shake`, `hash_id`), `hmac`, `hkdf`, `pbkdf2`, `random`, `constant_time`, `secure_zero`; the AEADs `aes_gcm`, `chacha20_poly1305`, `xchacha20_poly1305` and `nonce_counter`, the bare `aes`, `aes_ctr`, `chacha20`; `x25519`, `ed25519`, `p256` and `p384` (ECDH, ECDSA), `rsa` (PKCS #1 v1.5 and PSS signatures, OAEP), `x509` (certificates, pools, the verification of a chain with the host name, the reason when it fails), `secret`; on the processor's crypto instructions on arm64, plain C++ elsewhere; `crypto::error`. Not independently audited |
| [compress](compress/README.md) | `sgcl/compress/compress.h` | `sgcl::compress`: `flate`, `zlib`, `gzip` (both ways; `compress`/`decompress` in memory, `reader`/`writer` as io streams, `level`, `limits`), `bzip2` (reading), `lzw`, `lzma` (`.lzma` both ways), `xz` (`.xz` both ways: LZMA2, the branch converters, Delta, the four checks, several streams), `zip` (`archive`, `writer`, `entry`, ZIP64), `tar` (`reader`, `writer`, `entry`: ustar, pax, GNU), `sevenzip` (`archive`, `writer`, `entry`, `walk`: 7z both ways, 7zAES, Deflate64 read); `compress::error` |
| [net](net/README.md) | `sgcl/net/net.h` | `sgcl::net`: `ip_address`, `ip_network`, `endpoint` as values; `connection`, `listener`, `udp::socket`; `tcp`, `udp`, `unix_domain`, `dns`; `url` (`net::url`, `net::query_params`, WHATWG); HTTP/1.1 in `net::http` (`client`, `server` with Go 1.22 routes); every call that waits in two forms (`x` on a thread, `async_x` in a task), absolute deadlines, happy eyeballs |
| [net http](net/http/README.md) | `sgcl/net/http/http.h` | HTTP/1.1 and HTTP/2: `net::http::client` (`get`, `post`, `send`, `download`), `server` (Go 1.22 routes), `request`, `response`, `response_writer`, `headers`, `cookie`, `status` |
| [codec](codec/README.md) | `sgcl/codec/codec.h` | `sgcl::codec`: images — `codec::image` (a one-word handle, nine pixel formats, `convert`, orientation, EXIF and ICC as bytes), `codec::load` and `codec::decode` by signature, `image.save`/`codec::save` by extension, `png` and `jpeg` (decode and encode), `gif` (decode, `frames` for animation), `webp` (lossless and lossy, alpha, animation), `heif` (HEIC, HEIF and AVIF through the system's codec, HEIC written), `codec::error`, `limits` |

The sections below list the same pages by what they are.

## Pointers

| page | header | what it is |
|---|---|---|
| [tracked_ptr](core/tracked_ptr.md) | `sgcl/core/tracked_ptr.h` | the pointer the collector follows: one word, a write barrier, no count; aliases, `type()`, `is<U>()`, `as<U>()`, `if_alive()`; `shade()` and the store with `barrier::off` for the copies of immutable nodes |
| [unique_ptr](core/unique_ptr.md) | `sgcl/core/unique_ptr.h` | what `make_tracked` returns: a `std::unique_ptr` to a managed object, deterministic until moved into a `tracked_ptr` |
| [make_tracked](core/make_tracked.md) | `sgcl/core/make_tracked.h` | creates an object on the managed heap |
| [root_ptr](core/root_ptr.md) | `sgcl/core/root_ptr.h` | a root that lives anywhere (a global, a `std` container, a lambda on the heap): a cell of a managed block under it, the `tracked_ptr` it holds its object by one step away; the pointer of an interpreter's handle table or a program's globals |
| [rooted](core/rooted.md) | `sgcl/core/rooted.h` | a value with tracked pointers inside kept in a managed object of its own under a root: what an exception object, a `std` container, a global or a platform's closure holds instead of the value |
| [weak_ptr](core/weak_ptr.md) | `sgcl/core/weak_ptr.h` | a pointer that does not keep its object alive, cleared by the cycle that finds the object unreachable |
| [weak_map, weak_multimap](core/weak_map.md) | `sgcl/core/weak_map.h` | values attached to objects the map does not keep alive: keyed by the object, an entry dies with it |
| [weak_set](core/weak_set.md) | `sgcl/core/weak_set.h` | a set of objects it does not keep alive |
| [variant](core/variant.md) | `sgcl/core/variant.h` | `std::variant`'s interface with the tracked pointers in a word of their own, apart from the data of the other alternatives |
| [any](core/any.md) | `sgcl/core/any.h` | `std::any`'s interface with a tracked pointer in a word of its own and an object with pointers in a managed node of its own |
| [function](core/function.md) | `sgcl/core/function.h` | `std::function` and `std::move_only_function` whose closure may capture tracked pointers: the closure in a managed node of its own |
| [thread](core/thread.md) | `sgcl/core/thread.h` | `std::thread` with the callable and the arguments in a managed node of their own: a `tracked_ptr` captured by value as in a `function`; `sgcl::this_thread` is `std::this_thread` |
| [utf8, unicode, runes](core/utf8.md) | `sgcl/core/utf8.h`, `sgcl/core/unicode.h` | the encoding (`decode`, `encode`, `count`, `valid`), the code point's case and white space (`to_lower`, `is_space`, `equal_fold`), the code points of a text as a range |
| [txt::properties](txt/properties.md) | `sgcl/txt/properties.h` | the general category and the predicates over it, the value of a digit, the script, and `columns`: the cells a code point and a text take on a terminal |
| [txt::segment](txt/segment.md) | `sgcl/txt/segment.h` | the boundaries of UAX #29 and #14: `graphemes`, `words`, `word_breaks`, `sentences`, `line_breaks`, the cursor moves over graphemes, `wrap` and `truncate` |
| [txt::normalize](txt/normalize.md) | `sgcl/txt/normalize.h` | the four forms of UAX #15 as tags (`nfc`, `nfd`, `nfkc`, `nfkd`), `is_normalized`, `equal_normalized`, `compare_normalized`, `hash_normalized`, `combining_class_of`, `compose`, `decompose` |
| [txt::case](txt/case.md) | `sgcl/txt/case.h` | the full case mappings and `locale`: `to_lower_full`, `to_upper_full`, `to_title`, `fold_case`, `equal_fold_full` — a letter that becomes two, the sigma that ends a word, the three languages that spell an i differently |
| [txt::search](txt/search.md) | `sgcl/txt/search.h` | `searcher` (a prepared pattern, Boyer–Moore–Horspool), `find_fold`, `find_normalized`, both reporting the position in the original text |
| [txt::bidi](txt/bidi.md) | `sgcl/txt/bidi.h` | text that runs both ways at once (UAX #9): `bidi_class_of`, `paragraph_direction`, `levels`, `visual_order`, `bidi_runs` — the pieces in the order they are drawn |
| [txt::encoding](txt/encoding.md) | `sgcl/txt/encoding.h` | `decode`/`encode` between UTF-8 and UTF-16, UTF-32, and the 27 single byte encodings of the WHATWG's list (ISO-8859-2 to -16, KOI8-R and -U, windows-874 and -1250 to -1258, IBM866, Mac OS Roman and Cyrillic); `to_utf16`, `from_utf16`, `to_utf32`, `from_utf32`, `detect_bom`, `encoding_from_name` |
| [txt::collate](txt/collate.md) | `sgcl/txt/collate.h` | the order a reader expects (UTS #10): `collator` — a comparator and a key, three strengths, the root order of the DUCET and the own order of 88 languages from CLDR |
| [math::big_integer](math/big_integer.md) | `sgcl/math/big_integer.h` | a whole number of any size: the operators of an `int`, `mod`, `div_rem`, two's complement bits, text in bases 2 to 36, bytes, `to_double`, the literal `_big`, `txt::format`, `std::hash` |
| [math::rational](math/rational.md) | `sgcl/math/rational.h` | an exact fraction of two `big_integer`s in lowest terms: the operators, `to_double` rounded once, `to_decimal`, `parse` of fractions and decimals |
| [math::random](math/random.md) | `sgcl/math/random.h` | ChaCha8Rand, Go's `ChaCha8` stream: `next_int`, `next_uint64`, `next_double`, `next_bool`, `next_normal`, `next_exponential`, `next_bytes`, `shuffle`, `pick`, `permutation`; a uniform random bit generator of the standard's |
| [encoding::error](encoding/error.md) | `sgcl/encoding/error.h` | the one error of the module: `code()`, `offset()`, `line()`, `column()`, `path()`, `message()` |
| [encoding::base64](encoding/base64.md) | `sgcl/encoding/base64.h` | RFC 4648: `standard`, `url`, `raw_standard`, `raw_url`, an alphabet of one's own, `encode`, `decode`, `encode_to`, `decode_to`, `encoder_to`, `decoder_from` |
| [encoding::base32](encoding/base32.md) | `sgcl/encoding/base32.h` | RFC 4648: `standard`, `hex`, the same members as base64 |
| [encoding::hex](encoding/hex.md) | `sgcl/encoding/hex.h` | `encode`, `encode_upper`, `decode`, `dump` and `dumper_to` as `hexdump -C` writes |
| [encoding::ascii85](encoding/ascii85.md) | `sgcl/encoding/ascii85.h` | the btoa form Go writes, `z` for four zeros |
| [encoding::pem](encoding/pem.md) | `sgcl/encoding/pem.h` | RFC 7468 blocks with the headers of RFC 1421: `parse`, `parse_all`, `to_string` |
| [encoding::big_endian, little_endian, varint](encoding/binary.md) | `sgcl/encoding/binary.h` | numbers as bytes: `read_u32`, `write_u64`, `append_u16`...; LEB128 and zigzag as Go writes them |
| [encoding::json](encoding/json.md) | `sgcl/encoding/json.h` | RFC 8259: an immutable value of 24 bytes, `parse`, `to_string`, access, `set`, `builder`, JSON Pointer; numbers exact or rounded once, written as ECMAScript writes them; `parse<T>` and `stringify` of a described type |
| [encoding::json::reader](encoding/json-reader.md) | `sgcl/encoding/json.h` | the tokens of a stream, resumable, `read()` of a value whole, `read<T>`, `skip`; `max_depth`, `max_token_size` |
| [encoding::json::writer](encoding/json-writer.md) | `sgcl/encoding/json.h` | tokens and values to a stream, the structure checked, compact or indented |
| [encoding::field_list](encoding/fields.md) | `sgcl/encoding/fields.h` | a program's type described by its fields once, read and written by JSON, CSV and XML: names, `optional`, `omit_empty`, `required`, `tagged` variants, `to_text`/`from_text` |
| [encoding::csv](encoding/csv.md) | `sgcl/encoding/csv.h` | RFC 4180 as Go reads it: `reader` (resumable, positions in code points, `max_record_size`), `row`, the header, `read<T>`, `writer` |
| [encoding::xml](encoding/xml.md) | `sgcl/encoding/xml.h` | XML 1.0 with namespaces, no DTD: an immutable tree, `parse`, `to_string`, `parse<T>` and `stringify` with `attribute()` and `text()` |
| [encoding::xml::reader](encoding/xml-reader.md) | `sgcl/encoding/xml.h` | the tokens of a stream, resumable, `skip`, `peek`, `read<T>`; UTF-16 and single-byte encodings built in; `max_depth`, `max_token_size` |
| [encoding::xml::writer](encoding/xml-writer.md) | `sgcl/encoding/xml.h` | elements, attributes and text to a stream, escaped, indented on request |
| [net::ip_address, ip_network, endpoint](net/ip.md) | `sgcl/net/ip.h` | addresses as values: RFC 4291 parsing, RFC 5952 text, predicates, prefixes, no allocation |
| [net::connection, listener, udp::socket](net/connection.md) | `sgcl/net/connection.h` | the handles of a connection, a listener and a UDP socket: read, write, read_line, copy_to, accept, deadlines, close |
| [net::tcp, udp, unix_domain](net/socket.md) | `sgcl/net/socket.h` | the factories: connect with happy eyeballs, listen, bind, reuse_port |
| [net::dns](net/dns.md) | `sgcl/net/dns.h` | lookup and reverse_lookup through the system's resolver, on the blocking pool, stopped by a token |
| [net::errc](net/error.md) | `sgcl/net/error.h` | the codes of the module and of getaddrinfo |
| [hash::hasher](hash/hasher.md) | `sgcl/hash/mixin/hasher.h` | `mixin::hasher<D>`: the text overloads of `update`, `of`, `copy_from`, `copy_from`; `req::hasher`, what `crypto`'s digests implement |
| [hash::crc32](hash/crc32.md) | `sgcl/hash/crc32.h` | `crc32` (zlib, gzip, zip, PNG; Go's `crc32.IEEE`) and `crc32c` (Castagnoli), `combine` |
| [hash::crc64](hash/crc64.md) | `sgcl/hash/crc64.h` | `crc64` (CRC-64/XZ, Go's `crc64.ECMA`) and `crc64_iso`, `combine` |
| [hash::adler32](hash/adler32.md) | `sgcl/hash/adler32.h` | Adler-32 of RFC 1950, `combine` |
| [hash::fnv](hash/fnv.md) | `sgcl/hash/fnv.h` | `fnv32`, `fnv32a`, `fnv64`, `fnv64a`, `fnv128`, `fnv128a` |
| [hash::xxh3_64, xxh3_128](hash/xxh3.md) | `sgcl/hash/xxh3.h` | XXH3 with a seed, 64 and 128 bits: values fixed across versions and machines |
| [hash::maphash](hash/maphash.md) | `sgcl/hash/maphash.h` | the hash of a table in the process, seeded once per process |
| [hash::siphash](hash/siphash.md) | `sgcl/hash/siphash.h` | SipHash-2-4, a keyed hash for keys from an adversary |
| [crypto](crypto/README.md) | `sgcl/crypto/crypto.h` | the module: what is here, errors, secrets in memory, nonces, passwords, constant time, the two roads (instructions and plain C++) |
| [crypto::error](crypto/error.md) | `sgcl/crypto/error.h` | the one error of the module: `code()`, `offset()`, `message()` |
| [crypto::sha1](crypto/sha1.md) | `sgcl/crypto/sha1.h` | SHA-1 (FIPS 180-4), for what still needs it |
| [crypto::sha224, sha256](crypto/sha256.md) | `sgcl/crypto/sha256.h` | SHA-224 and SHA-256, on the SHA-256 instructions of arm64 |
| [crypto::sha384, sha512, sha512_256](crypto/sha512.md) | `sgcl/crypto/sha512.h` | the SHA-512 family, on the SHA-512 instructions of ARMv8.2 |
| [crypto::sha3, shake](crypto/sha3.md) | `sgcl/crypto/sha3.h` | SHA3-224…512, SHAKE128 and SHAKE256 (FIPS 202), on the SHA-3 instructions |
| [crypto::hash_id](crypto/hash_id.md) | `sgcl/crypto/hash_id.h` | a digest named by data: `digest(id, data)`, `digest_size`, `block_size` |
| [crypto::hmac](crypto/hmac.md) | `sgcl/crypto/hmac.h` | `hmac<H>` (RFC 2104), `hmac_sha256` and the rest, `verify` in constant time |
| [crypto::hkdf](crypto/hkdf.md) | `sgcl/crypto/hkdf.h` | `extract`, `expand`, `derive` (RFC 5869), the PRK as a secret |
| [crypto::pbkdf2](crypto/pbkdf2.md) | `sgcl/crypto/pbkdf2.h` | keys from a password (RFC 8018) |
| [crypto::random](crypto/random.md) | `sgcl/crypto/random.h` | `fill`, `bytes`: the system's generator |
| [crypto::constant_time](crypto/constant_time.md) | `sgcl/crypto/constant_time.h` | `equal`, whose time does not depend on the bytes |
| [crypto::secure_zero](crypto/secure_zero.md) | `sgcl/crypto/secure_zero.h` | zeros the compiler cannot drop |
| [crypto::aead](crypto/aead.md) | `sgcl/crypto/mixin/aead.h` | `seal`, `open`, `seal_to`, `open_to`: the interface of the AEADs |
| [crypto::aes_gcm](crypto/aes_gcm.md) | `sgcl/crypto/gcm.h` | AES-GCM (SP 800-38D), AES-128/192/256 |
| [crypto::chacha20_poly1305, xchacha20_poly1305](crypto/chacha20_poly1305.md) | `sgcl/crypto/chacha20_poly1305.h` | RFC 8439 and XChaCha20, `seal_random`/`open_random` |
| [crypto::nonce_counter](crypto/nonce_counter.md) | `sgcl/crypto/nonce_counter.h` | nonces that never repeat under a key |
| [crypto::aes](crypto/aes.md) | `sgcl/crypto/aes.h` | the block cipher alone |
| [crypto::aes_ctr](crypto/aes_ctr.md) | `sgcl/crypto/ctr.h` | AES in counter mode, unauthenticated |
| [crypto::chacha20](crypto/chacha20.md) | `sgcl/crypto/chacha20.h` | the stream cipher alone, XChaCha20 with a 24-byte nonce |
| [crypto::x25519](crypto/x25519.md) | `sgcl/crypto/x25519.h` | ECDH on Curve25519 (RFC 7748), PKCS #8 and SPKI |
| [crypto::ed25519](crypto/ed25519.md) | `sgcl/crypto/ed25519.h` | signatures (RFC 8032), PKCS #8 and SPKI |
| [crypto::p256, p384](crypto/p256.md) | `sgcl/crypto/p256.h`, `p384.h` | the NIST curves: `private_key` (ECDSA), `ecdh_key`, `public_key`; SEC 1, PKCS #8, SPKI ([p384](crypto/p384.md)) |
| [crypto::ecdsa](crypto/ecdsa.md) | `sgcl/crypto/ecdsa.h` | signing and verifying on the curves: hedged RFC 6979 nonces, DER and raw signatures |
| [crypto::rsa](crypto/rsa.md) | `sgcl/crypto/rsa.h` | RSA (RFC 8017): `private_key`, `public_key`; PKCS #1 v1.5 and PSS signatures, OAEP; PKCS #1, PKCS #8, SPKI |
| [crypto::x509](crypto/x509.md) | `sgcl/crypto/x509.h` | X.509 (RFC 5280): `certificate` (DER, PEM, names, key, extensions), `certificate_pool` (`system()`), `verify` to a root with RFC 6125 host names and IP addresses as bytes, `reason` of a failure; no CRL, no OCSP |
| [crypto::secret](crypto/secret.md) | `sgcl/crypto/secret.h` | `secret<N>`: a shared secret or a scalar that zeroes itself |
| [compress](compress/README.md) | `sgcl/compress/compress.h` | the module: one shape for every format (`compress`/`decompress` in memory, `reader`/`writer` as io streams), `level`, `limits` |
| [compress::error](compress/error.md) | `sgcl/compress/error.h` | the one error of the module: `code()`, `offset()`, `io_error()`, `message()` |
| [compress::flate](compress/flate.md) | `sgcl/compress/flate.h` | DEFLATE (RFC 1951) both ways, a dictionary, `flush` |
| [compress::zlib](compress/zlib.md) | `sgcl/compress/zlib.h` | RFC 1950: Adler-32, the preset dictionary named by `dictionary_id` |
| [compress::gzip](compress/gzip.md) | `sgcl/compress/gzip.h` | RFC 1952: the header (ISO 8859-1 strings), several members, `single_member` |
| [compress::bzip2](compress/bzip2.md) | `sgcl/compress/bzip2.h` | bzip2 1.0 read, several streams |
| [compress::lzma](compress/lzma.md) | `sgcl/compress/lzma.h` | LZMA alone (`.lzma`) both ways: xz's levels 0..9 and `-e`, lc/lp/pb, the dictionary held against `limits.max_memory` |
| [compress::xz](compress/xz.md) | `sgcl/compress/xz.h` | xz both ways: LZMA2, `check` (none, CRC-32, CRC-64, SHA-256), `filter` (x86, ARM, ARM-Thumb, ARM64, PowerPC, SPARC, IA-64, RISC-V) and Delta, several blocks and streams |
| [compress::lzw](compress/lzw.md) | `sgcl/compress/lzw.h` | LZW both bit orders, Go's output byte for byte |
| [compress::zip](compress/zip.md) | `sgcl/compress/zip.h` | `archive` (entries read in any order, checked against the directory, limits) and `writer` (ZIP64, data descriptors) |
| [compress::tar](compress/tar.md) | `sgcl/compress/tar.h` | `reader` and `writer`: ustar, pax, GNU's long names |
| [compress::sevenzip](compress/sevenzip.md) | `sgcl/compress/sevenzip.h` | 7z: `archive` (`open`, `from`, `entries`, `reader`, `read`, `walk` with one decoder a folder) and `writer` (to a file or an `io::buffer`, solid, automatic filters, the header packed); LZMA/LZMA2/PPMd/Deflate/Copy both ways, BZip2 read, BCJ/BCJ2 and the other converters, Delta, anti-items |
| [duration](core/duration.md) | `sgcl/core/duration.h` | a span of time: nanoseconds in 64 bits, Go's text both ways (`"1h30m"`), `seconds()` and the other units, saturated arithmetic; from every integral `std::chrono` duration and into `std::chrono::nanoseconds`; what the timers take |
| [time::date](time/date.md) | `sgcl/time/date.h` | a date with no time of day and no zone: carried like Go's `time.Date`, `weekday`, `iso_week`, `add_months` cut to the month's end, ISO 8601's three forms |
| [time::zone](time/zone.md) | `sgcl/time/zone.h` | UTC, a fixed offset, the tz database, TZif bytes, a POSIX TZ string; offsets and transitions |
| [time::datetime](time/datetime.md) | `sgcl/time/datetime.h` | an instant and its zone, calendar arithmetic across a change of the clock, `now()` |
| [time text](time/layout.md) | `sgcl/time/layout.h` | formats by name and `%` patterns, written and read; `txt::format` |
| [range](core/range.md) | `sgcl/core/range.h` | a pair of iterators as a range (what `equal_range` hands back, made iterable) and the integers of `range(n)`, `range(first, last)`; for a range-for and `std::ranges` |
| [string](core/string.md) | `sgcl/core/string.h` | an immutable string on the managed heap: one word, shared by copying, compared and hashed by its contents, no destructor; UTF-8, with `runes()`, a `char32_t` as a character and Unicode's case and white space |
| [slice](core/slice.md) | `sgcl/core/slice.h` | the elements of a contiguous range and the managed object they lie in, held: Go's slice; a `std::span` when the memory is unmanaged (no owner); `slice<const char>` is text, what `as_slice` and the pieces of `split` are, `slice<byte>` the buffers of io |
| [expected](core/expected.md) | `sgcl/core/expected.h` | `std::expected`'s interface (C++23) over a variant: the value and the error laid out apart |
| [optional, pair, tuple, error_code](core/aliases.md) | `sgcl/core/aliases.h` | the `std` types under the library's names: they hold a tracked pointer correctly as they are, one value per place |
| [atomic, atomic_ref](core/atomic.md) | `sgcl/core/atomic.h`, `sgcl/core/atomic_ref.h` | lock-free atomic `tracked_ptr`: `load`, `store`, compare-exchange, `wait`/`notify`, no ABA ([atomic_ref](core/atomic_ref.md) on its own page) |

## Containers

The interfaces of `std`, the nodes and buffers on the managed heap: a container lives where a `tracked_ptr` may live, its elements are destroyed exactly when `std` destroys them, and the collector reclaims the memory.

| page | `std` counterpart |
|---|---|
| [vector](core/vector.md) | `std::vector` |
| [array](core/array.md) | `std::array`, with the braces of an aggregate and the mixins of a range |
| [dynamic_array](core/dynamic_array.md) | a count fixed at creation in a managed buffer that never moves: Java's `new T[n]` |
| [deque](core/deque.md) | `std::deque` |
| [list](core/list.md) | `std::list` |
| [forward_list](core/forward_list.md) | `std::forward_list` |
| [stack](core/stack.md) | `std::stack` |
| [queue, priority_queue](core/queue.md) | `std::queue`, `std::priority_queue` |
| [sorted_map](core/sorted_map.md) | `std::map` |
| [sorted_multimap](core/sorted_multimap.md) | `std::multimap` |
| [sorted_set](core/sorted_set.md) | `std::set` |
| [sorted_multiset](core/sorted_multiset.md) | `std::multiset` |
| [map](core/map.md) | `std::unordered_map` |
| [multimap](core/multimap.md) | `std::unordered_multimap` |
| [set](core/set.md) | `std::unordered_set` |
| [multiset](core/multiset.md) | `std::unordered_multiset` |
| [ordered_map](core/ordered_map.md) | a hash map in insertion order (Java `LinkedHashMap`) |
| [ordered_set](core/ordered_set.md) | a hash set in insertion order (Java `LinkedHashSet`) |

The questions, the order and the writes of a range (`contains`, `index_of`, `find_if`, `sort`, `reverse`, `min`, `for_each`...) are members of every container that iterates, from the mixins (`namespace mixin`: a static interface, brought in by a template, and a declaration a requirement (`namespace req`) can ask for):

| page | header | what it is |
|---|---|---|
| [mixin/](core/mixin/README.md) | `sgcl/core/mixin/mixin.h` | the mixins (`namespace mixin`) and the requirements (`namespace req`): [mixin::enumerable](core/mixin/enumerable.md), [mixin::equatable](core/mixin/equatable.md), [mixin::comparable](core/mixin/comparable.md), [mixin::ordered](core/mixin/ordered.md), [mixin::sequence](core/mixin/sequence.md), [mixin::lookup](core/mixin/lookup.md), [mixin::immutable](core/mixin/immutable.md), [mixin::text](core/mixin/text.md); [req](core/req.md): `req::enumerable`, `req::ordered`, `req::sequence`, `req::lookup`, `req::immutable`, `req::comparable`, ...; who carries what |

## Immutable containers

| page | header | what it is |
|---|---|---|
| [immutable::vector](immutable/vector.md) | `sgcl/immutable/vector.h` | Clojure's bit-partitioned trie with a tail: every `push_back`, `pop_back` and `sorted_set` a new version sharing all but a path |
| [immutable::list](immutable/list.md) | `sgcl/immutable/list.h` | the list of Lisp and ML: `push_front` one cell in front of the shared chain, `pop_front` the rest of it |
| [immutable::map](immutable/map.md) | `sgcl/immutable/map.h` | Bagwell's hash array mapped trie: every `insert` and `erase` a new version sharing all but a path; transparent lookup |
| [immutable::set](immutable/set.md) | `sgcl/immutable/set.h` | the same trie with the key as the element |

## Lock-free containers

Structures shared by any number of threads without a lock, the textbook algorithms with no reclamation scheme in them, because the collector is one; the interfaces of `java.util.concurrent` under the names of `std`.

| page | header | what it is |
|---|---|---|
| [concurrent::queue](concurrent/queue.md) | `sgcl/concurrent/queue.h` | the Michael–Scott queue: unbounded, FIFO, `push`, `try_pop`, a blocking `pop` |
| [concurrent::stack](concurrent/stack.md) | `sgcl/concurrent/stack.h` | the Treiber stack: one word, `push`, `try_pop`, a blocking `pop` |
| [concurrent::bounded_queue](concurrent/bounded_queue.md) | `sgcl/concurrent/bounded_queue.h` | Vyukov's bounded MPMC queue: a ring of cells with sequence numbers, one compare-exchange per operation, no allocation per element, `try_push`, `try_pop`, a blocking `push` and `pop` |
| [spsc_queue](concurrent/spsc_queue.md) | `sgcl/concurrent/spsc_queue.h` | a ring with a sequence per cell for one producer and one consumer, the cell the one line the two share: wait-free, no compare-exchange, the same interface |
| [concurrent::priority_queue](concurrent/priority_queue.md) | `sgcl/concurrent/priority_queue.h` | a binary heap under a spin-then-park lock, what Java's `PriorityBlockingQueue` is (the skip-list one lost to it): the least element first, equal ones FIFO, `push`, `try_pop`, a blocking `pop`, `try_top` |
| [concurrent::sorted_map](concurrent/sorted_map.md) | `sgcl/concurrent/sorted_map.h` | the lock-free skip list of Herlihy and Shavit: an sorted map with `find`, `insert`, `try_emplace`, `erase`, weakly consistent iteration |
| [concurrent::sorted_set](concurrent/sorted_set.md) | `sgcl/concurrent/sorted_set.h` | the same skip list with the key as the element |
| [concurrent::map](concurrent/map.md) | `sgcl/concurrent/map.h` | the split-ordered list of Shalev and Shavit: a lock-free hash map that doubles its bucket array without moving a node |
| [concurrent::set](concurrent/set.md) | `sgcl/concurrent/set.h` | the same table with the key as the element |
| [concurrent::cache](concurrent/cache.md) | `sgcl/concurrent/cache.h` | a cache over the hash map bounded by a capacity and a time to live, the least recently used evicted by sampling: `get`, `put`, `get_or_compute`, `hits`, `misses` |
| [concurrent::weak_map](concurrent/weak_map.md) | `sgcl/concurrent/weak_map.h` | the weak_map shared by any number of threads: values attached to objects the map does not keep alive, over the lock-free hash table, the dead entries swept by the inserting threads |
| [concurrent::weak_set](concurrent/weak_set.md) | `sgcl/concurrent/weak_set.h` | the same table with the objects alone: a set of objects it does not keep alive, shared by the threads |
| [copy_on_write](concurrent/copy_on_write.md) | `sgcl/concurrent/copy_on_write.h` | a value read by many threads and replaced whole: one load for an immutable snapshot, a copy and a compare-exchange for a change |
| [intern](concurrent/intern.md) | `sgcl/concurrent/intern.h` | a pool where equal values share one managed object (Go's `unique`, Java's `String.intern`): `make(value)` the canonical object, held weakly, compared by identity; `intern_string` for strings |
| [channel](async/channel.md) | `sgcl/async/channel.h` | the channel of Go: a buffered or rendezvous queue that threads and coroutines send to and receive from, waiting on either side, closed to end the stream |
| [select](async/select.md) | `sgcl/async/select.h` | the select of Go: a wait on several channels at once, a receive or a send per case with a body, `otherwise` for a poll; for a thread or a coroutine |
| [broadcast](async/broadcast.md) | `sgcl/async/broadcast.h` | a channel every subscriber receives every value from (tokio's broadcast, Kotlin's SharedFlow): one ring, a cursor per subscription, a send that never waits, a subscriber that falls behind lapped and told how many it lost |
| [timer](async/timer.md) | `sgcl/async/timer.h` | time: `sleep`, `sleep_until`, `after`, `at`, `tick`, `timeout`: a task suspended for a while or until a point, an event set after a while or at a point, a channel signalled every while, a select case served after a while; one timer thread under them |
| [clock](core/clock.md) | `sgcl/core/clock.h` | `clock::now()`, the library's time in one place: the steady clock's, or a test's manual clock's while one is installed; `time_point` |
| [manual_clock](async/manual_clock.md) | `sgcl/async/timer.h` | the clock of a test: installed, time moves only by `advance(d)`, which fires every timer due with no real waiting |
| [run](async/run.md) | `sgcl/async/run.h` | the entry of a program: `async::run(program())`, and `async::run(f)` with a stop_token stopped by the first Ctrl-C or SIGTERM |
| [signal](async/signal.md) | `sgcl/async/signal.h` | `async::signals({SIGINT, SIGTERM})`: the signals of the process as a channel, for a task, a thread or a select; `reset_signals`, `ignore_signals` |
| [stop_token](async/stop_token.md) | `sgcl/async/stop_token.h` | cancellation: `stop_source` requests the stop, `stop_token` is a channel closed by it (a select case, an awaitable), a deadline is a timer, a child source stops with its parent |
| [when](async/when.md) | `sgcl/async/when.h` | the composition of tasks: `when_all` (every result as a tuple or a vector), `when_any` (the index of the first to finish) |
| [mutex](async/mutex.md) | `sgcl/async/mutex.h` | one holder at a time, a channel holding one signal: blocking, awaitable and as a select case; a task waiting holds no thread |
| [semaphore](async/semaphore.md) | `sgcl/async/semaphore.h` | n permits, a channel holding n signals; the three forms of a wait |
| [event](async/event.md) | `sgcl/async/event.h` | set once, waited for by any number: a channel closed by the set; what `readable`, `after` and `at` give |
| [wait_group](async/wait_group.md) | `sgcl/async/wait_group.h` | counts work down to zero, a channel per round closed at zero (Go's WaitGroup) |
| [once](async/once.md) | `sgcl/async/once.h` | the first caller runs it, the others wait for it, blocking or awaitable |
| [shared_mutex](async/shared_mutex.md) | `sgcl/async/shared_mutex.h` | any number of readers or one writer, over a word, writer preference (Go's RWMutex); blocking and awaitable |
| [condition_variable](async/condition_variable.md) | `sgcl/async/condition_variable.h` | Go's Cond over the mutex: a wait lets go of the mutex, waits for a notify and takes it back; blocking and awaitable |
| [promise](async/promise.md) | `sgcl/async/promise.h` | `async::promise<T>`: a one-shot completion set once by any thread or C callback, awaited by a task, blocked on by a thread, a case of a select; the adapter between a platform's callbacks and `co_await` |
| [blocking](async/blocking.md) | `sgcl/async/blocking.h` | `spawn_blocking`: a blocking call on a pool of threads apart from the workers, its result back through a promise; the pool grows on demand to a cap and its idle threads exit; `blocking_pool` for its statistics and stop |
| [task_group](async/task_group.md) | `sgcl/async/task_group.h` | structured concurrency: a scope that owns the tasks it spawns, waited for as one (a thread, a task, a select case), stopped as one by the first exception, which the wait rethrows; Go's `errgroup`, Kotlin's `coroutineScope` |
| [timeout](async/timeout.md) | `sgcl/async/timeout.h` | a timeout on a task: `async::with_timeout(t, d)` and `with_deadline(t, when)` the result or the error `timed_out`, a token as the deadline (`stopped`); the loser stopped through its source or left to finish |
| [reactor](async/reactor.md) | `sgcl/async/reactor.h` | `readable`, `writable`, `exited`: the readiness of a file descriptor, the end of a process, as an event; one thread on the kernel's queue (kqueue; epoll and IOCP to come); the foundation of io and net |

## Files and streams

| page | header | what it is |
|---|---|---|
| [error](io/error.md) | `sgcl/io/error.h` | `errc`, `error` (code, operation, path; `is_not_found()`…): every operation of io returns `expected<T, error>`, nothing throws |
| [stream](io/stream.md) | `sgcl/io/stream.h` | what io takes as a stream: whatever has `read` (`async_read`) or `write` (`async_write`), or a lambda of that shape, checked by `io::req`; `io::reader` and `io::writer` to hold any of them, Go's interface values; `copy`, `read_full`, `read_all`, `write` and their `async_` forms; the mixins; `limit_reader`, `tee_reader`, `multi_reader`, `multi_writer`, `transform_reader`, `io::discard`, `buffer` |
| [buffered](io/buffered.md) | `sgcl/io/buffered.h` | `buffered_reader`: lines and prefixes as views into a managed block, `lines()`, a bound for untrusted streams; `buffered_writer`: `flush` |
| [file](io/file.md) | `sgcl/io/file.h` | `file` over any descriptor: `open`, `create`, `from_fd`, `pipe`, `read_at`/`write_at`, `stat`, `sync`; async through the blocking pool or the reactor; `read_file`, `write_file`, `append_file`, `temp_file`, `temp_dir` |
| [fs](io/fs.md) | `sgcl/io/fs.h` | `stat`, `lstat`, `file_info`, `permissions`, `mkdir_all`, `remove_all`, `rename`, `copy_file`, `symlink`, `chmod`, `read_dir`, `walk_dir` |
| [mapping](io/mapping.md) | `sgcl/io/mapping.h` | `map`, `map_options`, `mapping`: a file mapped into memory, read only or writable, shared or a private copy, any range; the bytes a slice that keeps the mapping alive |
| [shared_memory](io/shared_memory.md) | `sgcl/io/shared_memory.h` | `shared_memory`: `create`, `open`, `remove` a named region between processes; POSIX `shm_open`, Windows `CreateFileMappingW` |
| [path](io/path.md) | `sgcl/io/path.h` | `clean`, `join`, `base`, `dir`, `ext`, `stem`, `split`, `abs`, `rel`, `match`, `glob`: paths as strings, a view in and a string out |
| [exec](io/exec.md) | `sgcl/io/exec.h` | `command`: a program run with the fields of `exec.Cmd`, its streams the library's, `run`, `output`, the pipes, a stop token; `process`, `process_state`, `look_path`; the exit waited for on the reactor |
| [print](io/print.md) | `sgcl/io/print.h` | `print`, `println`, `eprint`, `eprintln`: a pattern of `txt::format` and its values on `stdout`, `stderr` or any writer, in one call; `sgcl::println` as well |
| [os](io/os.md) | `sgcl/io/os.h` | `args`, `getenv`, `environ`, `expand_env`, `working_dir`, `home_dir`, `cache_dir`, `executable`, `hostname`, `stdin`/`stdout`/`stderr` as files, `exit` |

## Coroutines, observers, the collector

| page | header | what it is |
|---|---|---|
| [managed_frame, frame_ptr](core/coroutine.md) | `sgcl/core/coroutine.h` | coroutine frames on the managed heap, whose parameters, locals and promise are roots while the frame is held; the owner of a coroutine that lives anywhere |
| [generator](core/generator.md) | `sgcl/core/generator.h` | a coroutine that `co_yield`s values, consumed with a range-for, its frame managed; no scheduler |
| [coroutine](async/coroutine.md) | `sgcl/async/coroutine.h` | `task`, `async::generator`: coroutines on the managed frame of the core; a task is spawned, joined, awaited, detached; a task starts with the first wait for it; `on_unhandled` for what a task let go of threw |
| [scheduler](async/scheduler.md) | `sgcl/async/scheduler.h` | the pool of workers that runs the tasks: `spawn`, `yield`, `async::scheduler::stop`; a task that waits holds no thread |
| [executor](async/executor.md) | `sgcl/async/executor.h` | `executor`: a task on a thread of the program's choosing (the main thread, a foreign loop through `poll`), resumed there after every wait; `strand`: tasks on the workers one at a time, in order; `co_await on(ex)`, `co_await on_workers()` |
| [task_local](async/task_local.md) | `sgcl/async/task_local.h` | a value visible to a task and to the tasks it starts, read from any function under it: `co_await x.set(v)`, `x.get()`, `x.with(v, t)`; inherited, copy on write |
| [expiry_queue](core/expiry_queue.md) | `sgcl/core/expiry_queue.h` | a callback for an object the collector found unreachable, with the object alive again for the call |
| [collector](core/collector.md) | `sgcl/core/collector.h` | `force_collect`, `terminate`, statistics and phase times, live objects and bytes by type, the memory limit |
| [config](core/config.md) | `sgcl/core/config.h` | the compile-time constants and the `-D` macros that set them |
| [diagnostics](../garbage_collector/diagnostics.md) | | the tools and the cases: what is alive and why, what a rule broken looks like, what a cycle costs, a race with the collector |
| [how it works](../garbage_collector/how-it-works.md) | | the engine: the heap, a slot's states, the barrier, the roots, a cycle phase by phase, epochs and parity, young and full cycles, the weak phase, the cells of the `root_ptr`s and when to use which pointer, allocation |

## Reading the pages

Every page has the same layout: the include and the declaration, what the class is and how it differs from `std`, the rules that apply to it (where an object of the class may live, what it may hold, thread safety, what happens in destructors), the members in the order of the header, each with its signature and a short example, one complete program at the end, and links to the related pages and README sections. The examples use C++20 class template argument deduction (`tracked_ptr p = make_tracked<T>();`) and every `force_collect()` in them is optional, there to show the result at once.

The code on the pages assumes `using namespace sgcl;`. Each complete program says so after its includes and has no other directive: every other module's name is written with its module, `encoding::hex::encode(crypto::sha256::of("abc"))`, `async::task<> worker(async::channel<int> jobs)`, `net::http::server`, `io::open`, `txt::format`, `concurrent::queue`, as Go writes `hex.EncodeToString(sha256.Sum256(b))`. The module says where a name comes from, examples are copied into programs where modules meet, and one rule needs no list of exceptions. It also settles the names two modules share (`async::sleep` beside `sgcl::sleep` and the C library's, `crypto::random` beside `math::random`, `hash::mixin` beside `sgcl::mixin`) and io's `remove`, `rename`, `getenv`, `chdir` and `symlink`, functions of the C library too, which a bare call with a string literal would choose without a word; `println` is in `sgcl`, so a program that only prints names no io at all. The one directive kept is for literals, which cannot be written qualified: `using namespace math::literals;`, as `std::chrono_literals`. One collision the directive brings: a bare `time(nullptr)` of the C library is ambiguous with the namespace `sgcl::time` under `using namespace sgcl;` — write `std::time(nullptr)`.

A variable is not named after its type (`crypto::aes cipher(key)`, `net::http::server srv`, not `aes aes(key)`), and a value is named by its type where `auto` would need a `*`: `vector<byte> key = encoding::hex::decode("…")`, never `auto key = *encoding::hex::decode("…")`; a type that has a constructor from text takes the literal in it (`net::ip_address a("10.0.0.1")`), a type whose constructor from text would mean something else — json's, which makes a string value — takes the literal by its name and the conversion (`encoding::json doc = encoding::json::parse(R"({"a": 1})");`, never `.value()` or `*`), and `parse` is kept for text that comes from outside, an argument, a file, the network. The short snippets beside the members keep their names as they stand, most of them qualified (`async::task<>`, `io::open`). The synopsis blocks stand inside the header's namespace as the headers do.

A call that may wait comes in two ways. In io, net, encoding and hash, which are synchronous as their counterparts in POSIX, `std` and Go are, the name does its work on the calling thread and returns the result (`f.read(b)`, `io::copy(w, r)`), and `async_` is the same for a task, which gives the worker back while it waits (`co_await f.async_read(b)`). In async, whose calls exist to wait in a task, an operation has one name and is carried out by `co_await ch.receive()` in a task or `ch.receive().wait()` on a thread; the name alone makes a description of the operation ([`async::operation`](async/README.md#waiting-operations), nodiscard) and does nothing. The Lockable members the standard names — a mutex's `lock()`, `try_lock()`, `unlock()` and their shared forms — stay blocking, for `std::lock_guard` and `std::shared_lock`; a task takes a mutex with `co_await m.scoped_lock()`.
