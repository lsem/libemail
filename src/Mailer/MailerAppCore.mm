//
//  MailerAppCore.m
//  Mailer
//
//  Created by semkiv on 13.04.2024.
//

#include "MailerAppCore.h"

#include <mailer_poc.hpp>

// How to wrap shared pointer into Objective-C class.
// I can create encapculated objective C class.
// Inside of the class I will put credentials object.
//

@interface Credentials () {
   @public
    std::unique_ptr<mailer::IMAPConnectionCreds> creds_ptr_;
}
@end

@implementation Credentials

- (instancetype)init {
    return [self initWithExistingIMAPConnectionsCreds:{}];
}

- (instancetype)initWithExistingIMAPConnectionsCreds:(mailer::IMAPConnectionCreds)creds {
    self = [super init];
    if (self) {
        creds_ptr_ = std::make_unique<mailer::IMAPConnectionCreds>(std::move(creds));
    }
    return self;
}

@end

ApplicationState translate_from_mailer(mailer::ApplicationState s) {
    switch (s) {
        case mailer::ApplicationState::not_ready:
            return ApplicationStateValueNotReady;
        case mailer::ApplicationState::login_required:
            return ApplicationStateValueLoginRequired;
        case mailer::ApplicationState::waiting_auth:
            return ApplicationStateValueWaitingAuth;
        case mailer::ApplicationState::connecting_to_test:
            return ApplicationStateValueConnectingToTest;
        case mailer::ApplicationState::connected_to_test:
            return ApplicationStateValueConnectedToTest;
        case mailer::ApplicationState::ready_to_connect:
            return ApplicationStateValueReadyToConnect;
        case mailer::ApplicationState::imap_authenticating:
            return ApplicationStateValueIMAPAuthenticating;
        case mailer::ApplicationState::imap_established:
            return ApplicationStateValueIMAPEstablished;
    }
}

class MailerGluedWithItsListener : public mailer::MailerPOCCallbacks {
   public:
    void state_changed(mailer::ApplicationState s) override {
        assert(state_changed_fn_);
        state_changed_fn_(s);
    }
    void auth_initiated(std::string uri) override {
        assert(auth_initiated_fn_);
        auth_initiated_fn_(uri);
    }
    void auth_done(std::error_code ec, mailer::IMAPConnectionCreds creds) override {
        assert(auth_done_fn_);
        auth_done_fn_(ec, creds);
    }
    void tree_about_to_change() override {
        assert(tree_about_to_change_fn_);
        tree_about_to_change_fn_();
    }
    void tree_model_changed() override {
        assert(tree_model_changed_fn_);
        tree_model_changed_fn_();
    }

    // This function must be execute fb in the UI thread. This supposedly is the only
    // place where it is safe to update the model.
    void update_state(std::function<void()> fn) override {
        assert(update_state_fn_);
        update_state_fn_(std::move(fn));
    }

    static std::unique_ptr<MailerGluedWithItsListener> create_instance() {
        auto mailer_instance_ = mailer::make_mailer_poc();
        if (!mailer_instance_) {
            log_error("failed creating mailer poc instance");
            return nullptr;
        }

        auto instance = std::make_unique<MailerGluedWithItsListener>();

        mailer_instance_->set_callbacks_if(instance.get());
        instance->mailer_instance_ = std::move(mailer_instance_);

        return instance;
    }

    std::shared_ptr<mailer::MailerPOC> mailer_instance_;

    std::function<void(mailer::ApplicationState)> state_changed_fn_;
    std::function<void(std::string)> auth_initiated_fn_;
    std::function<void(std::error_code, mailer::IMAPConnectionCreds)> auth_done_fn_;
    std::function<void()> tree_about_to_change_fn_;
    std::function<void()> tree_model_changed_fn_;
    std::function<void(std::function<void()>)>
        update_state_fn_;  // TODO: consider renaming to dispath_ui(function<void()>)
};

struct MailerPOCCallbacks {
   protected:
    ~MailerPOCCallbacks() = default;

   public:
    // Auth server is ready on URI. We can improve overall handling by first requesting
    // reauth so the client can prepare UI accordingly, then resolve callback and only
    // then have server started.
};

@interface MailerAppCore () {
    std::unique_ptr<MailerGluedWithItsListener> mailer_with_listener_;
}

@end

@implementation MailerAppCore

- (instancetype)init {
    self = [super init];
    if (self) {
        mailer_with_listener_ = MailerGluedWithItsListener::create_instance();
        if (!mailer_with_listener_) {
            log_error("failed creating mailer poc instance");
            // ARC will take care of deallocating self.
            return nil;
        }

        mailer_with_listener_->state_changed_fn_ = [self](mailer::ApplicationState s) {
            if (self.stateChangedBlock) {
                log_debug("Calling stateChangedBlock block");
                dispatch_async(dispatch_get_main_queue(), ^{
                  self.stateChangedBlock(translate_from_mailer(s));
                });

            } else {
                log_warning("No stateChanged block");
            }
            // translate_from_mailer(s);
        };

        mailer_with_listener_->auth_initiated_fn_ = [self](std::string uri) {
            if (self.authInitiatedBlock) {
                log_debug("Calling authInitiatedBlock block");
                dispatch_async(dispatch_get_main_queue(), ^{
                  self.authInitiatedBlock([NSString stringWithUTF8String:uri.c_str()]);
                });
            } else {
                log_warning("No authInitiatedBlock block");
            }
        };

        mailer_with_listener_->auth_done_fn_ = [self](std::error_code ec,
                                                      mailer::IMAPConnectionCreds creds) {
            if (self.authDoneBlock) {
                log_debug("Calling authDoneBlock block");
                // lets create somehow credentilas object.
                Credentials* new_objc_creds =
                    [[Credentials alloc] initWithExistingIMAPConnectionsCreds:std::move(creds)];
                dispatch_async(dispatch_get_main_queue(), ^{
                  self.authDoneBlock(ec ? FALSE : true, new_objc_creds);
                });
            } else {
                log_warning("No authDoneBlock block");
            }
        };

        mailer_with_listener_->tree_about_to_change_fn_ = [self]() {
            if (self.treeAboutToChangeBlock) {
                log_debug("Calling treeAboutToChangeBlock block");
                dispatch_async(dispatch_get_main_queue(), ^{
                  self.treeAboutToChangeBlock();
                });
            } else {
                log_warning("No treeAboutToChangeBlock block");
            }
        };

        mailer_with_listener_->tree_model_changed_fn_ = [self]() {
            if (self.treeModelChangedBlock) {
                log_debug("Calling treeModelChangedBlock block");
                dispatch_async(dispatch_get_main_queue(), ^{
                  self.treeModelChangedBlock();
                });
            } else {
                log_warning("No treeModelChangedBlock block");
            }
        };

        // TODO: it looks like update_state_fn (aka dispatch_ui) is not a callbacks but
        // a depdency. Tehcnically it can be among of callabcks but it should better be
        // in separate pack if we want to have descrnt API for C++ core.
        mailer_with_listener_->update_state_fn_ = [self](std::function<void()> fn) {
            dispatch_async(dispatch_get_main_queue(), ^{
              fn();
            });
        };
    }
    return self;
}

- (void)startEventLoop {
    mailer_with_listener_->mailer_instance_->start_working_thread();
}

- (void)stopEventLoop {
    mailer_with_listener_->mailer_instance_->stop_working_thread();
}

- (void)asyncRunWithCompletionBlock:(void (^)(BOOL))block {
    mailer_with_listener_->mailer_instance_->async_run([block](std::error_code ec) {
        if (ec) {
            log_error("failed running mailer instance: {}", ec);
            dispatch_async(dispatch_get_main_queue(), ^{
              block(FALSE);
            });
            return;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
          block(TRUE);
        });
    });
}

- (void)asyncRequestGmailAuth:(void (^)(BOOL, NSString*))cb {
    mailer_with_listener_->mailer_instance_->async_request_gmail_auth(
        [cb](std::error_code ec, mailer::AuthStartDetails details) {
            if (ec) {
                log_error("async_request_gmail_auth failed: {}", ec);
                dispatch_async(dispatch_get_main_queue(), ^{
                  cb(FALSE, nil);
                });
                return;
            }
            dispatch_async(dispatch_get_main_queue(), ^{
              cb(TRUE, [NSString stringWithUTF8String:details.uri.c_str()]);
            });
        });
}

- (void)asyncWaitAuthDone:(void (^)(BOOL, Credentials*))cb {
    mailer_with_listener_->mailer_instance_->async_wait_auth_done(
        [cb](std::error_code ec, mailer::IMAPConnectionCreds creds) {
            Credentials* new_objc_creds =
                [[Credentials alloc] initWithExistingIMAPConnectionsCreds:std::move(creds)];
            dispatch_async(dispatch_get_main_queue(), ^{
              cb(ec ? FALSE : true, new_objc_creds);
            });
        });
}

- (void)asyncAcceptCreds:(Credentials*)creds completionCB:(void (^)(BOOL))cb {
    mailer_with_listener_->mailer_instance_->async_accept_creds(
        *creds->creds_ptr_, [cb](std::error_code ec) {
            dispatch_async(dispatch_get_main_queue(), ^{
              cb(ec ? FALSE : TRUE);
            });
        });
}

- (void)asyncTestCreds:(Credentials*)creds completionCB:(void (^)(BOOL))cb {
    mailer_with_listener_->mailer_instance_->async_test_creds(
        *creds->creds_ptr_, [cb](std::error_code ec) {
            dispatch_async(dispatch_get_main_queue(), ^{
              cb(ec ? FALSE : TRUE);
            });
        });
}

- (ApplicationState)getState {
    return translate_from_mailer(mailer_with_listener_->mailer_instance_->get_state());
}

@end
