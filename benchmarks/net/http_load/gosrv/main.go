package main

// Go's side of the HTTP load test: hello, world on net/http; POST /echo
// the request's body back; POST /resp?size=N the body read and dropped, a
// response of N bytes (at most 1 MB); GET /stream?size=N&parts=K N
// bytes flushed in K pieces; GET /file the file named by
// HTTP_LOAD_FILE (http.ServeFile). HTTP/1.1 and h2c (by prior
// knowledge, http.Protocols) on the plain port; with a certificate and its
// key, https (crypto/tls, TLS 1.3, ALPN h2 and http/1.1).
//
//	gosrv [address=:18081 [certificate.pem key.pem]]
import (
	"bytes"
	"io"
	"net/http"
	"os"
	"strconv"
)

func main() {
	addr := ":18081"
	if len(os.Args) > 1 {
		addr = os.Args[1]
	}
	filler := bytes.Repeat([]byte{'r'}, 1<<20)
	mux := http.NewServeMux()
	mux.HandleFunc("GET /", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/plain")
		w.Write([]byte("hello, world\n"))
	})
	mux.HandleFunc("POST /echo", func(w http.ResponseWriter, r *http.Request) {
		b, _ := io.ReadAll(r.Body)
		w.Write(b)
	})
	mux.HandleFunc("POST /resp", func(w http.ResponseWriter, r *http.Request) {
		io.Copy(io.Discard, r.Body)
		n := 1024
		if s := r.URL.Query().Get("size"); s != "" {
			if v, err := strconv.Atoi(s); err == nil && v >= 0 && v <= len(filler) {
				n = v
			}
		}
		w.Write(filler[:n])
	})
	// GET /stream?size=N&parts=K: N bytes in K pieces, each flushed (chunked)
	mux.HandleFunc("GET /stream", func(w http.ResponseWriter, r *http.Request) {
		n, parts := 65536, 4
		if v, err := strconv.Atoi(r.URL.Query().Get("size")); err == nil && v >= 0 && v <= len(filler) {
			n = v
		}
		if v, err := strconv.Atoi(r.URL.Query().Get("parts")); err == nil && v > 0 {
			parts = v
		}
		w.Header().Set("Content-Type", "application/octet-stream")
		f := w.(http.Flusher)
		at := 0
		for k := 0; k < parts; k++ {
			piece := n / parts
			if k+1 == parts {
				piece = n - at
			}
			w.Write(filler[at : at+piece])
			at += piece
			f.Flush()
		}
	})
	// GET /file: the file named by HTTP_LOAD_FILE (net/http's ServeFile:
	// sendfile over TCP)
	file := os.Getenv("HTTP_LOAD_FILE")
	mux.HandleFunc("GET /file", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/octet-stream")
		http.ServeFile(w, r, file)
	})
	p := new(http.Protocols)
	p.SetHTTP1(true)
	p.SetHTTP2(true)
	p.SetUnencryptedHTTP2(true)
	srv := &http.Server{Addr: addr, Handler: mux, Protocols: p}
	if len(os.Args) > 3 {
		srv.ListenAndServeTLS(os.Args[2], os.Args[3])
		return
	}
	srv.ListenAndServe()
}
