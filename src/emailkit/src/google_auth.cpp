#include <emailkit/global.hpp>
#include "google_auth.hpp"
#include <fmt/format.h>
#include <asio/ts/internet.hpp>
#include <emailkit/http_srv.hpp>
#include <emailkit/log.hpp>
#include <folly/folly_uri.hpp>
#include "uri_codec.hpp"

// TODO: refactor to http client.
#include <asio/read_until.hpp>
#include <asio/ssl.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>

#include <optional>
#include <regex>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

namespace emailkit {

namespace {
bool launch_system_browser(std::string uri) {
    // https://blog.kowalczyk.info/article/j/guide-to-predefined-macros-in-c-compilers-gcc-clang-msvc-etc..html

#if defined(__linux__)
    // TODO: see more variants if this does not work
    // https://www.baeldung.com/linux/open-url-in-default-browser
    return std::system(fmt::format("xdg-open {} > /dev/null 2>&1", uri).c_str()) == 0;

#elif defined(__APPLE__)
    return std::system(fmt::format("open {}", uri).c_str()) == 0;
#elif defined(__WIN32)
    return std::system(fmt::format("start {}", uri).c_str()) == 0;
#endif
}

expected<std::string> parse_userinfo_reply(const std::string& userinfo_json) {
    rapidjson::Document d;
    d.Parse(userinfo_json.c_str());
    if (d.HasParseError()) {
        log_error("JSON parse error: '{}'", userinfo_json);
        return unexpected(make_error_code(std::errc::io_error));
    }

    auto as_str_or = [&d](const char* key, std::string _default) -> std::string {
        return d.HasMember(key) && d[key].IsString() ? d[key].GetString() : _default;
    };

    if (d.HasMember("email") && d["email"].IsString()) {
        return d["email"].GetString();
    } else {
        log_error("parsing email field from userinfo failed");
        return unexpected(make_error_code(std::errc::io_error));
    }
}

expected<auth_data_t> parase_google_oauth20_json(std::string j) {
    rapidjson::Document d;
    d.Parse(j.c_str());
    if (d.HasParseError()) {
        log_error("JSON parse error: '{}'", j);
        return unexpected(make_error_code(std::errc::io_error));
    }

    auto as_str_or = [&d](const char* key, std::string _default) -> std::string {
        return d.HasMember(key) && d[key].IsString() ? d[key].GetString() : _default;
    };
    auto as_int_or = [&d](const char* key, int _default) -> int {
        return d.HasMember(key) && d[key].IsInt() ? d[key].GetInt() : _default;
    };

    auto as_string_vec = [&d](const char* key) -> std::vector<std::string> {
        std::vector<std::string> r;
        if (!d.HasMember(key) || !d.IsArray()) {
            return {};
        }
        for (auto& x : d[key].GetArray()) {
            if (x.IsString()) {
                r.emplace_back(x.GetString());
            }
        }
        return r;
    };

    return auth_data_t{.access_token = as_str_or("access_token", ""),
                       .expires_in = as_int_or("expires_in", 0),
                       .token_type = as_str_or("token_type", ""),
                       .id_token = as_str_or("id_token", ""),
                       .scope = as_str_or("scope", "")};
}

std::string mask_http_control_characters(std::string s) {
    std::string result;
    for (auto c : s) {
        if (c == '\r') {
            result += "\\r";
        } else if (c == '\n') {
            result += "\\n";
        } else {
            result += c;
        }
    }
    return result;
}

// simplest possible https client without redirecations support for google authentication.
class https_client_t : public EnableUseThis<https_client_t> {
   public:
    explicit https_client_t(asio::io_context& ctx)
        : m_ctx(ctx), m_ssl_ctx(asio::ssl::context::sslv23), m_socket(m_ctx, m_ssl_ctx) {}

    void async_connect(std::string host, std::string port, async_callback<void> cb) {
        if (m_socket.lowest_layer().is_open()) {
            log_error("already opened");
            cb(make_error_code(std::errc::already_connected));
            return;
        }

        asio::ip::tcp::resolver resolver{m_ctx};
        asio::ip::tcp::resolver::query query{host, port};

        std::error_code ec;
        auto endpoints = resolver.resolve(query, ec);
        if (ec) {
            log_error("resolve failed: {}", ec.message());
            cb(ec);
            return;
        }

        asio::async_connect(m_socket.lowest_layer(), std::move(endpoints),
                            [cb = std::move(cb), this_weak = weak_from_this()](
                                std::error_code ec, const asio::ip::tcp::endpoint& e) mutable {
                                if (ec) {
                                    log_error("failed connecting: {}", ec);
                                    cb(ec);
                                    return;
                                }
                                auto this_ptr = this_weak.lock();
                                if (!this_ptr) {
                                    cb(make_error_code(std::errc::owner_dead));
                                    return;
                                }
                                auto& this_ = *this_ptr;

                                log_debug("connected to: {}", e.address().to_string());

                                this_.m_socket.async_handshake(
                                    asio::ssl::stream_base::client,
                                    [cb = std::move(cb)](std::error_code ec) mutable {
                                        if (ec) {
                                            log_error("handshake failed: {}", ec);
                                            cb(ec);
                                            return;
                                        }

                                        cb({});
                                    });
                            });
    }

    struct http_response_parse_state_t {
        http_srv::reply reply;

        bool first = true;
        bool got_headers = false;
        bool got_content = false;

        std::optional<int> maybe_content_length;
    };

    static std::error_code http_parse_next_line(http_response_parse_state_t& state,
                                                std::string line) {
        if (state.got_content) {
            // i can't find out why but there is '0'\r\n after content.
            return {};
        }

        // https://developer.mozilla.org/en-US/docs/Web/HTTP/Messages
        if (state.first) {
            static const std::regex http_status_line_regex{R"(HTTP\/(\d)\.(\d) (\d+) (.*)\r\n)"};
            std::smatch match;
            if (!std::regex_match(line, match, http_status_line_regex)) {
                log_error("failed matching http header");
                return make_error_code(std::errc::illegal_byte_sequence);
            }
            state.reply.status = static_cast<http_srv::reply::status_type>(std::stoi(match[3]));
            state.first = false;
        } else {
            if (line == "\r\n") {
                log_debug("found brake after headers");
                state.got_headers = true;
            } else {
                if (!state.got_headers) {
                    // this must be header.
                    if (line.size() < 2 || line[line.size() - 2] != '\r' ||
                        line[line.size() - 1] != '\n') {
                        log_error("bad response, found a line without \r\n: {}", line);
                        return make_error_code(std::errc::illegal_byte_sequence);
                    }
                    line.resize(line.size() - 2);

                    const size_t pos = line.find_first_of(':');
                    if (pos != std::string::npos) {
                        // this must be header
                        std::string header = std::string(line, 0, pos);
                        std::string value = std::string(line, pos + 1 + 1);  // skip whitespace
                        log_debug("parsed header: '{}': '{}'", header, value);
                        state.reply.headers.emplace_back(std::move(header), std::move(value));
                    }
                } else {
                    // body/content
                    //
                    // from body we don't expect to have \r\n but only \n
                    if (state.maybe_content_length.has_value()) {
                        state.reply.content += line;
                        if (state.reply.content.size() > *state.maybe_content_length) {
                            const size_t extra_bytes =
                                state.reply.content.size() - *state.maybe_content_length;

                            // crlf if expected
                            if (!(extra_bytes == 2 &&
                                  state.reply.content[state.reply.content.size() - 2] == '\r' &&
                                  state.reply.content[state.reply.content.size() - 1] == '\n')) {
                                log_warning(
                                    "skipping {} additional bytes on top of "
                                    "content length",
                                    extra_bytes);
                            }
                            state.reply.content.resize(*state.maybe_content_length);
                            state.got_content = true;
                        }
                    } else {
                        state.maybe_content_length = std::stoi(line, 0, 16);
                        log_debug("got content length: {}", *state.maybe_content_length);
                    }
                }
            }
        }

        return {};
    }

    void async_make_request(http_srv::request r, async_callback<http_srv::reply> cb) {
        if (!m_socket.lowest_layer().is_open()) {
            log_error("not opened");
            cb(make_error_code(std::errc::not_connected), {});
            return;
        }

        std::stringstream request_s;

        request_s << fmt::format("{} {} HTTP/{}.{}\r\n", r.method, r.uri, r.http_version_major,
                                 r.http_version_minor);
        for (auto& [k, v] : r.headers) {
            request_s << fmt::format("{}: {}\r\n", k, v);
        }
        request_s << "\r\n";

        if (!r.body.empty()) {
            request_s << r.body << "\r\n";
        }

        log_debug("request to be sent:\n{}", request_s.str());

        asio::async_write(
            m_socket, asio::buffer(request_s.str()),
            [this_weak = weak_from_this(), cb = std::move(cb)](std::error_code ec, size_t) mutable {
                if (ec) {
                    log_error("async_write failed: {}", ec);
                    cb(ec, {});
                    return;
                }
                auto this_ptr = this_weak.lock();
                if (!this_ptr) {
                    cb(make_error_code(std::errc::owner_dead), {});
                    return;
                }
                auto& this_ = *this_ptr;

                log_debug("request written, reading..");

                asio::async_read(
                    this_.m_socket, this_.m_recv_buff,
                    [this_weak = this_.weak_from_this(), cb = std::move(cb)](
                        std::error_code ec, size_t bytes_transfered) mutable {
                        if (ec && ec != asio::ssl::error::stream_truncated) {
                            log_error("async read failed: {}", ec);
                            cb(ec, {});
                            return;
                        }
                        if (ec != asio::ssl::error::stream_truncated) {
                            log_warning("truncated result");
                        }

                        auto this_ptr = this_weak.lock();
                        if (!this_ptr) {
                            cb(make_error_code(std::errc::owner_dead), {});
                            return;
                        }
                        auto& this_ = *this_ptr;

                        log_debug("read done, parsing output");

                        std::istream is{&this_.m_recv_buff};
                        std::string line;

                        http_response_parse_state_t state;

                        while (std::getline(is, line)) {
                            // our getline eats "\n" so we need to add it back

                            line += "\n";
                            log_debug("received line: '{}'", mask_http_control_characters(line));

                            const auto parse_ec = http_parse_next_line(state, std::move(line));
                            if (parse_ec) {
                                log_error("failed parsing http response");
                                cb(ec, {});
                                return;
                            }
                        }

                        cb({}, std::move(state.reply));
                    });
            });
    }

   private:
    asio::io_context& m_ctx;
    asio::ssl::context m_ssl_ctx;
    asio::ssl::stream<asio::ip::tcp::socket> m_socket;
    asio::streambuf m_recv_buff{16384};  // we don't expect to receive larger result.
};

void async_request_token(https_client_t& client,
                         google_auth_app_creds_t app_creds,
                         std::string auth_code,
                         std::string scope,
                         std::string redirect_uri,
                         async_callback<auth_data_t> cb) {
    // now when we have code we need to make post request.
    const std::string host = "accounts.google.com";
    const std::string port = "443";

    log_debug("making params...");

    std::vector<std::string> token_request_params;
    token_request_params.emplace_back(fmt::format("client_id={}", app_creds.client_id));
    token_request_params.emplace_back(fmt::format("client_secret={}", app_creds.client_secret));
    token_request_params.emplace_back(fmt::format("scope={}", scope));
https:  // mail.google.com
    token_request_params.emplace_back(
        fmt::format("redirect_uri={}", encode_uri_component(redirect_uri)));
    token_request_params.emplace_back("grant_type=authorization_code");
    token_request_params.emplace_back(fmt::format("code={}", auth_code));

    // url-encoded form
    const auto body = fmt::format("{}", fmt::join(token_request_params, "&"));

    log_debug("making request...");

    http_srv::request r;
    r.http_version_major = 1;
    r.http_version_minor = 1;

    r.method = "POST";
    r.uri = "/o/oauth2/token";
    r.body = body;

    r.headers.emplace_back("Host", host);
    r.headers.emplace_back("Connection", "close");
    r.headers.emplace_back("Content-Type", "application/x-www-form-urlencoded");
    r.headers.emplace_back("Content-Length", std::to_string(body.size()));
    r.headers.emplace_back("Accept", "*/*");

    log_debug("connecting...");

    client.async_connect(
        host, port, [&client, r = std::move(r), cb = std::move(cb)](std::error_code ec) mutable {
            if (ec) {
                log_error("post request failed: conected failed: {}", ec);
                cb(ec, {});
                return;
            }

            log_debug("connected, making request..");

            client.async_make_request(
                r, [cb = std::move(cb)](std::error_code ec, http_srv::reply reply) mutable {
                    if (ec) {
                        log_error("post request failed: async_make_request failed: {}", ec);
                        cb(ec, {});
                        return;
                    }

                    log_debug("got a reply");
                    log_debug("status: {}", static_cast<int>(reply.status));
                    log_debug("headers: {}", reply.headers);
                    log_debug("content:\n'{}'", reply.content);

                    if (reply.status != http_srv::reply::ok) {
                        log_error("google replies with non-ok status: {}",
                                  static_cast<int>(reply.status));
                        cb(make_error_code(std::errc::protocol_error), {});
                        return;
                    }

                    const auto maybe_content_type = http_srv::header_value(reply, "Content-Type");
                    if (!maybe_content_type) {
                        log_error("no content type header");
                        cb(make_error_code(std::errc::protocol_error), {});
                        return;
                    }
                    if (*maybe_content_type != "application/json; charset=utf-8") {
                        log_error("invalid content type: '{}'", *maybe_content_type);
                        cb(make_error_code(std::errc::protocol_not_supported), {});
                        return;
                    }

                    log_debug("parsing response: '{}'", reply.content);
                    auto oauth20_data_or_err = parase_google_oauth20_json(reply.content);
                    if (!oauth20_data_or_err) {
                        log_error("failed parsing oauth20 google response");
                        cb(make_error_code(std::errc::protocol_not_supported), {});
                        return;
                    }
                    auto& oauth20_data = *oauth20_data_or_err;

                    cb({}, std::move(oauth20_data));
                });
        });
};

void async_request_userinfo(asio::io_context& ctx,
                            auth_data_t auth_data,
                            async_callback<std::string> cb) {
    const std::string host = "www.googleapis.com";
    const std::string port = "443";

    log_debug("making request...");

    http_srv::request r;
    r.http_version_major = 1;
    r.http_version_minor = 1;

    r.method = "GET";
    r.uri = fmt::format("/oauth2/v3/userinfo?access_token=\"{}\"", auth_data.access_token);
    r.body = "";

    r.headers.emplace_back("Host", host);
    r.headers.emplace_back("Connection", "close");
    r.headers.emplace_back("Accept", "*/*");

    log_debug("connecting...");

    auto client = std::make_shared<https_client_t>(ctx);

    // TODO: All this requests should have timeouts. And probably, the client should be held as
    // member somwhere so this can be cancalled either by timeout or anytime when parent instance is
    // destroyed.
    client->async_connect(
        host, port, [client, r = std::move(r), cb = std::move(cb)](std::error_code ec) mutable {
            if (ec) {
                log_error("post request failed: conected failed: {}", ec);
                cb(ec, {});
                return;
            }

            log_debug("connected, making request..");

            client->async_make_request(
                r, [client, cb = std::move(cb)](std::error_code ec, http_srv::reply reply) mutable {
                    if (ec) {
                        log_error("post request failed: async_make_request failed: {}", ec);
                        cb(ec, {});
                        return;
                    }
                    log_debug("got a reply");
                    log_debug("status: {}", static_cast<int>(reply.status));
                    log_debug("headers: {}", reply.headers);
                    log_debug("content:\n'{}'", reply.content);

                    if (reply.status == http_srv::reply::ok) {
                        std::string user_email;
                        auto email_or_err = parse_userinfo_reply(reply.content);
                        if (!email_or_err) {
                            cb(make_error_code(std::errc::io_error), "");
                            return;
                        }
                        cb({}, *email_or_err);
                    } else {
                        cb(make_error_code(std::errc::io_error), "");
                    }
                });
        });
}

const std::string auth_page_template = R"(
<!doctype html>
<html lang="en"
<head> <meta charset="UTF-8"> </head>
<body>
    <a href="https://accounts.google.com/o/oauth2/auth?response_type=code&client_id={client_id}&scope={scope}&redirect_uri={redirect_uri}">OAuth 2.0 аутентифікація через Google</a>
</body>
</html>
)";

const std::string auth_success_page = R"(
<!doctype html>
<html lang="en"
<head> <meta charset="UTF-8"> </head>
<body>
    <h1> Auth success! </h1>
</body>
</html>
)";

// TODO:
// Potentially missing part is to enable automatically imap in gmail client on behalf of user (with
// corresponding token):
//     const std::string url =
//         "https://gmail.googleapis.com/gmail/v1/users/me/settings/imap";

//     curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
//     curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
//     curl_easy_setopt(curl, CURLOPT_PUT, 1L);

//     std::stringstream ss;
//     ss << R"({
//   "enabled": true,
//   "autoExpunge": true,
//   "expungeBehavior": "expungeBehaviorUnspecified"
// })";
class google_auth_t_impl : public google_auth_t,
                           public std::enable_shared_from_this<google_auth_t_impl> {
   public:
    google_auth_t_impl(asio::io_context& ctx, std::string host, std::string port)
        : m_ctx(ctx),
          m_host(host),
          m_port(port),
          m_https_client(std::make_shared<https_client_t>(m_ctx)) {}

    bool initialize() {
        m_srv = http_srv::make_http_srv(m_ctx, m_host, m_port);
        return m_srv != nullptr;
    }

    std::string get_auth_page_text(const google_auth_app_creds_t& app_creds,
                                   const std::vector<std::string>& scopes) {
        const auto scopes_encoded =
            emailkit::encode_uri_component(fmt::format("{}", fmt::join(scopes, " ")));

        // log_debug("scopes_encoded: {}", scopes_encoded);

        // NOTE: redirect uri should be registered in google console in Credentials for the
        // app. otherwise one will got: 400/redirect_uri_mismatch.
        // TODO: make URL adjustable so clients of this class can utilize public interface of the
        // class.
        const auto redirect_uri = emailkit::encode_uri_component(local_site_uri("/done"));

        return fmt::format(
            fmt::runtime(auth_page_template), fmt::arg("client_id", app_creds.client_id),
            fmt::arg("scope", scopes_encoded), fmt::arg("redirect_uri", redirect_uri));
    }

    void async_initiate_web_based_auth(google_auth_app_creds_t app_creds,
                                       std::vector<std::string> scopes,
                                       async_callback<auth_data_t> auth_done_cb,
                                       async_callback<std::string> cb) override {
        if (m_auth_initiated) {
            log_error("auth already running");
            cb(make_error_code(std::errc::io_error), {});
            return;
        }

        auth_done_cb = [auth_done_cb = std::move(auth_done_cb), this_weak = weak_from_this()](
                           std::error_code ec, auth_data_t auth_data) mutable {
            if (auto this_ = this_weak.lock()) {
                log_error("stopping http server");
                this_->m_srv->stop();
                auth_done_cb(ec, std::move(auth_data));
            }
        };

        // TODO: check also its own state to prevent invalid usage.
        const auto scopes_encoded = fmt::format("{}", fmt::join(scopes, " "));

        assert(m_srv);

        m_srv->register_handler(
            "get", "/",
            [html_page = get_auth_page_text(app_creds, scopes)](
                const http_srv::request& req, async_callback<http_srv::reply> cb) {
                log_debug("page:\n'{}'", html_page);
                http_srv::reply reply;
                reply.headers.emplace_back(http_srv::header{"Connection", "Close"});
                reply.headers.emplace_back(
                    http_srv::header{"Content-Length", std::to_string(html_page.size())});
                reply.headers.emplace_back(http_srv::header{"Content-Type", "text/html"});
                reply.content = html_page;
                reply.status = http_srv::reply::ok;

                cb({}, reply);
            });
        m_srv->register_handler(
            "get", "/done",
            [this_weak = weak_from_this(), app_creds, scopes_encoded,
             auth_done_cb = std::move(auth_done_cb)](const http_srv::request& req,
                                                     async_callback<http_srv::reply> cb) mutable {
                auto this_ptr = this_weak.lock();
                if (!this_ptr) {
                    cb(make_error_code(std::errc::owner_dead), {});
                    return;
                }
                auto& this_ = *this_ptr;

                const auto complete_uri = this_.local_site_uri("/done") + req.uri;

                auto uri_or_err = folly::Uri::tryFromString(complete_uri);
                if (!uri_or_err) {
                    log_error("failed parsing uri '{}': {}", complete_uri, uri_or_err.error());
                    cb({}, http_srv::reply::stock_reply(http_srv::reply::internal_server_error));
                    auth_done_cb(make_error_code(std::errc::io_error), {});
                    return;
                }
                auto& uri = *uri_or_err;

                // TODO: don't know if we need to check if returned scope is requested one.
                std::string code;
                for (auto& [p, v] : uri.getQueryParams()) {
                    log_debug("param: {}: {}", p, v);
                    if (p == "code") {
                        code = v;
                    }
                }
                if (code.empty()) {
                    // something wentt wrong
                    log_error("unexpected response, code not found in URL from google: {}",
                              complete_uri);
                    cb({}, http_srv::reply::stock_reply(http_srv::reply::internal_server_error));
                    auth_done_cb(make_error_code(std::errc::io_error), {});
                    return;
                }

                async_request_token(
                    *this_.m_https_client, app_creds, code, scopes_encoded,
                    this_.local_site_uri("/done"),
                    [this_weak = this_.weak_from_this(), cb = std::move(cb),
                     auth_done_cb = std::move(auth_done_cb)](

                        std::error_code ec, auth_data_t auth_data) mutable {
                        auto this_ptr = this_weak.lock();
                        if (!this_ptr) {
                            cb(make_error_code(std::errc::owner_dead), {});
                            return;
                        }
                        auto& this_ = *this_ptr;

                        if (ec) {
                            log_error("async_request_token failed: {}", ec);
                            cb(ec, http_srv::reply::stock_reply(
                                       http_srv::reply::internal_server_error));
                            auth_done_cb(make_error_code(std::errc::io_error), {});
                            return;
                        }

                        if (auth_data.access_token.empty()) {
                            log_error("didn't get token");
                            cb(ec, http_srv::reply::stock_reply(
                                       http_srv::reply::internal_server_error));
                            auth_done_cb(make_error_code(std::errc::io_error), {});
                            return;
                        }

                        http_srv::reply reply;
                        reply.status = http_srv::reply::ok;
                        reply.headers.emplace_back(http_srv::header{"Connection", "Close"});
                        reply.content = auth_success_page;
                        cb({}, reply);

                        log_debug("making request for userinfo (email)");

                        async_request_userinfo(
                            this_.m_ctx, auth_data,
                            [auth_data, this_weak = this_.weak_from_this(), cb = std::move(cb),
                             auth_done_cb = std::move(auth_done_cb)](
                                std::error_code ec, std::string user_email) mutable {
                                auto this_ptr = this_weak.lock();
                                if (!this_ptr) {
                                    cb(make_error_code(std::errc::owner_dead), {});
                                    return;
                                }
                                auto& this_ = *this_ptr;

                                if (ec) {
                                    log_error("async_request_userinfo failed: {}", ec);
                                    cb(ec, http_srv::reply::stock_reply(
                                               http_srv::reply::internal_server_error));
                                    auth_done_cb(make_error_code(std::errc::io_error), {});
                                    return;
                                }

                                auth_data.user_email = user_email;

                                asio::post(this_.m_ctx,
                                           [auth_data = std::move(auth_data),
                                            shared_auth_done =
                                                std::make_shared<async_callback<auth_data_t>>(
                                                    std::move(auth_done_cb))] {
                                               (*shared_auth_done)({}, std::move(auth_data));
                                           });
                            });
                    });
            });
        m_srv->register_handler(
            "get", "/favicon.ico",
            [](const http_srv::request& req, async_callback<http_srv::reply> cb) {
                // TODO:
                cb({}, http_srv::reply::stock_reply(http_srv::reply::not_found));
            });

        if (auto ec = m_srv->start(); ec) {
            log_error("failed starting local http server needed for auth: {}", ec);
            m_callback(ec, {});
            return;
        }

        log_info("GMAIL auth HTTP server started at: {}", local_site_uri("/"));

        m_auth_initiated = true;

        // TODO: make sure we can really connect to immidiately after start() returned. What can be

        cb({}, local_site_uri("/"));
    }

    std::string local_site_uri(std::string resource) {
        return fmt::format("http://{}:{}{}", m_host, m_port, resource);
    }

   private:
    asio::io_context& m_ctx;
    std::string m_host;
    std::string m_port;
    shared_ptr<http_srv::http_srv_t> m_srv;
    std::shared_ptr<https_client_t> m_https_client;
    async_callback<auth_data_t> m_callback;
    bool m_auth_initiated = false;
};
}  // namespace

shared_ptr<google_auth_t> make_google_auth(asio::io_context& ctx,
                                           std::string host,
                                           std::string port) {
    auto inst = std::make_shared<google_auth_t_impl>(ctx, host, port);
    if (!inst->initialize()) {
        return nullptr;
    }
    return inst;
}
}  // namespace emailkit
