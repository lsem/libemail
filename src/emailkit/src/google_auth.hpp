#pragma once
#include <emailkit/global.hpp>

namespace emailkit {

// controller-like class responsible for gmail autihentication.
// In a nutshell, gmail authentication OAuth2 authentication works as following:
//     1) we prepare a page or web server serving a page which has invintation to authenticate via
//     google.
//        being pressed this page allows to select one of known accounts in this browser or add new
//        first.
//     2) we, developers have registred application in google cloud platform console. This
//     application among other things has requested scopes. 2) user selects account and based on
//     scopes requested in gmail application google warns user about requested scopes and if user
//     proceeds,
//        google redirects browser to a page we provided in the link from step (1).
//     3) we receive credentials on our web server as JSON which includes oauth token, expiration
//     time, etc.. Using our client ID, client secret and token we can make requests to services we
//     got tokens to.
// https://accounts.google.com/o/oauth2/auth
// https://developers.google.com/identity/protocols/oauth2/
// https://developers.google.com/gmail/api/auth/scopes
// https://developers.google.com/gmail/imap/xoauth2-protocol
// https://developers.google.com/gmail/imap/imap-smtp
// https://developers.google.com/gmail/api/auth/scopes
// https://developers.google.com/gmail/api/reference/rest/v1/users.settings/getImap
// https://developers.google.com/gmail/api/guides/pop_imap_settings
// https://developers.google.com/identity/protocols/oauth2/production-readiness/restricted-scope-verification

// to be copy pasted to playground:
//      ideally we should not be commiting this stuff into source code I guess.
// email: liubomyr.semkiv.test
// password: *D*E6BGWV8w2
// // email: liubomyr.semkiv.test2
//    password: ViZj07u4*HW7
// const GOOGLE_TOKEN_URI = 'https://accounts.google.com/o/oauth2/token';
// const std::string google_client_id =
//     "303173870696-bsun94hmoseeumiat4iaa6dr752ta805.apps.googleusercontent.com";
// const std::string google_client_secret = "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";

// represents a thing given in application google console as json:
// TODO: what if we accept the JSON itself so app can just put into file or compile into code.
// {
//    "installed":{
//       "client_id":"303173870696-tpi64a42emnt758cjn3tqp2ukncggof9.apps.googleusercontent.com",
//       "project_id":"glowing-bolt-393519",
//       "auth_uri":"https://accounts.google.com/o/oauth2/auth",
//       "token_uri":"https://oauth2.googleapis.com/token",
//       "auth_provider_x509_cert_url":"https://www.googleapis.com/oauth2/v1/certs",
//       "client_secret":"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",
//       "redirect_uris":[
//          "http://localhost"
//       ]
//    }
// }

struct google_auth_app_creds_t {
    std::string client_id;
    std::string client_secret;
};

struct auth_data_t {
    std::string user_email;
    std::string access_token;
    int expires_in;
    std::string token_type;
    std::string id_token;
    std::string scope;

    // TODO: we need to provide scoped granted by user so we can inspect whether we have access at
    // all before processing with IMAP/SMTP.
    std::vector<std::string> scope_vec;
};

using LaunchBrowserFn = std::function<void(std::string uri)>;

class google_auth_t {
   public:
    virtual ~google_auth_t() = default;

    // opens system browser with page inviting to handle auth, brings up http server
    // to finally receive tokem from goole.
    virtual void async_handle_auth(google_auth_app_creds_t app_creds,
                                   std::vector<std::string> scopes,
                                   async_callback<auth_data_t> cb) = 0;
};

// host and port denote where local web server used for serving site and accepting auth data will be
// brought up.
shared_ptr<google_auth_t> make_google_auth(asio::io_context& ctx,
                                           std::string host,
                                           std::string port,
                                           LaunchBrowserFn = nullptr);

}  // namespace emailkit

DEFINE_FMT_FORMATTER(
    emailkit::auth_data_t,
    "auth_data_t(access_token: '{}', expires_in: {}, token_type: '{}', id_token: '{}', scope: {})",
    arg.access_token,
    arg.expires_in,
    arg.token_type,
    arg.id_token,
    arg.scope);
