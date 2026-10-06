// The net module's stage 1a counterparts in Go: net and net/netip, one case
// per run (benchmarks/net/net.cpp has the SGCL side, the same cases).
// Prints one line, ns per operation.
//
//	pingpong [n]   64 B there and back over one TCP connection, both ends goroutines: per round trip
//	stream [mb]    mb megabytes (1024 by default) one way over one connection, 32 KB writes: per byte, and GB/s
//	connect [n]    net.Dial and Accept on the loopback, then both closed: per connection
//	parse [n]      netip.ParseAddr of a mix of IPv4 and IPv6 text: per address
//	format [n]     netip.Addr.String of the same mix: per address
//	dns_parse, dns_https_parse, dns_lookup: dns.go
//	socks5_connect [n]  a minimal SOCKS5 client by hand (the standard library has no SOCKS
//	               dialer of its own) to an IPv4 target through a minimal SOCKS5 server on
//	               the loopback, then closed: per connection
//	proxy_get [n]  GET of a 13-byte body as http_hello, through a minimal HTTP forward proxy on
//	               the loopback (the same proxy as the C++ side's): per request
//	multipart_parse [n]  a multipart/form-data body of 20 fields and one file part of 1 MB, in
//	               memory, read by mime/multipart.Reader, the contents read 32 KB at a time: per body
//	multipart_write [n]  the same form written by mime/multipart.Writer to io.Discard, its file of
//	               1 MB copied from a file in $TMPDIR: per body
//	udp_multicast: multicast.go
//	tls_handshake [n]  a TCP connection and a full TLS 1.3 handshake on the loopback (crypto/tls, X25519, the ECDSA
//	               P-256 leaf of tests/net/tls_testdata, tickets on, no client cache), one byte read, both closed:
//	               per connection (run from the root of the tree: the certificates' paths)
//	tls_resume [n] the same, every handshake resuming the session of the one before (ClientSessionCache)
//	tls_mtls [n]   tls_handshake with a client certificate (ECDSA P-256), RequireAndVerifyClientCert
//	tls_ech [n]    tls_handshake with Encrypted Client Hello (RFC 9849): the server's key DHKEM(X25519) with
//	               HKDF-SHA256 and AES-128-GCM, the public name public.example, the client given its ECHConfigList
//	tls_stream [mb]  stream over one TLS 1.3 connection (the configs of tls_handshake, AES-128-GCM): mb
//	               megabytes (1024 by default) from the client to the server, 32 KB writes and reads: per byte
//	ws_echo [n]    a WebSocket text message of 64 B there and back over one connection: per round
//	               trip; both sides a minimal peer written by hand from RFC 6455 with the standard
//	               library alone (no gorilla, no x/net)
//	ws_throughput [n]  binary messages of 1 MB from the client to the server, masked: per message
//	sse_events [n] Server-Sent Events of about 30 B through one stream, each flushed by http.Flusher,
//	               read by a reader of bufio lines written by hand: per event
//	tls12_server   a crypto/tls server of TLS 1.2 alone (MinVersion and MaxVersion 1.2, X25519, the ECDSA
//	               P-256 leaf): prints "port N", answers each connection with one byte, until killed
//	tls12_handshake [n]  a TCP connection and a full TLS 1.2 handshake to the tls12_server at
//	               $SGCL_TLS12_SERVER (both sides' clients against the one Go server), one byte read,
//	               closed: per connection (Go's client offers 1.3 and 1.2, as the module's does)
//	tls12_resume [n]  the same, every handshake resuming the session of the one before
//	               (ClientSessionCache: the abbreviated handshake of TLS 1.2 by the server's ticket)
//	reverse_proxy_get [n]  GET of a 13-byte body as http_hello, through httputil.NewSingleHostReverseProxy (the
//	               handler of a net/http server, its Transport keeping 256 idle connections a host) to the
//	               backend, all three on the loopback: per request
//	reverse_proxy_stream [n]  GET of 1 MB through the same proxy, the backend writing it in 16 flushed pieces
//	               of 64 KB: per request, and MB/s
//	imap_*: imap.go
//	dot_lookup, doh_lookup: dnssecure.go
//	mdns_parse, mdns_build, mdns_roundtrip: mdns.go
package main

import (
	"bufio"
	"bytes"
	"compress/gzip"
	"crypto/ecdh"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha1"
	"crypto/sha256"
	"crypto/tls"
	"crypto/x509"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"math/big"
	"mime/multipart"
	"net"
	"net/http"
	"net/http/cookiejar"
	"net/http/httptest"
	"net/http/httputil"
	"net/netip"
	"net/url"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"
)

var urls = []string{"http://example.com/", "https://user:pass@example.com:8443/a/b/c?x=1&y=2#top", "http://10.0.0.1/index.html", "https://[2001:db8::1]:443/", "http://example.com/a/../b/./c/d?q=hello%20world", "ftp://ftp.example.org/pub/file.tar.gz", "https://api.example.com/v1/users/12345/orders?limit=50&offset=100", "ws://chat.example.com/socket"}

var addresses = []string{"127.0.0.1", "10.1.2.3", "192.168.100.200", "8.8.8.8", "::1", "2001:db8::1", "fe80::1:2:3:4", "2001:db8:85a3::8a2e:370:7334", "::ffff:192.0.2.128", "2606:4700:4700::1111"}

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func report(what string, wall float64, ops float64, extra string) {
	fmt.Printf("net %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs%s\n", what, wall*1e9/ops, ops/wall, wall, cpuSeconds(), extra)
}

func listen() net.Listener {
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		panic(err)
	}
	return l
}

// A minimal SOCKS5 server (RFC 1928): no authentication, CONNECT to an
// IPv4 address; the target dialed, the reply sent, the client's close
// awaited, both closed
func socks5Server(l net.Listener) {
	for {
		c, err := l.Accept()
		if err != nil {
			return
		}
		go func(c net.Conn) {
			defer c.Close()
			b := make([]byte, 16)
			if _, err := io.ReadFull(c, b[:3]); err != nil {
				return
			}
			c.Write([]byte{5, 0})
			if _, err := io.ReadFull(c, b[:10]); err != nil || b[3] != 1 {
				return
			}
			to := &net.TCPAddr{IP: net.IPv4(b[4], b[5], b[6], b[7]), Port: int(b[8])<<8 | int(b[9])}
			t, err := net.DialTCP("tcp", nil, to)
			if err != nil {
				return
			}
			defer t.Close()
			c.Write([]byte{5, 0, 0, 1, 127, 0, 0, 1, 0, 0})
			c.Read(b)
		}(c)
	}
}

// A minimal HTTP forward proxy: a connection to the origin per client
// connection, kept; each request head's absolute-form target cut to
// origin-form and written there, the response read by its Content-Length
// and written back (GETs without bodies)
func proxyServer(l net.Listener, origin string) {
	for {
		c, err := l.Accept()
		if err != nil {
			return
		}
		go func(c net.Conn) {
			defer c.Close()
			up, err := net.Dial("tcp", origin)
			if err != nil {
				return
			}
			defer up.Close()
			buf := make([]byte, 16384)
			var in, out []byte
			for {
				e := -1
				for e = bytesIndex(in, "\r\n\r\n"); e < 0; e = bytesIndex(in, "\r\n\r\n") {
					n, err := c.Read(buf)
					if n == 0 || err != nil {
						return
					}
					in = append(in, buf[:n]...)
				}
				head := string(in[:e+4])
				in = in[e+4:]
				sp := strings.IndexByte(head, ' ')
				scheme := strings.Index(head[sp:], "://") + sp
				path := strings.IndexByte(head[scheme+3:], '/') + scheme + 3
				head = head[:sp+1] + head[path:]
				if _, err := up.Write([]byte(head)); err != nil {
					return
				}
				for e = bytesIndex(out, "\r\n\r\n"); e < 0; e = bytesIndex(out, "\r\n\r\n") {
					n, err := up.Read(buf)
					if n == 0 || err != nil {
						return
					}
					out = append(out, buf[:n]...)
				}
				length := 0
				if cl := bytesIndex(out[:e], "Content-Length: "); cl >= 0 {
					rest := string(out[cl+16 : e])
					if nl := strings.IndexByte(rest, '\r'); nl >= 0 {
						rest = rest[:nl]
					}
					length, _ = strconv.Atoi(rest)
				}
				for len(out) < e+4+length {
					n, err := up.Read(buf)
					if n == 0 || err != nil {
						return
					}
					out = append(out, buf[:n]...)
				}
				if _, err := c.Write(out[:e+4+length]); err != nil {
					return
				}
				out = out[e+4+length:]
			}
		}(c)
	}
}

// The multipart cases' file part: 1 MB of bytes from the C++ side's generator
func multipartFile() []byte {
	b := make([]byte, 1<<20)
	x := uint32(12345)
	for i := range b {
		x = x*1103515245 + 12345
		b[i] = byte(x >> 23)
	}
	return b
}

func multipartBody(boundary string) []byte {
	var b bytes.Buffer
	for i := 0; i < 20; i++ {
		fmt.Fprintf(&b, "--%s\r\nContent-Disposition: form-data; name=\"field%d\"\r\n\r\nvalue %d of the form, some text\r\n", boundary, i, i)
	}
	fmt.Fprintf(&b, "--%s\r\nContent-Disposition: form-data; name=\"upload\"; filename=\"data.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n", boundary)
	b.Write(multipartFile())
	fmt.Fprintf(&b, "\r\n--%s--\r\n", boundary)
	return b.Bytes()
}

func bytesIndex(b []byte, s string) int {
	return strings.Index(string(b), s)
}

// io.Discard that counts what it is given
type countingWriter struct{ n int64 }

func (c *countingWriter) Write(p []byte) (int, error) {
	c.n += int64(len(p))
	return len(p), nil
}

// The client's side: greeting, method, CONNECT, reply (10 bytes for an IPv4 BND)
func socks5Dial(proxy string, target *net.TCPAddr) (net.Conn, error) {
	c, err := net.Dial("tcp", proxy)
	if err != nil {
		return nil, err
	}
	b := make([]byte, 10)
	if _, err := c.Write([]byte{5, 1, 0}); err != nil {
		c.Close()
		return nil, err
	}
	if _, err := io.ReadFull(c, b[:2]); err != nil || b[0] != 5 || b[1] != 0 {
		c.Close()
		return nil, fmt.Errorf("socks5 method")
	}
	req := []byte{5, 1, 0, 1}
	req = append(req, target.IP.To4()...)
	req = append(req, byte(target.Port>>8), byte(target.Port))
	if _, err := c.Write(req); err != nil {
		c.Close()
		return nil, err
	}
	if _, err := io.ReadFull(c, b[:10]); err != nil || b[1] != 0 {
		c.Close()
		return nil, fmt.Errorf("socks5 reply")
	}
	return c, nil
}

// A minimal WebSocket (RFC 6455) by hand: frames of one message each way,
// the client masking eight bytes at a time
type wsConn struct {
	c      net.Conn
	r      *bufio.Reader
	client bool
	buf    []byte
}

func wsMask(p []byte, key [4]byte) {
	k := uint64(binary.LittleEndian.Uint32(key[:]))
	k |= k << 32
	i := 0
	for ; i+8 <= len(p); i += 8 {
		binary.LittleEndian.PutUint64(p[i:], binary.LittleEndian.Uint64(p[i:])^k)
	}
	for ; i < len(p); i++ {
		p[i] ^= key[i%4]
	}
}

func (w *wsConn) write(op byte, payload []byte) error {
	h := []byte{0x80 | op, 0}
	n := len(payload)
	switch {
	case n < 126:
		h[1] = byte(n)
	case n <= 0xFFFF:
		h[1] = 126
		h = append(h, byte(n>>8), byte(n))
	default:
		h[1] = 127
		h = binary.BigEndian.AppendUint64(h, uint64(n))
	}
	if !w.client {
		_, err := w.c.Write(append(h, payload...))
		return err
	}
	h[1] |= 0x80
	var key [4]byte
	rand.Read(key[:])
	h = append(h, key[:]...)
	w.buf = append(append(w.buf[:0], h...), payload...)
	wsMask(w.buf[len(h):], key)
	_, err := w.c.Write(w.buf)
	return err
}

func (w *wsConn) read() (byte, []byte, error) {
	var h [2]byte
	if _, err := io.ReadFull(w.r, h[:]); err != nil {
		return 0, nil, err
	}
	n := uint64(h[1] & 0x7F)
	switch n {
	case 126:
		var b [2]byte
		io.ReadFull(w.r, b[:])
		n = uint64(binary.BigEndian.Uint16(b[:]))
	case 127:
		var b [8]byte
		io.ReadFull(w.r, b[:])
		n = binary.BigEndian.Uint64(b[:])
	}
	var key [4]byte
	masked := h[1]&0x80 != 0
	if masked {
		io.ReadFull(w.r, key[:])
	}
	p := make([]byte, n)
	if _, err := io.ReadFull(w.r, p); err != nil {
		return 0, nil, err
	}
	if masked {
		wsMask(p, key)
	}
	return h[0] & 0x0F, p, nil
}

func wsAcceptKey(key string) string {
	h := sha1.Sum([]byte(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
	return base64.StdEncoding.EncodeToString(h[:])
}

// The server's handler: the upgrade, then echo (or count, answering a
// text with the count of the bytes before it)
func wsHandler(echo bool) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		c, rw, err := w.(http.Hijacker).Hijack()
		if err != nil {
			return
		}
		defer c.Close()
		c.Write([]byte("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + wsAcceptKey(r.Header.Get("Sec-WebSocket-Key")) + "\r\n\r\n"))
		ws := &wsConn{c: c, r: rw.Reader}
		total := 0
		for {
			op, p, err := ws.read()
			if err != nil || op == 8 {
				return
			}
			if echo {
				ws.write(op, p)
				continue
			}
			if op == 1 {
				ws.write(1, []byte(strconv.Itoa(total)))
				continue
			}
			total += len(p)
		}
	}
}

func wsDial(addr string) (*wsConn, error) {
	c, err := net.Dial("tcp", addr)
	if err != nil {
		return nil, err
	}
	var k [16]byte
	rand.Read(k[:])
	key := base64.StdEncoding.EncodeToString(k[:])
	fmt.Fprintf(c, "GET /ws HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n", addr, key)
	r := bufio.NewReader(c)
	res, err := http.ReadResponse(r, nil)
	if err != nil || res.StatusCode != 101 || res.Header.Get("Sec-WebSocket-Accept") != wsAcceptKey(key) {
		c.Close()
		return nil, fmt.Errorf("handshake")
	}
	return &wsConn{c: c, r: r, client: true}, nil
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: net <pingpong|stream|connect|parse|format|url|http_parse|http_hello|serve_range|serve_ranges|serve_etag|serve_handler_range|serve_handler_etag|mw_none|mw_chain_filter|mw_chain_around|mw_csrf|auth_basic|oauth2_get|oidc_verify|compress_json|compress_stream|dns_parse|dns_https_parse|dns_lookup|socks5_connect|proxy_get|multipart_parse|multipart_write|udp_multicast|tls_handshake|tls_resume|tls_mtls|tls_ech|tls_stream|ws_echo|ws_throughput|sse_events|tls12_server|tls12_handshake|tls12_resume|cookie_jar|reverse_proxy_get|reverse_proxy_stream|dot_lookup|doh_lookup|mdns_parse|mdns_roundtrip> [n]")
		os.Exit(2)
	}
	what := os.Args[1]
	var n int64
	if len(os.Args) > 2 {
		n, _ = strconv.ParseInt(os.Args[2], 10, 64)
	}
	ok := true
	switch what {
	case "pingpong":
		if n == 0 {
			n = 50000
		}
		l := listen()
		go func() {
			c, err := l.Accept()
			if err != nil {
				return
			}
			io.Copy(c, c)
			c.Close()
		}()
		c, err := net.Dial("tcp", l.Addr().String())
		if err != nil {
			panic(err)
		}
		buf := make([]byte, 64)
		t0 := time.Now()
		done := make(chan int64)
		go func() {
			var k int64
			for ; k < n; k++ {
				if _, err := c.Write(buf); err != nil {
					break
				}
				if _, err := io.ReadFull(c, buf); err != nil {
					break
				}
			}
			c.Close()
			done <- k
		}()
		k := <-done
		report("pingpong", time.Since(t0).Seconds(), float64(n), "")
		l.Close()
		ok = k == n
	case "stream":
		mb := n
		if mb == 0 {
			mb = 1024
		}
		bytes := mb << 20
		l := listen()
		received := make(chan int64)
		go func() {
			c, err := l.Accept()
			if err != nil {
				received <- 0
				return
			}
			buf := make([]byte, 32768)
			var total int64
			for {
				r, err := c.Read(buf)
				total += int64(r)
				if err != nil {
					break
				}
			}
			c.Close()
			received <- total
		}()
		c, err := net.Dial("tcp", l.Addr().String())
		if err != nil {
			panic(err)
		}
		buf := make([]byte, 32768)
		t0 := time.Now()
		var sent int64
		for sent < bytes {
			w, err := c.Write(buf)
			if err != nil {
				break
			}
			sent += int64(w)
		}
		c.Close()
		got := <-received
		wall := time.Since(t0).Seconds()
		report("stream", wall, float64(got), fmt.Sprintf(" GB/s=%.2f", float64(got)/wall/1e9))
		l.Close()
		ok = got == sent
	case "connect":
		if n == 0 {
			n = 2000 // RUNS of both variants inside the 16384 ephemeral ports a TIME_WAIT of 30 s leaves
		}
		l := listen()
		taken := make(chan int64)
		go func() {
			var k int64
			for ; k < n; k++ {
				c, err := l.Accept()
				if err != nil {
					break
				}
				c.Close()
			}
			taken <- k
		}()
		t0 := time.Now()
		var made int64
		for ; made < n; made++ {
			c, err := net.Dial("tcp", l.Addr().String())
			if err != nil {
				break
			}
			c.Close()
		}
		if made < n {
			l.Close() // a dial failed (the ports ran out): the accepts end rather than wait for connections that will not come
		}
		k := <-taken
		report("connect", time.Since(t0).Seconds(), float64(n), "")
		l.Close()
		ok = made == n && k == n
	case "parse", "format":
		if n == 0 {
			n = 20000000
		}
		parsed := make([]netip.Addr, len(addresses))
		for i, s := range addresses {
			parsed[i] = netip.MustParseAddr(s)
		}
		check := 0
		t0 := time.Now()
		if what == "parse" {
			for i := int64(0); i < n; i++ {
				a, _ := netip.ParseAddr(addresses[i%int64(len(addresses))])
				if a.Is4() {
					check++
				}
			}
		} else {
			for i := int64(0); i < n; i++ {
				check += len(parsed[i%int64(len(parsed))].String())
			}
		}
		report(what, time.Since(t0).Seconds(), float64(n), "")
		ok = check > 0
	case "url":
		if n == 0 {
			n = 2000000
		}
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			u, _ := url.Parse(urls[i%int64(len(urls))])
			check += len(u.EscapedPath())
		}
		report("url", time.Since(t0).Seconds(), float64(n), "")
		ok = check > 0
	case "http_parse":
		if n == 0 {
			n = 2000000
		}
		text := "GET /api/v1/users/12345?fields=name,email HTTP/1.1\r\nHost: api.example.com\r\nUser-Agent: bench/1.0\r\n" +
			"Accept: application/json\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: en-US,en;q=0.9\r\n" +
			"Connection: keep-alive\r\nCookie: session=abc123; theme=dark\r\nReferer: https://example.com/page\r\n" +
			"Cache-Control: no-cache\r\nX-Request-Id: 7f3c9a2e-1b4d-4e6f-8a9b-0c1d2e3f4a5b\r\n" +
			"Authorization: Bearer eyJhbGciOiJIUzI1NiJ9.e30.x\r\nX-Forwarded-For: 203.0.113.7\r\n\r\n"
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			r, err := http.ReadRequest(bufio.NewReader(strings.NewReader(text)))
			if err == nil {
				check += len(r.Header) + 1
			}
		}
		report("http_parse", time.Since(t0).Seconds(), float64(n), "")
		ok = check == int(n)*12
	case "http_hello":
		if n == 0 {
			n = 50000
		}
		l := listen()
		mux := http.NewServeMux()
		mux.HandleFunc("GET /hello", func(w http.ResponseWriter, r *http.Request) { io.WriteString(w, "hello, world\n") })
		srv := &http.Server{Handler: mux}
		go srv.Serve(l)
		url := "http://" + l.Addr().String() + "/hello"
		c := &http.Client{}
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			res, err := c.Get(url)
			if err == nil {
				b, _ := io.ReadAll(res.Body)
				res.Body.Close()
				check += len(b)
			}
		}
		report("http_hello", time.Since(t0).Seconds(), float64(n), "")
		srv.Close()
		ok = check == int(n)*13
	case "compress_json", "compress_stream":
		if n == 0 {
			n = 3000
		}
		var sb strings.Builder
		sb.WriteString("[")
		for i := 0; sb.Len() < 65536; i++ {
			fmt.Fprintf(&sb, "{\"id\":%d,\"name\":\"item %d\",\"ok\":true},", i, i)
		}
		body := []byte(sb.String()[:65536])
		stream := what == "compress_stream"
		h := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Content-Type", "application/json")
			if !strings.Contains(r.Header.Get("Accept-Encoding"), "gzip") {
				w.Write(body)
				return
			}
			w.Header().Set("Content-Encoding", "gzip")
			w.Header().Add("Vary", "Accept-Encoding")
			z := gzip.NewWriter(w)
			if stream {
				for k := 0; k < 16; k++ {
					z.Write(body[k*4096 : (k+1)*4096])
					z.Flush()
					w.(http.Flusher).Flush()
				}
			} else {
				z.Write(body)
			}
			z.Close()
		})
		check, out := 0, 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			r := httptest.NewRequest("GET", "/data", nil)
			r.Header.Set("Accept-Encoding", "gzip")
			rec := httptest.NewRecorder()
			h.ServeHTTP(rec, r)
			out += rec.Body.Len()
			if rec.Header().Get("Content-Encoding") == "gzip" {
				check++
			}
		}
		report(what, time.Since(t0).Seconds(), float64(n), fmt.Sprintf(" bytes=%d", out/int(n)))
		ok = check == int(n)
	case "oauth2_get":
		if n == 0 {
			n = 50000
		}
		l := listen()
		mux := http.NewServeMux()
		mux.HandleFunc("GET /hello", func(w http.ResponseWriter, r *http.Request) {
			if r.Header.Get("Authorization") != "Bearer at-1" {
				w.WriteHeader(401)
				return
			}
			io.WriteString(w, "hello, world\n")
		})
		srv := &http.Server{Handler: mux}
		go srv.Serve(l)
		url := "http://" + l.Addr().String() + "/hello"
		c := &http.Client{Transport: &bearerTransport{base: http.DefaultTransport, token: "at-1"}}
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			res, err := c.Get(url)
			if err == nil {
				b, _ := io.ReadAll(res.Body)
				res.Body.Close()
				check += len(b)
			}
		}
		report("oauth2_get", time.Since(t0).Seconds(), float64(n), "")
		srv.Close()
		ok = check == int(n)*13
	case "oidc_verify":
		if n == 0 {
			n = 20000
		}
		key, _ := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
		now := time.Now().Unix()
		head := base64.RawURLEncoding.EncodeToString([]byte(`{"alg":"ES256","kid":"k1","typ":"JWT"}`))
		body, _ := json.Marshal(map[string]any{"iss": "http://op", "sub": "ann", "aud": "web", "nonce": "n-1", "exp": now + 3600, "iat": now})
		signing := head + "." + base64.RawURLEncoding.EncodeToString(body)
		h := sha256.Sum256([]byte(signing))
		r, s, _ := ecdsa.Sign(rand.Reader, key, h[:])
		sig := make([]byte, 64)
		r.FillBytes(sig[:32])
		s.FillBytes(sig[32:])
		token := signing + "." + base64.RawURLEncoding.EncodeToString(sig)
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if verifyIDToken(token, &key.PublicKey, "http://op", "web", "n-1") {
				check++
			}
		}
		report("oidc_verify", time.Since(t0).Seconds(), float64(n), "")
		ok = check == int(n)
	case "auth_basic":
		if n == 0 {
			n = 1000000
		}
		hello := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { io.WriteString(w, "hello") })
		h := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			u, p, ok := r.BasicAuth()
			if !ok || u != "ann" || p != "s3cret" {
				w.Header().Set("WWW-Authenticate", `Basic realm="bench", charset="UTF-8"`)
				http.Error(w, "Unauthorized", http.StatusUnauthorized)
				return
			}
			hello.ServeHTTP(w, r)
		})
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			r := httptest.NewRequest("GET", "/hello", nil)
			r.Header.Set("Authorization", "Basic YW5uOnMzY3JldA==")
			rec := httptest.NewRecorder()
			h.ServeHTTP(rec, r)
			if rec.Code == 200 {
				check++
			}
		}
		report(what, time.Since(t0).Seconds(), float64(n), "")
		ok = check == int(n)
	case "mw_none", "mw_chain_filter", "mw_chain_around", "mw_csrf":
		if n == 0 {
			n = 1000000
		}
		var h http.Handler = http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { io.WriteString(w, "hello") })
		switch what {
		case "mw_chain_filter", "mw_chain_around":
			for k := 0; k < 10; k++ {
				next := h
				h = http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { next.ServeHTTP(w, r) })
			}
		case "mw_csrf":
			h = http.NewCrossOriginProtection().Handler(h)
		}
		post := what == "mw_csrf"
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			method := "GET"
			if post {
				method = "POST"
			}
			r := httptest.NewRequest(method, "/hello", nil)
			if post {
				r.Header.Set("Sec-Fetch-Site", "same-origin")
			}
			rec := httptest.NewRecorder()
			h.ServeHTTP(rec, r)
			if rec.Code == 200 {
				check++
			}
		}
		report(what, time.Since(t0).Seconds(), float64(n), "")
		ok = check == int(n)
	case "serve_handler_range", "serve_handler_etag":
		if n == 0 {
			n = 200000
		}
		dir, _ := os.MkdirTemp("", "sgcl_bench_serveh_")
		defer os.RemoveAll(dir)
		data := make([]byte, 1<<20)
		for i := range data {
			data[i] = byte('a' + i%26)
		}
		os.WriteFile(filepath.Join(dir, "f.bin"), data, 0644)
		h := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			f, err := os.Open(filepath.Join(dir, filepath.Clean("/"+r.URL.Path)))
			if err != nil {
				http.NotFound(w, r)
				return
			}
			defer f.Close()
			st, err := f.Stat()
			if err != nil || !st.Mode().IsRegular() {
				http.NotFound(w, r)
				return
			}
			w.Header().Set("ETag", fmt.Sprintf("W/\"%x-%x\"", st.Size(), st.ModTime().UnixNano()))
			http.ServeContent(w, r, st.Name(), st.ModTime(), f)
		})
		first := httptest.NewRecorder()
		h.ServeHTTP(first, httptest.NewRequest("GET", "/f.bin", nil))
		tag := first.Header().Get("ETag")
		etag := what == "serve_handler_etag"
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			r := httptest.NewRequest("GET", "/f.bin", nil)
			if etag {
				r.Header.Set("If-None-Match", tag)
			} else {
				r.Header.Set("Range", "bytes=1000-1999")
			}
			rec := httptest.NewRecorder()
			h.ServeHTTP(rec, r)
			if (etag && rec.Code == 304) || (!etag && rec.Code == 206) {
				check++
			}
		}
		report(what, time.Since(t0).Seconds(), float64(n), "")
		ok = check == int(n)
	case "serve_range", "serve_ranges", "serve_etag":
		if n == 0 {
			n = 50000
		}
		dir, _ := os.MkdirTemp("", "sgcl_bench_serve_")
		defer os.RemoveAll(dir)
		data := make([]byte, 1<<20)
		for i := range data {
			data[i] = byte('a' + i%26)
		}
		os.WriteFile(filepath.Join(dir, "f.bin"), data, 0644)
		l := listen()
		mux := http.NewServeMux()
		// http.ServeContent with the ETag the module makes (W/"size-mtime" in hex), set from Stat
		mux.HandleFunc("GET /{path...}", func(w http.ResponseWriter, r *http.Request) {
			f, err := os.Open(filepath.Join(dir, filepath.Clean("/"+r.PathValue("path"))))
			if err != nil {
				http.NotFound(w, r)
				return
			}
			defer f.Close()
			st, err := f.Stat()
			if err != nil || !st.Mode().IsRegular() {
				http.NotFound(w, r)
				return
			}
			w.Header().Set("ETag", fmt.Sprintf("W/\"%x-%x\"", st.Size(), st.ModTime().UnixNano()))
			http.ServeContent(w, r, st.Name(), st.ModTime(), f)
		})
		srv := &http.Server{Handler: mux}
		go srv.Serve(l)
		url := "http://" + l.Addr().String() + "/f.bin"
		c := &http.Client{}
		tag := ""
		if res, err := c.Get(url); err == nil {
			tag = res.Header.Get("ETag")
			io.ReadAll(res.Body)
			res.Body.Close()
		}
		expect, min := 206, 1000
		if what == "serve_ranges" {
			min = 200
		} else if what == "serve_etag" {
			expect, min = 304, 0
		}
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			req, _ := http.NewRequest("GET", url, nil)
			switch what {
			case "serve_range":
				req.Header.Set("Range", "bytes=1000-1999")
			case "serve_ranges":
				req.Header.Set("Range", "bytes=0-99,5000-5099")
			default:
				req.Header.Set("If-None-Match", tag)
			}
			res, err := c.Do(req)
			if err == nil {
				b, _ := io.ReadAll(res.Body)
				res.Body.Close()
				if res.StatusCode == expect && len(b) >= min {
					check++
				}
			}
		}
		report(what, time.Since(t0).Seconds(), float64(n), "")
		srv.Close()
		ok = check == int(n)
	case "dns_parse":
		ok = benchDNSParse(n)
	case "dns_https_parse":
		ok = benchDNSHTTPSParse(n)
	case "dns_lookup":
		ok = benchDNSLookup(n)
	case "proxy_get":
		if n == 0 {
			n = 50000
		}
		l := listen()
		mux := http.NewServeMux()
		mux.HandleFunc("GET /hello", func(w http.ResponseWriter, r *http.Request) { io.WriteString(w, "hello, world\n") })
		srv := &http.Server{Handler: mux}
		go srv.Serve(l)
		p := listen()
		go proxyServer(p, l.Addr().String())
		pu, _ := url.Parse("http://" + p.Addr().String())
		target := "http://" + l.Addr().String() + "/hello"
		c := &http.Client{Transport: &http.Transport{Proxy: http.ProxyURL(pu)}}
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			res, err := c.Get(target)
			if err == nil {
				b, _ := io.ReadAll(res.Body)
				res.Body.Close()
				check += len(b)
			}
		}
		report("proxy_get", time.Since(t0).Seconds(), float64(n), "")
		p.Close()
		srv.Close()
		ok = check == int(n)*13
	case "multipart_parse":
		if n == 0 {
			n = 2000
		}
		boundary := "4f3c2a1b0e9d8c7b6a5f4e3d2c1b0a99887766554433221100ffeeddccbb"
		body := multipartBody(boundary)
		buf := make([]byte, 32768)
		check, parts := 0, 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			r := multipart.NewReader(bytes.NewReader(body), boundary)
			for {
				p, err := r.NextPart()
				if err != nil {
					break
				}
				parts++
				for {
					k, err := p.Read(buf)
					check += k
					if err != nil {
						break
					}
				}
			}
		}
		wall := time.Since(t0).Seconds()
		report("multipart_parse", wall, float64(n), fmt.Sprintf(" MB/s=%.0f", float64(len(body))*float64(n)/wall/1e6))
		ok = parts == int(n)*21 && check > int(n)<<20
	case "multipart_write":
		if n == 0 {
			n = 2000
		}
		path := filepath.Join(os.TempDir(), "sgcl_bench_multipart_go.bin")
		os.WriteFile(path, multipartFile(), 0600)
		var check int64
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			cw := &countingWriter{}
			w := multipart.NewWriter(cw)
			for k := 0; k < 20; k++ {
				w.WriteField(fmt.Sprintf("field%d", k), fmt.Sprintf("value %d of the form, some text", k))
			}
			part, _ := w.CreateFormFile("upload", "data.bin")
			f, err := os.Open(path)
			if err != nil {
				break
			}
			io.CopyBuffer(part, f, make([]byte, 32768))
			f.Close()
			w.Close()
			check += cw.n
		}
		wall := time.Since(t0).Seconds()
		report("multipart_write", wall, float64(n), fmt.Sprintf(" MB/s=%.0f", float64(check)/wall/1e6))
		os.Remove(path)
		ok = check > n<<20
	case "socks5_connect":
		if n == 0 {
			n = 1000 // two connections each: RUNS of both variants inside the ephemeral ports a TIME_WAIT leaves
		}
		target := listen()
		taken := make(chan int64)
		go func() {
			var k int64
			for ; k < n; k++ {
				c, err := target.Accept()
				if err != nil {
					break
				}
				c.Close()
			}
			taken <- k
		}()
		proxy := listen()
		go socks5Server(proxy)
		to := target.Addr().(*net.TCPAddr)
		t0 := time.Now()
		var made int64
		for ; made < n; made++ {
			c, err := socks5Dial(proxy.Addr().String(), to)
			if err != nil {
				break
			}
			c.Close()
		}
		if made < n {
			target.Close()
		}
		k := <-taken
		report("socks5_connect", time.Since(t0).Seconds(), float64(n), "")
		proxy.Close()
		target.Close()
		ok = made == n && k == n
	case "udp_multicast":
		ok = benchMulticast(n)
	case "tls_handshake", "tls_resume", "tls_mtls", "tls_ech":
		if n == 0 {
			n = 2000
		}
		dir := "tests/net/tls_testdata/"
		pair, err := tls.LoadX509KeyPair(dir+"ecdsa.pem", dir+"ecdsa.key")
		if err != nil {
			panic(err)
		}
		pem, err := os.ReadFile(dir + "ca.pem")
		if err != nil {
			panic(err)
		}
		roots := x509.NewCertPool()
		roots.AppendCertsFromPEM(pem)
		curves := []tls.CurveID{tls.X25519}
		scfg := &tls.Config{Certificates: []tls.Certificate{pair}, MinVersion: tls.VersionTLS13, CurvePreferences: curves}
		ccfg := &tls.Config{RootCAs: roots, ServerName: "localhost", MinVersion: tls.VersionTLS13, CurvePreferences: curves}
		if what == "tls_resume" {
			ccfg.ClientSessionCache = tls.NewLRUClientSessionCache(64)
		}
		if what == "tls_ech" {
			config, key := echConfig("public.example")
			scfg.EncryptedClientHelloKeys = []tls.EncryptedClientHelloKey{{Config: config, PrivateKey: key, SendAsRetry: true}}
			ccfg.EncryptedClientHelloConfigList = append([]byte{byte(len(config) >> 8), byte(len(config))}, config...)
		}
		if what == "tls_mtls" {
			cpair, err := tls.LoadX509KeyPair(dir+"client_ecdsa.pem", dir+"client_ecdsa.key")
			if err != nil {
				panic(err)
			}
			scfg.ClientAuth = tls.RequireAndVerifyClientCert
			scfg.ClientCAs = roots
			ccfg.Certificates = []tls.Certificate{cpair}
		}
		l, err := tls.Listen("tcp", "127.0.0.1:0", scfg)
		if err != nil {
			panic(err)
		}
		served := make(chan int64)
		go func() {
			var ok int64
			for i := int64(0); i < n+1; i++ {
				c, err := l.Accept()
				if err != nil {
					break
				}
				go func(c net.Conn) {
					c.Write([]byte{'x'})
					c.Close()
				}(c)
				ok++
			}
			served <- ok
		}()
		dial := func(count int64) int64 {
			var ok int64
			buf := make([]byte, 1)
			for i := int64(0); i < count; i++ {
				c, err := tls.Dial("tcp", l.Addr().String(), ccfg)
				if err != nil {
					fmt.Fprintln(os.Stderr, err)
					break
				}
				if _, err := io.ReadFull(c, buf); err == nil {
					ok++
				}
				c.Close()
			}
			return ok
		}
		warm := dial(1)
		t0 := time.Now()
		done := dial(n)
		report(what, time.Since(t0).Seconds(), float64(n), "")
		got := <-served
		l.Close()
		ok = warm == 1 && done == n && got == n+1
	case "tls_stream":
		mb := n
		if mb == 0 {
			mb = 1024
		}
		bytes := mb << 20
		dir := "tests/net/tls_testdata/"
		pair, err := tls.LoadX509KeyPair(dir+"ecdsa.pem", dir+"ecdsa.key")
		if err != nil {
			panic(err)
		}
		pem, err := os.ReadFile(dir + "ca.pem")
		if err != nil {
			panic(err)
		}
		roots := x509.NewCertPool()
		roots.AppendCertsFromPEM(pem)
		curves := []tls.CurveID{tls.X25519}
		scfg := &tls.Config{Certificates: []tls.Certificate{pair}, MinVersion: tls.VersionTLS13, CurvePreferences: curves}
		ccfg := &tls.Config{RootCAs: roots, ServerName: "localhost", MinVersion: tls.VersionTLS13, CurvePreferences: curves}
		l, err := tls.Listen("tcp", "127.0.0.1:0", scfg)
		if err != nil {
			panic(err)
		}
		received := make(chan int64)
		go func() {
			c, err := l.Accept()
			if err != nil {
				received <- 0
				return
			}
			buf := make([]byte, 32768)
			var total int64
			for {
				r, err := c.Read(buf)
				total += int64(r)
				if err != nil {
					break
				}
			}
			c.Close()
			received <- total
		}()
		c, err := tls.Dial("tcp", l.Addr().String(), ccfg)
		if err != nil {
			panic(err)
		}
		buf := make([]byte, 32768)
		t0 := time.Now()
		var sent int64
		for sent < bytes {
			w, err := c.Write(buf)
			if err != nil {
				break
			}
			sent += int64(w)
		}
		c.Close()
		got := <-received
		wall := time.Since(t0).Seconds()
		report("tls_stream", wall, float64(got), fmt.Sprintf(" GB/s=%.2f", float64(got)/wall/1e9))
		l.Close()
		ok = got == sent
	case "ws_echo", "ws_throughput":
		echo := what == "ws_echo"
		if n == 0 {
			if echo {
				n = 50000
			} else {
				n = 2000
			}
		}
		l := listen()
		mux := http.NewServeMux()
		mux.HandleFunc("/ws", wsHandler(echo))
		srv := &http.Server{Handler: mux}
		go srv.Serve(l)
		ws, err := wsDial(l.Addr().String())
		if err != nil {
			panic(err)
		}
		check := 0
		t0 := time.Now()
		if echo {
			msg := bytes.Repeat([]byte("m"), 64)
			for i := int64(0); i < n; i++ {
				if ws.write(1, msg) != nil {
					break
				}
				_, p, err := ws.read()
				if err != nil {
					break
				}
				check += len(p)
			}
			report("ws_echo", time.Since(t0).Seconds(), float64(n), "")
			ok = check == int(n)*64
		} else {
			block := bytes.Repeat([]byte{0x5A}, 1<<20)
			for i := int64(0); i < n; i++ {
				if ws.write(2, block) != nil {
					break
				}
			}
			ws.write(1, []byte("end"))
			if _, p, err := ws.read(); err == nil {
				check, _ = strconv.Atoi(string(p))
			}
			wall := time.Since(t0).Seconds()
			report("ws_throughput", wall, float64(n), fmt.Sprintf(" MB/s=%.0f", float64(check)/wall/1e6))
			ok = check == int(n)<<20
		}
		ws.c.Close()
		srv.Close()
	case "sse_events":
		if n == 0 {
			n = 200000
		}
		l := listen()
		mux := http.NewServeMux()
		count := n
		mux.HandleFunc("GET /events", func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Content-Type", "text/event-stream")
			w.Header().Set("Cache-Control", "no-cache")
			f := w.(http.Flusher)
			for i := int64(0); i < count; i++ {
				fmt.Fprintf(w, "event: tick\ndata: event number %d\n\n", i)
				f.Flush()
			}
		})
		srv := &http.Server{Handler: mux}
		go srv.Serve(l)
		got := int64(0)
		t0 := time.Now()
		res, err := http.Get("http://" + l.Addr().String() + "/events")
		if err == nil {
			r := bufio.NewReader(res.Body)
			var data []string
			for {
				line, err := r.ReadString('\n')
				if err != nil {
					break
				}
				line = strings.TrimSuffix(strings.TrimSuffix(line, "\n"), "\r")
				if line == "" {
					if data != nil {
						got++
					}
					data = nil
					continue
				}
				if field, value, _ := strings.Cut(line, ":"); field == "data" {
					data = append(data, strings.TrimPrefix(value, " "))
				}
			}
			res.Body.Close()
		}
		wall := time.Since(t0).Seconds()
		report("sse_events", wall, float64(n), fmt.Sprintf(" events/s=%.0f", float64(got)/wall))
		srv.Close()
		ok = got == n
	case "tls12_server":
		dir := "tests/net/tls_testdata/"
		pair, err := tls.LoadX509KeyPair(dir+"ecdsa.pem", dir+"ecdsa.key")
		if err != nil {
			panic(err)
		}
		cfg := &tls.Config{Certificates: []tls.Certificate{pair}, MinVersion: tls.VersionTLS12, MaxVersion: tls.VersionTLS12, CurvePreferences: []tls.CurveID{tls.X25519}}
		l, err := tls.Listen("tcp", "127.0.0.1:0", cfg)
		if err != nil {
			panic(err)
		}
		fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
		os.Stdout.Sync()
		for {
			c, err := l.Accept()
			if err != nil {
				return
			}
			go func(c net.Conn) {
				c.Write([]byte{'x'})
				c.Close()
			}(c)
		}
	case "tls12_handshake", "tls12_resume":
		if n == 0 {
			n = 2000
		}
		address := os.Getenv("SGCL_TLS12_SERVER")
		if address == "" {
			fmt.Fprintln(os.Stderr, what+": SGCL_TLS12_SERVER (the address of `net tls12_server`) is not set")
			os.Exit(2)
		}
		pem, err := os.ReadFile("tests/net/tls_testdata/ca.pem")
		if err != nil {
			panic(err)
		}
		roots := x509.NewCertPool()
		roots.AppendCertsFromPEM(pem)
		ccfg := &tls.Config{RootCAs: roots, ServerName: "localhost", MinVersion: tls.VersionTLS12, CurvePreferences: []tls.CurveID{tls.X25519}}
		if what == "tls12_resume" {
			ccfg.ClientSessionCache = tls.NewLRUClientSessionCache(64)
		}
		// resumed: the abbreviated handshake required (the check made once, before the timing)
		dial := func(count int64, resumed bool) int64 {
			var done int64
			buf := make([]byte, 1)
			for i := int64(0); i < count; i++ {
				c, err := tls.Dial("tcp", address, ccfg)
				if err != nil {
					fmt.Fprintln(os.Stderr, err)
					break
				}
				if c.ConnectionState().Version != tls.VersionTLS12 {
					fmt.Fprintln(os.Stderr, "not TLS 1.2")
					break
				}
				if resumed && !c.ConnectionState().DidResume {
					fmt.Fprintln(os.Stderr, "the session was not resumed")
					break
				}
				if _, err := io.ReadFull(c, buf); err == nil {
					done++
				}
				c.Close()
			}
			return done
		}
		warm := dial(1, false)
		if what == "tls12_resume" {
			warm = dial(1, true)
		}
		t0 := time.Now()
		done := dial(n, false)
		report(what, time.Since(t0).Seconds(), float64(n), "")
		ok = warm == 1 && done == n
	case "imap_noop", "imap_fetch_flags", "imap_fetch_body", "imap_fetch_envelope", "imap_search", "imap_append", "imap_pipeline":
		ok = imapCase(what, n)
	case "cookie_jar":
		if n == 0 {
			n = 1000000
		}
		jar, _ := cookiejar.New(nil)
		sites := []string{"https://www.example.com/", "https://shop.example.co.uk/", "https://a.github.io/",
			"https://api.example.org/v1/", "https://example.net/"}
		for _, site := range sites {
			su, _ := url.Parse(site)
			for k := 0; k < 4; k++ {
				jar.SetCookies(su, []*http.Cookie{{Name: "k" + strconv.Itoa(k), Value: "v" + strconv.Itoa(k), Path: "/", MaxAge: 3600}})
			}
		}
		u, _ := url.Parse("https://www.example.com/a/b")
		response := []*http.Cookie{{Name: "session", Value: "abc123", Path: "/", Secure: true, HttpOnly: true},
			{Name: "theme", Value: "dark", Path: "/a", MaxAge: 86400}}
		want := "theme=dark; k0=v0; k1=v1; k2=v2; k3=v3; session=abc123"
		header := func() string {
			var b strings.Builder
			for i, c := range jar.Cookies(u) {
				if i > 0 {
					b.WriteString("; ")
				}
				b.WriteString(c.Name)
				b.WriteByte('=')
				b.WriteString(c.Value)
			}
			return b.String()
		}
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			jar.SetCookies(u, response)
			check += len(header())
		}
		report("cookie_jar", time.Since(t0).Seconds(), float64(n), "")
		ok = check == int(n)*len(want) && header() == want
	case "reverse_proxy_get", "reverse_proxy_stream":
		stream := what == "reverse_proxy_stream"
		if n == 0 {
			n = 50000
			if stream {
				n = 2000
			}
		}
		filler := bytes.Repeat([]byte{'r'}, 1<<20)
		l := listen()
		mux := http.NewServeMux()
		mux.HandleFunc("GET /hello", func(w http.ResponseWriter, r *http.Request) { io.WriteString(w, "hello, world\n") })
		mux.HandleFunc("GET /stream", func(w http.ResponseWriter, r *http.Request) {
			for at := 0; at < len(filler); at += 65536 {
				w.Write(filler[at : at+65536])
				w.(http.Flusher).Flush()
			}
		})
		srv := &http.Server{Handler: mux}
		go srv.Serve(l)
		backend, _ := url.Parse("http://" + l.Addr().String())
		rp := httputil.NewSingleHostReverseProxy(backend)
		t := http.DefaultTransport.(*http.Transport).Clone()
		t.Proxy = nil
		t.MaxIdleConns = 0
		t.MaxIdleConnsPerHost = 256
		rp.Transport = t
		p := listen()
		front := &http.Server{Handler: rp}
		go front.Serve(p)
		target := "http://" + p.Addr().String() + "/hello"
		if stream {
			target = "http://" + p.Addr().String() + "/stream"
		}
		c := &http.Client{Transport: &http.Transport{Proxy: nil}}
		check := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			res, err := c.Get(target)
			if err == nil {
				b, _ := io.ReadAll(res.Body)
				res.Body.Close()
				check += len(b)
			}
		}
		wall := time.Since(t0).Seconds()
		extra := ""
		if stream {
			extra = fmt.Sprintf(" MB/s=%.0f", float64(len(filler))*float64(n)/wall/1e6)
		}
		report(what, wall, float64(n), extra)
		front.Close()
		srv.Close()
		want := 13
		if stream {
			want = len(filler)
		}
		ok = check == int(n)*want
	case "dot_lookup":
		ok = benchDoT(n)
	case "doh_lookup":
		ok = benchDoH(n)
	case "mdns_parse", "mdns_build":
		ok = benchMdnsCodec(what, n)
	case "mdns_roundtrip":
		ok = benchMdnsRoundtrip(n)
	default:
		fmt.Fprintf(os.Stderr, "unknown case %s\n", what)
		os.Exit(2)
	}
	if !ok {
		os.Exit(1)
	}
}

// x/oauth2's Transport with the standard library alone: the token read under
// the source's mutex (ReuseTokenSource), the request cloned with its header
type bearerTransport struct {
	base  http.RoundTripper
	mu    sync.Mutex
	token string
}

func (t *bearerTransport) RoundTrip(r *http.Request) (*http.Response, error) {
	t.mu.Lock()
	tok := t.token
	t.mu.Unlock()
	r2 := r.Clone(r.Context())
	r2.Header.Set("Authorization", "Bearer "+tok)
	return t.base.RoundTrip(r2)
}

// An ECHConfig of RFC 9849 §4 (version 0xfe0d, id 1, DHKEM(X25519), HKDF-SHA256
// with AES-128-GCM, no longest name, no extensions) of a fresh key, and the key
func echConfig(publicName string) ([]byte, []byte) {
	k, err := ecdh.X25519().GenerateKey(rand.Reader)
	if err != nil {
		panic(err)
	}
	pub := k.PublicKey().Bytes()
	var c []byte
	c = append(c, 1, 0x00, 0x20, byte(len(pub)>>8), byte(len(pub)))
	c = append(c, pub...)
	c = append(c, 0, 4, 0, 1, 0, 1, 0, byte(len(publicName)))
	c = append(c, publicName...)
	c = append(c, 0, 0)
	out := append([]byte{0xfe, 0x0d, byte(len(c) >> 8), byte(len(c))}, c...)
	return out, k.Bytes()
}

// An ID token verified as go-oidc does, with the standard library alone: the
// compact JWS split, the header's alg and kid, ecdsa.Verify of r||s, the
// claims decoded and iss, aud, exp, iat and the nonce checked
func verifyIDToken(token string, pub *ecdsa.PublicKey, issuer, client, nonce string) bool {
	parts := strings.Split(token, ".")
	if len(parts) != 3 {
		return false
	}
	hb, err := base64.RawURLEncoding.DecodeString(parts[0])
	if err != nil {
		return false
	}
	var head struct {
		Alg string `json:"alg"`
		Kid string `json:"kid"`
	}
	if json.Unmarshal(hb, &head) != nil || head.Alg != "ES256" || head.Kid != "k1" {
		return false
	}
	sig, err := base64.RawURLEncoding.DecodeString(parts[2])
	if err != nil || len(sig) != 64 {
		return false
	}
	h := sha256.Sum256([]byte(parts[0] + "." + parts[1]))
	if !ecdsa.Verify(pub, h[:], new(big.Int).SetBytes(sig[:32]), new(big.Int).SetBytes(sig[32:])) {
		return false
	}
	cb, err := base64.RawURLEncoding.DecodeString(parts[1])
	if err != nil {
		return false
	}
	var c struct {
		Iss   string `json:"iss"`
		Sub   string `json:"sub"`
		Aud   any    `json:"aud"`
		Exp   int64  `json:"exp"`
		Iat   int64  `json:"iat"`
		Nonce string `json:"nonce"`
	}
	if json.Unmarshal(cb, &c) != nil {
		return false
	}
	aud := false
	switch a := c.Aud.(type) {
	case string:
		aud = a == client
	case []any:
		for _, x := range a {
			aud = aud || x == client
		}
	}
	now := time.Now().Unix()
	return c.Iss == issuer && aud && c.Sub != "" && c.Iat != 0 && c.Exp+60 > now && c.Nonce == nonce
}
