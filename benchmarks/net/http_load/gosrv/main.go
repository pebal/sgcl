package main

// Go's side of the HTTP load test: hello, world on net/http; POST /echo
// the request's body back; POST /resp?size=N the body read and dropped, a
// response of N bytes (at most 1 MB). HTTP/1.1 and h2c (by prior
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
