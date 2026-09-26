//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

namespace sgcl::net::http {
    // The status codes of the IANA registry (RFC 9110 §15 and the RFCs
    // it lists), as Go's http.StatusOK...: plain ints, since a status is
    // a number on the wire and a program compares it with one
    namespace status {
        inline constexpr int continue_ = 100;                  // "continue" is a keyword
        inline constexpr int switching_protocols = 101;
        inline constexpr int processing = 102;
        inline constexpr int early_hints = 103;

        inline constexpr int ok = 200;
        inline constexpr int created = 201;
        inline constexpr int accepted = 202;
        inline constexpr int non_authoritative_information = 203;
        inline constexpr int no_content = 204;
        inline constexpr int reset_content = 205;
        inline constexpr int partial_content = 206;
        inline constexpr int multi_status = 207;
        inline constexpr int already_reported = 208;
        inline constexpr int im_used = 226;

        inline constexpr int multiple_choices = 300;
        inline constexpr int moved_permanently = 301;
        inline constexpr int found = 302;
        inline constexpr int see_other = 303;
        inline constexpr int not_modified = 304;
        inline constexpr int use_proxy = 305;
        inline constexpr int temporary_redirect = 307;
        inline constexpr int permanent_redirect = 308;

        inline constexpr int bad_request = 400;
        inline constexpr int unauthorized = 401;
        inline constexpr int payment_required = 402;
        inline constexpr int forbidden = 403;
        inline constexpr int not_found = 404;
        inline constexpr int method_not_allowed = 405;
        inline constexpr int not_acceptable = 406;
        inline constexpr int proxy_authentication_required = 407;
        inline constexpr int request_timeout = 408;
        inline constexpr int conflict = 409;
        inline constexpr int gone = 410;
        inline constexpr int length_required = 411;
        inline constexpr int precondition_failed = 412;
        inline constexpr int content_too_large = 413;
        inline constexpr int payload_too_large = 413;          // the name before RFC 9110
        inline constexpr int uri_too_long = 414;
        inline constexpr int unsupported_media_type = 415;
        inline constexpr int range_not_satisfiable = 416;
        inline constexpr int expectation_failed = 417;
        inline constexpr int im_a_teapot = 418;
        inline constexpr int misdirected_request = 421;
        inline constexpr int unprocessable_content = 422;
        inline constexpr int locked = 423;
        inline constexpr int failed_dependency = 424;
        inline constexpr int too_early = 425;
        inline constexpr int upgrade_required = 426;
        inline constexpr int precondition_required = 428;
        inline constexpr int too_many_requests = 429;
        inline constexpr int request_header_fields_too_large = 431;
        inline constexpr int header_fields_too_large = 431;
        inline constexpr int unavailable_for_legal_reasons = 451;

        inline constexpr int internal_server_error = 500;
        inline constexpr int not_implemented = 501;
        inline constexpr int bad_gateway = 502;
        inline constexpr int service_unavailable = 503;
        inline constexpr int gateway_timeout = 504;
        inline constexpr int http_version_not_supported = 505;
        inline constexpr int variant_also_negotiates = 506;
        inline constexpr int insufficient_storage = 507;
        inline constexpr int loop_detected = 508;
        inline constexpr int not_extended = 510;
        inline constexpr int network_authentication_required = 511;
    }

    // The reason phrase of a status, as the registry names it: "Not
    // Found"; "" for a code the registry does not have (Go's StatusText)
    constexpr const char* reason(int code) noexcept {
        switch (code) {
            case 100: return "Continue";
            case 101: return "Switching Protocols";
            case 102: return "Processing";
            case 103: return "Early Hints";
            case 200: return "OK";
            case 201: return "Created";
            case 202: return "Accepted";
            case 203: return "Non-Authoritative Information";
            case 204: return "No Content";
            case 205: return "Reset Content";
            case 206: return "Partial Content";
            case 207: return "Multi-Status";
            case 208: return "Already Reported";
            case 226: return "IM Used";
            case 300: return "Multiple Choices";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 303: return "See Other";
            case 304: return "Not Modified";
            case 305: return "Use Proxy";
            case 307: return "Temporary Redirect";
            case 308: return "Permanent Redirect";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 402: return "Payment Required";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 406: return "Not Acceptable";
            case 407: return "Proxy Authentication Required";
            case 408: return "Request Timeout";
            case 409: return "Conflict";
            case 410: return "Gone";
            case 411: return "Length Required";
            case 412: return "Precondition Failed";
            case 413: return "Content Too Large";
            case 414: return "URI Too Long";
            case 415: return "Unsupported Media Type";
            case 416: return "Range Not Satisfiable";
            case 417: return "Expectation Failed";
            case 418: return "I'm a teapot";
            case 421: return "Misdirected Request";
            case 422: return "Unprocessable Content";
            case 423: return "Locked";
            case 424: return "Failed Dependency";
            case 425: return "Too Early";
            case 426: return "Upgrade Required";
            case 428: return "Precondition Required";
            case 429: return "Too Many Requests";
            case 431: return "Request Header Fields Too Large";
            case 451: return "Unavailable For Legal Reasons";
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            case 504: return "Gateway Timeout";
            case 505: return "HTTP Version Not Supported";
            case 506: return "Variant Also Negotiates";
            case 507: return "Insufficient Storage";
            case 508: return "Loop Detected";
            case 510: return "Not Extended";
            case 511: return "Network Authentication Required";
            default: return "";
        }
    }
}
