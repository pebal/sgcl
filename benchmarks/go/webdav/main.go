// WebDAV's three exchanges with Go's standard library alone: net/http's
// server (GET by http.FileServer, PUT and PROPFIND by hand, the multistatus
// written with encoding/xml) and net/http's client reading it — the cases of
// benchmarks/net/webdav.cpp. Prints one line, ns per operation.
//
//	serve DIR                 the server of DIR under /dav on 127.0.0.1: prints "port N"
//	webdav_get ADDR [n]       GET of a file of 64 KB: per request
//	webdav_put ADDR [n]       PUT of 64 KB: per request
//	webdav_propfind ADDR [n]  PROPFIND, Depth 1, of 100 files, read by encoding/xml: per request
package main

import (
	"bytes"
	"encoding/xml"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

func report(what string, d time.Duration, n int) {
	fmt.Printf("webdav %s ns/op=%.1f ops/s=%.0f wall=%.2fs\n", what, float64(d.Nanoseconds())/float64(n), float64(n)/d.Seconds(), d.Seconds())
}

func count(i, def int) int {
	if len(os.Args) > i {
		if n, err := strconv.Atoi(os.Args[i]); err == nil {
			return n
		}
	}
	return def
}

type prop struct {
	ResourceType *struct {
		Collection *struct{} `xml:"DAV: collection"`
	} `xml:"DAV: resourcetype"`
	Length   string `xml:"DAV: getcontentlength,omitempty"`
	Modified string `xml:"DAV: getlastmodified,omitempty"`
	ETag     string `xml:"DAV: getetag,omitempty"`
}

type response struct {
	Href     string `xml:"DAV: href"`
	Propstat struct {
		Prop   prop   `xml:"DAV: prop"`
		Status string `xml:"DAV: status"`
	} `xml:"DAV: propstat"`
}

type multistatus struct {
	XMLName   xml.Name   `xml:"DAV: multistatus"`
	Responses []response `xml:"DAV: response"`
}

func entry(href string, fi os.FileInfo) response {
	var r response
	r.Href = (&url.URL{Path: href}).EscapedPath()
	r.Propstat.Status = "HTTP/1.1 200 OK"
	r.Propstat.Prop.ResourceType = &struct {
		Collection *struct{} `xml:"DAV: collection"`
	}{}
	if fi.IsDir() {
		r.Propstat.Prop.ResourceType.Collection = &struct{}{}
	} else {
		r.Propstat.Prop.Length = strconv.FormatInt(fi.Size(), 10)
	}
	r.Propstat.Prop.Modified = fi.ModTime().UTC().Format(http.TimeFormat)
	r.Propstat.Prop.ETag = fmt.Sprintf("\"%x-%x\"", fi.Size(), fi.ModTime().Unix())
	return r
}

func serve(dir string) {
	files := http.StripPrefix("/dav", http.FileServer(http.Dir(dir)))
	http.HandleFunc("/dav/", func(w http.ResponseWriter, r *http.Request) {
		rel := filepath.Join(dir, filepath.FromSlash(strings.TrimPrefix(r.URL.Path, "/dav")))
		switch r.Method {
		case "GET", "HEAD":
			files.ServeHTTP(w, r)
		case "PUT":
			f, err := os.Create(rel)
			if err != nil {
				w.WriteHeader(409)
				return
			}
			io.Copy(f, r.Body)
			f.Close()
			w.WriteHeader(204)
		case "PROPFIND":
			io.Copy(io.Discard, r.Body)
			fi, err := os.Stat(rel)
			if err != nil {
				w.WriteHeader(404)
				return
			}
			ms := multistatus{Responses: []response{entry(r.URL.Path, fi)}}
			if fi.IsDir() && r.Header.Get("Depth") == "1" {
				des, _ := os.ReadDir(rel)
				for _, de := range des {
					if info, err := de.Info(); err == nil {
						ms.Responses = append(ms.Responses, entry(strings.TrimSuffix(r.URL.Path, "/")+"/"+de.Name(), info))
					}
				}
			}
			out, _ := xml.Marshal(ms)
			w.Header().Set("Content-Type", "application/xml; charset=utf-8")
			w.WriteHeader(207)
			w.Write([]byte(xml.Header))
			w.Write(out)
		default:
			w.WriteHeader(405)
		}
	})
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		panic(err)
	}
	fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
	os.Stdout.Sync()
	http.Serve(l, nil)
}

func main() {
	if os.Args[1] == "serve" {
		serve(os.Args[2])
		return
	}
	what, addr := os.Args[1], os.Args[2]
	base := "http://" + addr + "/dav"
	c := &http.Client{}
	body := bytes.Repeat([]byte("w"), 65536)
	switch what {
	case "webdav_get":
		n := count(3, 20000)
		t0 := time.Now()
		for i := 0; i < n; i++ {
			r, err := c.Get(base + "/data.bin")
			if err != nil {
				panic(err)
			}
			b, _ := io.ReadAll(r.Body)
			r.Body.Close()
			if len(b) != 65536 {
				panic("size")
			}
		}
		report(what, time.Since(t0), n)
	case "webdav_put":
		n := count(3, 10000)
		t0 := time.Now()
		for i := 0; i < n; i++ {
			req, _ := http.NewRequest("PUT", base+"/put.bin", bytes.NewReader(body))
			r, err := c.Do(req)
			if err != nil {
				panic(err)
			}
			io.Copy(io.Discard, r.Body)
			r.Body.Close()
		}
		report(what, time.Since(t0), n)
	case "webdav_propfind":
		n := count(3, 5000)
		q := `<?xml version="1.0" encoding="utf-8"?><D:propfind xmlns:D="DAV:"><D:prop><D:resourcetype/><D:getcontentlength/><D:getlastmodified/><D:getetag/></D:prop></D:propfind>`
		t0 := time.Now()
		for i := 0; i < n; i++ {
			req, _ := http.NewRequest("PROPFIND", base+"/many/", strings.NewReader(q))
			req.Header.Set("Depth", "1")
			r, err := c.Do(req)
			if err != nil {
				panic(err)
			}
			var ms multistatus
			if err := xml.NewDecoder(r.Body).Decode(&ms); err != nil || len(ms.Responses) != 101 {
				panic(fmt.Sprint("propfind ", err, len(ms.Responses)))
			}
			r.Body.Close()
		}
		report(what, time.Since(t0), n)
	}
}
