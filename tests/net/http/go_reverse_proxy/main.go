// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The other side of tests/net/http/reverse_proxy_interop.cpp: Go's
// net/http as a backend behind the module's reverse proxy, and as a client
// of it, on the loopback. The test builds it (go build) and runs it:
//
//	go_reverse_proxy backend       listens on 127.0.0.1:0, prints "port N", serves until GET /quit
//	go_reverse_proxy client BASE   runs the scenarios below against BASE (the proxy), one line each
//
// The backend's endpoints: /echo (the request's head and body length
// back as text); /stream (three parts, a flush each); /trailer (a body and
// a trailer, announced); /hints (103 Early Hints, then the response);
// /sse (two events); /big (8 MiB).
package main

import (
	"bufio"
	"bytes"
	"context"
	"crypto/sha256"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/http/httptrace"
	"net/textproto"
	"os"
	"sort"
	"strings"
	"sync"
	"time"
)

func backend() {
	quit := make(chan struct{})
	var once sync.Once
	mux := http.NewServeMux()
	mux.HandleFunc("/quit", func(w http.ResponseWriter, r *http.Request) {
		once.Do(func() { close(quit) })
	})
	mux.HandleFunc("/echo", func(w http.ResponseWriter, r *http.Request) {
		b, _ := io.ReadAll(r.Body)
		var keys []string
		for k := range r.Header {
			keys = append(keys, k)
		}
		sort.Strings(keys)
		fmt.Fprintf(w, "%s %s %s\n", r.Method, r.URL.RequestURI(), r.Proto)
		fmt.Fprintf(w, "Host: %s\n", r.Host)
		for _, k := range keys {
			fmt.Fprintf(w, "%s: %s\n", k, strings.Join(r.Header[k], ", "))
		}
		fmt.Fprintf(w, "body %d %x\n", len(b), sha256.Sum256(b))
	})
	mux.HandleFunc("/stream", func(w http.ResponseWriter, r *http.Request) {
		for i := 0; i < 3; i++ {
			fmt.Fprintf(w, "part %d\n", i)
			w.(http.Flusher).Flush()
			time.Sleep(10 * time.Millisecond)
		}
	})
	mux.HandleFunc("/trailer", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Trailer", "X-Sum")
		io.WriteString(w, "body")
		w.(http.Flusher).Flush()
		w.Header().Set("X-Sum", "4")
	})
	mux.HandleFunc("/hints", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Add("Link", "</style.css>; rel=preload; as=style")
		w.WriteHeader(http.StatusEarlyHints)
		w.Header().Del("Link")
		io.WriteString(w, "after hints")
	})
	mux.HandleFunc("/sse", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/event-stream")
		for i := 0; i < 2; i++ {
			fmt.Fprintf(w, "data: event %d\n\n", i)
			w.(http.Flusher).Flush()
			time.Sleep(10 * time.Millisecond)
		}
	})
	mux.HandleFunc("/big", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Length", fmt.Sprint(8<<20))
		w.Write(bytes.Repeat([]byte("0123456789abcdef"), (8<<20)/16))
	})
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		panic(err)
	}
	fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
	os.Stdout.Sync()
	go http.Serve(l, mux)
	<-quit
}

func check(name string, ok bool, why string) {
	if ok {
		fmt.Printf("ok %s\n", name)
	} else {
		fmt.Printf("fail %s: %s\n", name, why)
	}
}

func client(base string) {
	c := &http.Client{Transport: &http.Transport{Proxy: nil, MaxIdleConnsPerHost: 4}}
	// a GET through: X-Forwarded-For added, the hop-by-hop field gone
	req, _ := http.NewRequest("GET", base+"/echo?q=1", nil)
	req.Header.Set("X-Hop", "secret")
	req.Header.Set("Connection", "X-Hop")
	req.Header.Set("X-Keep", "kept")
	res, err := c.Do(req)
	if err != nil {
		check("get", false, err.Error())
	} else {
		b, _ := io.ReadAll(res.Body)
		res.Body.Close()
		s := string(b)
		check("get", res.StatusCode == 200 && strings.HasPrefix(s, "GET /echo?q=1 ") && strings.Contains(s, "X-Forwarded-For: 127.0.0.1\n") &&
			strings.Contains(s, "X-Keep: kept\n") && !strings.Contains(s, "X-Hop"), s)
	}
	// a large POST, with a length and chunked
	big := bytes.Repeat([]byte("abcdefgh"), 1<<18)
	sum := fmt.Sprintf("body %d %x\n", len(big), sha256.Sum256(big))
	for _, chunked := range []bool{false, true} {
		var body io.Reader = bytes.NewReader(big)
		if chunked {
			body = io.MultiReader(bytes.NewReader(big)) // no length known: chunked
		}
		req, _ := http.NewRequest("POST", base+"/echo", body)
		res, err := c.Do(req)
		name := fmt.Sprintf("post chunked=%v", chunked)
		if err != nil {
			check(name, false, err.Error())
			continue
		}
		b, _ := io.ReadAll(res.Body)
		res.Body.Close()
		check(name, strings.HasSuffix(string(b), sum), string(b))
	}
	// a stream read as it comes
	res, err = c.Get(base + "/stream")
	if err != nil {
		check("stream", false, err.Error())
	} else {
		r := bufio.NewReader(res.Body)
		first, _ := r.ReadString('\n')
		rest, _ := io.ReadAll(r)
		res.Body.Close()
		check("stream", first == "part 0\n" && string(rest) == "part 1\npart 2\n", first+string(rest))
	}
	// trailers
	res, err = c.Get(base + "/trailer")
	if err != nil {
		check("trailer", false, err.Error())
	} else {
		b, _ := io.ReadAll(res.Body)
		res.Body.Close()
		check("trailer", string(b) == "body" && res.Trailer.Get("X-Sum") == "4", fmt.Sprintf("%q %v", b, res.Trailer))
	}
	// big download
	res, err = c.Get(base + "/big")
	if err != nil {
		check("big", false, err.Error())
	} else {
		n, _ := io.Copy(io.Discard, res.Body)
		res.Body.Close()
		check("big", n == 8<<20 && res.ContentLength == 8<<20, fmt.Sprint(n, res.ContentLength))
	}
	// 103 Early Hints seen before the response
	var hints []string
	req, _ = http.NewRequest("GET", base+"/hints", nil)
	res, err = c.Do(req.WithContext(withHints(req, &hints)))
	if err != nil {
		check("hints", false, err.Error())
	} else {
		b, _ := io.ReadAll(res.Body)
		res.Body.Close()
		check("hints", string(b) == "after hints" && len(hints) == 1 && hints[0] == "</style.css>; rel=preload; as=style", fmt.Sprintf("%q %v", b, hints))
	}
}

// A context whose trace keeps the Link of each 103
func withHints(req *http.Request, out *[]string) context.Context {
	return httptrace.WithClientTrace(req.Context(), &httptrace.ClientTrace{
		Got1xxResponse: func(code int, h textproto.MIMEHeader) error {
			if code == http.StatusEarlyHints {
				*out = append(*out, h.Get("Link"))
			}
			return nil
		},
	})
}

func main() {
	if len(os.Args) > 1 && os.Args[1] == "backend" {
		backend()
		return
	}
	if len(os.Args) > 2 && os.Args[1] == "client" {
		client(os.Args[2])
		return
	}
	fmt.Fprintln(os.Stderr, "usage: go_reverse_proxy backend | client BASE")
	os.Exit(2)
}
