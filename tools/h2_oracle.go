// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go side of the HTTP/2 interoperability tests: net/http of Go's standard
// library (its own h2, chosen by http.Protocols), as an oracle only. Built by
// the tests with `go build`; no key log, no module outside the standard
// library.
//
// Server (the default): listens on -addr (127.0.0.1:0), prints "LISTEN
// <port>", serves until SIGINT/SIGTERM (or, with -stdin, until its standard
// input ends), then shuts down gracefully (a GOAWAY on every h2 connection)
// and prints "SHUTDOWN". With -cert and -key it is TLS (ALPN from
// -protocols, "h1,h2" by default); without them plain TCP ("h1,h2c": HTTP/1.1
// and h2 by prior knowledge on one port). -max-streams sets
// SETTINGS_MAX_CONCURRENT_STREAMS; -goaway-after N answers the N-th request
// of each connection with a GOAWAY (Go's server sends one for a response
// with "Connection: close").
//
// Its endpoints (every one takes ?delay=<ms>, a sleep before the answer, to
// hold streams open side by side):
//
//	/echo     the request's body back, the status from ?status=; headers
//	          x-proto, x-alpn, x-method, x-body-bytes and x-req-<name> for
//	          every field of the request; the request's trailers as the
//	          response's trailers
//	/stream   ?n=K&size=S&pause=<ms>: K pieces of S bytes, each flushed (a
//	          DATA frame at least), byte i of the body = i mod 251
//	/size     the body read whole: "bytes=<n> crc32=<8 hex>\n"
//	/stats    "conns=<accepted> open=<open> requests=<n> inflight_max=<n>\n"
//	          (the /stats request itself counted)
//	/goaway   answers "goaway\n" with a GOAWAY on its connection
//	/abort    ?after=<bytes>: that many bytes flushed, then the stream reset
//	          (RST_STREAM INTERNAL_ERROR in h2, the connection cut in 1.1)
//
// Client (-connect host:port): -n requests, -seq one after another,
// otherwise all at once (on one connection while the server's
// MAX_CONCURRENT_STREAMS allows, more beyond it; Go 1.27's
// StrictMaxConcurrentRequests is left out: it leaves some requests waiting
// until their timeout). -method, -path, -body <bytes> (byte i = i mod 251);
// TLS with -ca and -servername, or h2 by prior knowledge with -protocols
// h2c. -alpn overrides the protocols offered in the handshake (none when
// the Transport has only HTTP/1.1 and -alpn is empty); -trailer name=value
// sends a trailer after the body; -print prints each body's lines as
// "BODY <i> <line>" after its RESP line. It prints "SENT bytes=<n>
// crc32=<hex>" when there is a body, then per request in order either
// "RESP <i> <status> <proto> <alpn or -> bytes=<n> crc32=<hex>
// [trailer:<name>=<value>...] conn=<k>" (k: which of the connections used,
// from 0) or "ERROR <i> <text>", then "CONNS <connections used>" and
// "DONE"; the exit status is 1 when a request failed.
package main

import (
	"bytes"
	"context"
	"crypto/tls"
	"crypto/x509"
	"flag"
	"fmt"
	"hash/crc32"
	"io"
	"net"
	"net/http"
	"net/http/httptrace"
	"os"
	"os/signal"
	"sort"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"syscall"
	"time"
)

// pattern writes n bytes of the body pattern (i mod 251) from offset at
func pattern(at, n int) []byte {
	b := make([]byte, n)
	for i := range b {
		b[i] = byte((at + i) % 251)
	}
	return b
}

func protocols(list string) *http.Protocols {
	p := new(http.Protocols)
	for _, s := range strings.Split(list, ",") {
		switch strings.TrimSpace(s) {
		case "h1":
			p.SetHTTP1(true)
		case "h2":
			p.SetHTTP2(true)
		case "h2c":
			p.SetUnencryptedHTTP2(true)
		case "":
		default:
			fmt.Println("ERROR unknown protocol", s)
			os.Exit(2)
		}
	}
	return p
}

func millis(r *http.Request, name string) time.Duration {
	v, _ := strconv.Atoi(r.URL.Query().Get(name))
	return time.Duration(v) * time.Millisecond
}

// --- server ------------------------------------------------------------------

type connKey struct{}

// what a connection counts: its requests, for -goaway-after
type connState struct {
	requests atomic.Int64
}

type counters struct {
	conns, open, requests, inflight, inflightMax atomic.Int64
}

func (c *counters) enter() {
	c.requests.Add(1)
	n := c.inflight.Add(1)
	for {
		m := c.inflightMax.Load()
		if n <= m || c.inflightMax.CompareAndSwap(m, n) {
			return
		}
	}
}

func serve(addr, cert, key, protos string, maxStreams, goawayAfter int, stdin bool) {
	var st counters
	mux := http.NewServeMux()
	mux.HandleFunc("/echo", func(w http.ResponseWriter, r *http.Request) {
		b, _ := io.ReadAll(r.Body)
		h := w.Header()
		h.Set("x-proto", r.Proto)
		alpn := "-"
		if r.TLS != nil && r.TLS.NegotiatedProtocol != "" {
			alpn = r.TLS.NegotiatedProtocol
		}
		h.Set("x-alpn", alpn)
		h.Set("x-method", r.Method)
		h.Set("x-body-bytes", strconv.Itoa(len(b)))
		for name, vs := range r.Header {
			h.Set("x-req-"+strings.ToLower(name), strings.Join(vs, ", "))
		}
		if ct := r.Header.Get("Content-Type"); ct != "" {
			h.Set("Content-Type", ct)
		}
		status, _ := strconv.Atoi(r.URL.Query().Get("status"))
		if status == 0 {
			status = 200
		}
		w.WriteHeader(status)
		w.Write(b)
		for name, vs := range r.Trailer {
			h.Set(http.TrailerPrefix+name, strings.Join(vs, ", "))
		}
	})
	mux.HandleFunc("/stream", func(w http.ResponseWriter, r *http.Request) {
		n, _ := strconv.Atoi(r.URL.Query().Get("n"))
		size, _ := strconv.Atoi(r.URL.Query().Get("size"))
		pause := millis(r, "pause")
		w.Header().Set("Content-Type", "application/octet-stream")
		f := w.(http.Flusher)
		for i := 0; i < n; i++ {
			if _, err := w.Write(pattern(i*size, size)); err != nil {
				return
			}
			f.Flush()
			if pause > 0 {
				time.Sleep(pause)
			}
		}
	})
	mux.HandleFunc("/size", func(w http.ResponseWriter, r *http.Request) {
		h := crc32.NewIEEE()
		n, _ := io.Copy(h, r.Body)
		fmt.Fprintf(w, "bytes=%d crc32=%08x\n", n, h.Sum32())
	})
	mux.HandleFunc("/stats", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "conns=%d open=%d requests=%d inflight_max=%d\n",
			st.conns.Load(), st.open.Load(), st.requests.Load(), st.inflightMax.Load())
	})
	mux.HandleFunc("/goaway", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Connection", "close")
		io.WriteString(w, "goaway\n")
	})
	mux.HandleFunc("/abort", func(w http.ResponseWriter, r *http.Request) {
		after, _ := strconv.Atoi(r.URL.Query().Get("after"))
		if after > 0 {
			w.Write(pattern(0, after))
			w.(http.Flusher).Flush()
		}
		panic(http.ErrAbortHandler)
	})
	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		st.enter()
		defer st.inflight.Add(-1)
		if d := millis(r, "delay"); d > 0 {
			time.Sleep(d)
		}
		if goawayAfter > 0 {
			if c, ok := r.Context().Value(connKey{}).(*connState); ok && c.requests.Add(1) == int64(goawayAfter) {
				w.Header().Set("Connection", "close")
			}
		}
		mux.ServeHTTP(w, r)
	})
	srv := &http.Server{
		Handler: handler,
		ConnContext: func(ctx context.Context, c net.Conn) context.Context {
			return context.WithValue(ctx, connKey{}, new(connState))
		},
		ConnState: func(c net.Conn, s http.ConnState) {
			switch s {
			case http.StateNew:
				st.conns.Add(1)
				st.open.Add(1)
			case http.StateClosed, http.StateHijacked:
				st.open.Add(-1)
			}
		},
		HTTP2: &http.HTTP2Config{MaxConcurrentStreams: maxStreams},
	}
	tlsOn := cert != "" || key != ""
	if protos == "" {
		if tlsOn {
			protos = "h1,h2"
		} else {
			protos = "h1,h2c"
		}
	}
	srv.Protocols = protocols(protos)
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	fmt.Println("LISTEN", ln.Addr().(*net.TCPAddr).Port)
	done := make(chan error, 1)
	go func() {
		if tlsOn {
			done <- srv.ServeTLS(ln, cert, key)
		} else {
			done <- srv.Serve(ln)
		}
	}()
	stop := make(chan os.Signal, 1)
	signal.Notify(stop, syscall.SIGINT, syscall.SIGTERM)
	if stdin {
		go func() {
			io.Copy(io.Discard, os.Stdin)
			stop <- syscall.SIGTERM
		}()
	}
	select {
	case <-stop:
	case err := <-done:
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	srv.Shutdown(ctx)
	fmt.Println("SHUTDOWN")
}

// --- client ------------------------------------------------------------------

type result struct {
	line string
	body []string
	conn string
	err  bool
}

func client(addr, ca, name, protos, alpn, method, path, trailer string, n, body int, seq, show bool) {
	tr := &http.Transport{ForceAttemptHTTP2: true}
	scheme := "http"
	if ca != "" {
		pem, err := os.ReadFile(ca)
		if err != nil {
			fmt.Println("ERROR", err)
			os.Exit(1)
		}
		roots := x509.NewCertPool()
		roots.AppendCertsFromPEM(pem)
		tr.TLSClientConfig = &tls.Config{RootCAs: roots, ServerName: name}
		if alpn != "" {
			tr.TLSClientConfig.NextProtos = strings.Split(alpn, ",")
		}
		scheme = "https"
		if protos == "" {
			protos = "h1,h2"
		}
	} else if protos == "" {
		protos = "h2c"
	}
	tr.Protocols = protocols(protos)
	defer tr.CloseIdleConnections()
	c := &http.Client{Transport: tr, Timeout: 60 * time.Second}
	url := scheme + "://" + addr + path
	var payload []byte
	if body > 0 {
		payload = pattern(0, body)
		fmt.Printf("SENT bytes=%d crc32=%08x\n", body, crc32.ChecksumIEEE(payload))
	}
	results := make([]result, n)
	one := func(i int) {
		var local string
		trace := &httptrace.ClientTrace{GotConn: func(info httptrace.GotConnInfo) {
			local = info.Conn.LocalAddr().String()
		}}
		var rd io.Reader
		if payload != nil {
			rd = bytes.NewReader(payload)
		}
		req, err := http.NewRequestWithContext(httptrace.WithClientTrace(context.Background(), trace), method, url, rd)
		if err != nil {
			results[i] = result{line: fmt.Sprintf("ERROR %d %v", i, err), err: true}
			return
		}
		if k, v, ok := strings.Cut(trailer, "="); ok {
			req.Trailer = http.Header{http.CanonicalHeaderKey(k): {v}}
		}
		resp, err := c.Do(req)
		if err != nil {
			results[i] = result{line: fmt.Sprintf("ERROR %d %v", i, err), err: true}
			return
		}
		h := crc32.NewIEEE()
		var text bytes.Buffer
		var sink io.Writer = h
		if show {
			sink = io.MultiWriter(h, &text)
		}
		got, err := io.Copy(sink, resp.Body)
		resp.Body.Close()
		if err != nil {
			results[i] = result{line: fmt.Sprintf("ERROR %d %v", i, err), err: true, conn: local}
			return
		}
		alpn := "-"
		if resp.TLS != nil && resp.TLS.NegotiatedProtocol != "" {
			alpn = resp.TLS.NegotiatedProtocol
		}
		line := fmt.Sprintf("RESP %d %d %s %s bytes=%d crc32=%08x", i, resp.StatusCode, resp.Proto, alpn, got, h.Sum32())
		names := make([]string, 0, len(resp.Trailer))
		for k := range resp.Trailer {
			names = append(names, k)
		}
		sort.Strings(names)
		for _, k := range names {
			line += fmt.Sprintf(" trailer:%s=%s", strings.ToLower(k), strings.Join(resp.Trailer[k], ", "))
		}
		results[i] = result{line: line, conn: local}
		if show {
			results[i].body = strings.Split(strings.TrimSuffix(text.String(), "\n"), "\n")
		}
	}
	if seq {
		for i := 0; i < n; i++ {
			one(i)
		}
	} else {
		var wg sync.WaitGroup
		for i := 0; i < n; i++ {
			wg.Add(1)
			go func(i int) {
				defer wg.Done()
				one(i)
			}(i)
		}
		wg.Wait()
	}
	// the connections numbered in the order of the requests that used them
	index := map[string]int{}
	failed := false
	for _, r := range results {
		if r.conn != "" {
			if _, ok := index[r.conn]; !ok {
				index[r.conn] = len(index)
			}
		}
	}
	for _, r := range results {
		failed = failed || r.err
		if r.err {
			fmt.Println(r.line)
		} else {
			fmt.Printf("%s conn=%d\n", r.line, index[r.conn])
			for _, l := range r.body {
				fmt.Printf("BODY %s %s\n", strings.Fields(r.line)[1], l)
			}
		}
	}
	fmt.Println("CONNS", len(index))
	fmt.Println("DONE")
	if failed {
		os.Exit(1)
	}
}

func main() {
	addr := flag.String("addr", "127.0.0.1:0", "server: the address to listen on")
	cert := flag.String("cert", "", "server: certificate chain (PEM); TLS when given")
	key := flag.String("key", "", "server: private key (PEM)")
	protos := flag.String("protocols", "", "comma-separated h1, h2, h2c (default: h1,h2 over TLS; h1,h2c for a plain server, h2c for a plain client)")
	maxStreams := flag.Int("max-streams", 0, "server: SETTINGS_MAX_CONCURRENT_STREAMS (0: Go's default)")
	goawayAfter := flag.Int("goaway-after", 0, "server: a GOAWAY with the N-th response of each connection")
	stdin := flag.Bool("stdin", false, "server: shut down when standard input ends")
	connect := flag.String("connect", "", "client mode: host:port to connect to")
	ca := flag.String("ca", "", "client: the roots (PEM); TLS when given")
	name := flag.String("servername", "localhost", "client: the server name")
	method := flag.String("method", "GET", "client: the method")
	path := flag.String("path", "/echo", "client: the path and query")
	n := flag.Int("n", 1, "client: the number of requests")
	body := flag.Int("body", 0, "client: the body's size in bytes")
	seq := flag.Bool("seq", false, "client: the requests one after another")
	trailer := flag.String("trailer", "", "client: name=value, a trailer sent after the body")
	alpn := flag.String("alpn", "", "client: comma-separated ALPN protocols offered (default: from -protocols)")
	show := flag.Bool("print", false, "client: print the bodies")
	flag.Parse()
	if *connect != "" {
		client(*connect, *ca, *name, *protos, *alpn, *method, *path, *trailer, *n, *body, *seq, *show)
		return
	}
	serve(*addr, *cert, *key, *protos, *maxStreams, *goawayAfter, *stdin)
}
