// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The other side of tests/net/http/go.cpp: Go's net/http as a server for
// the module's client and as a client of the module's server, on the
// loopback. The test builds it (go build) and runs it:
//
//	go_peer server          listens on 127.0.0.1:0, prints "port N", serves until GET /quit
//	go_peer client BASE     runs the scenarios below against BASE, one line each, then exits
//
// The server's endpoints: /hello; /stream (three flushes: chunked); /echo
// (the body back, and X-Length); /trailer (a chunked body with a trailer);
// /redirect/N (N redirects, then /hello); /close (Connection: close);
// /conns (how many connections the server has seen); /head (a length for
// HEAD); /cookie (a Set-Cookie).
package main

import (
	"bufio"
	"bytes"
	"crypto/sha256"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/http/httptrace"
	"os"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

func serve() {
	var conns atomic.Int64
	quit := make(chan struct{})
	var once sync.Once
	mux := http.NewServeMux()
	mux.HandleFunc("/quit", func(w http.ResponseWriter, r *http.Request) {
		once.Do(func() { close(quit) })
	})
	mux.HandleFunc("/hello", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/plain")
		io.WriteString(w, "hello from go\n")
	})
	mux.HandleFunc("/stream", func(w http.ResponseWriter, r *http.Request) {
		for i := 0; i < 3; i++ {
			fmt.Fprintf(w, "part %d\n", i)
			w.(http.Flusher).Flush()
		}
	})
	mux.HandleFunc("/echo", func(w http.ResponseWriter, r *http.Request) {
		b, _ := io.ReadAll(r.Body)
		w.Header().Set("X-Length", strconv.Itoa(len(b)))
		w.Header().Set("X-Method", r.Method)
		w.Header().Set("X-Chunked", strconv.FormatBool(len(r.TransferEncoding) > 0))
		w.Write(b)
	})
	mux.HandleFunc("/trailer", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Trailer", "X-Sum")
		io.WriteString(w, "body")
		w.(http.Flusher).Flush()
		w.Header().Set("X-Sum", "4")
	})
	mux.HandleFunc("/redirect/", func(w http.ResponseWriter, r *http.Request) {
		n, _ := strconv.Atoi(strings.TrimPrefix(r.URL.Path, "/redirect/"))
		if n <= 0 {
			http.Redirect(w, r, "/hello", http.StatusFound)
			return
		}
		http.Redirect(w, r, "/redirect/"+strconv.Itoa(n-1), http.StatusFound)
	})
	mux.HandleFunc("/close", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Connection", "close")
		io.WriteString(w, "bye")
	})
	mux.HandleFunc("/conns", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprint(w, conns.Load())
	})
	mux.HandleFunc("/head", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Length", "1234")
		if r.Method != http.MethodHead {
			w.Write(bytes.Repeat([]byte("x"), 1234))
		}
	})
	mux.HandleFunc("/cookie", func(w http.ResponseWriter, r *http.Request) {
		http.SetCookie(w, &http.Cookie{Name: "id", Value: "a b", Path: "/", MaxAge: 60, HttpOnly: true, SameSite: http.SameSiteLaxMode})
	})
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		panic(err)
	}
	s := &http.Server{Handler: mux, ConnState: func(c net.Conn, st http.ConnState) {
		if st == http.StateNew {
			conns.Add(1)
		}
	}}
	fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
	os.Stdout.Sync()
	go s.Serve(l)
	<-quit
	time.Sleep(10 * time.Millisecond)
	s.Close()
}

func client(base string) {
	var reused, fresh atomic.Int64
	trace := &httptrace.ClientTrace{GotConn: func(i httptrace.GotConnInfo) {
		if i.Reused {
			reused.Add(1)
		} else {
			fresh.Add(1)
		}
	}}
	c := &http.Client{Timeout: 10 * time.Second, Transport: &http.Transport{ExpectContinueTimeout: 5 * time.Second, MaxIdleConnsPerHost: 64}}
	do := func(req *http.Request) (*http.Response, string, error) {
		req = req.WithContext(httptrace.WithClientTrace(req.Context(), trace))
		res, err := c.Do(req)
		if err != nil {
			return nil, "", err
		}
		b, err := io.ReadAll(res.Body)
		res.Body.Close()
		return res, string(b), err
	}
	line := func(name string, format string, a ...any) {
		fmt.Printf("%s: %s\n", name, fmt.Sprintf(format, a...))
	}
	// hello, three times: one connection
	for i := 0; i < 3; i++ {
		req, _ := http.NewRequest("GET", base+"/hello", nil)
		res, body, err := do(req)
		if err != nil {
			line("hello", "error %v", err)
			continue
		}
		_, dateErr := http.ParseTime(res.Header.Get("Date"))
		line("hello", "%d %q %s date=%v", res.StatusCode, body, res.Header.Get("Content-Type"), dateErr == nil)
	}
	line("reuse", "fresh=%d reused=%d", fresh.Load(), reused.Load())
	// a body of 1 MB, with its length and chunked
	big := bytes.Repeat([]byte("0123456789abcdef"), 1<<16)
	sum := sha256.Sum256(big)
	for _, chunked := range []bool{false, true} {
		var body io.Reader = bytes.NewReader(big)
		if chunked {
			body = io.MultiReader(bytes.NewReader(big))
		}
		req, _ := http.NewRequest("POST", base+"/echo", body)
		if chunked {
			req.ContentLength = -1
		}
		res, got, err := do(req)
		if err != nil {
			line("echo", "error %v", err)
			continue
		}
		line("echo", "%d chunked=%v same=%v", res.StatusCode, chunked, sha256.Sum256([]byte(got)) == sum)
	}
	// 100-continue
	req, _ := http.NewRequest("PUT", base+"/echo", strings.NewReader("continued"))
	req.Header.Set("Expect", "100-continue")
	res, got, err := do(req)
	if err != nil {
		line("continue", "error %v", err)
	} else {
		line("continue", "%d %q", res.StatusCode, got)
	}
	// HEAD, a status, a stream, a redirect
	req, _ = http.NewRequest("HEAD", base+"/hello", nil)
	res, got, err = do(req)
	if err == nil {
		line("head", "%d %d %q", res.StatusCode, res.ContentLength, got)
	}
	req, _ = http.NewRequest("GET", base+"/missing", nil)
	res, got, err = do(req)
	if err == nil {
		line("missing", "%d %q", res.StatusCode, got)
	}
	req, _ = http.NewRequest("GET", base+"/stream", nil)
	res, got, err = do(req)
	if err == nil {
		line("stream", "%d %v %q", res.StatusCode, res.TransferEncoding, got)
	}
	req, _ = http.NewRequest("GET", base+"/go-away", nil)
	res, got, err = do(req)
	if err == nil {
		line("redirect", "%d %q %s", res.StatusCode, got, res.Request.URL.Path)
	}
	// many at once: 32 goroutines, 25 requests each
	var wg sync.WaitGroup
	var wrong atomic.Int64
	for g := 0; g < 32; g++ {
		wg.Add(1)
		go func(g int) {
			defer wg.Done()
			for i := 0; i < 25; i++ {
				want := fmt.Sprintf("%d-%d", g, i)
				req, _ := http.NewRequest("POST", base+"/echo", strings.NewReader(want))
				res, got, err := do(req)
				if err != nil || res.StatusCode != 200 || got != want {
					wrong.Add(1)
				}
			}
		}(g)
	}
	wg.Wait()
	line("parallel", "wrong=%d", wrong.Load())
}

func main() {
	if len(os.Args) >= 2 && os.Args[1] == "server" {
		serve()
		return
	}
	if len(os.Args) >= 3 && os.Args[1] == "client" {
		client(os.Args[2])
		return
	}
	fmt.Fprintln(os.Stderr, "usage: go_peer server | go_peer client BASE")
	bufio.NewWriter(os.Stderr).Flush()
	os.Exit(2)
}
