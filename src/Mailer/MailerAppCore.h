//
//  MailerAppCore.h
//  Mailer
//
//  Created by semkiv on 13.04.2024.
//

#ifndef MailerAppCore_h
#define MailerAppCore_h

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, ApplicationState) {
    ApplicationStateValueNotReady,
    ApplicationStateValueLoginRequired,
    ApplicationStateValueWaitingAuth,
    ApplicationStateValueConnectingToTest,
    ApplicationStateValueConnectedToTest,
    ApplicationStateValueReadyToConnect,
    ApplicationStateValueIMAPAuthenticating,
    ApplicationStateValueIMAPEstablished,
};

@interface Credentials : NSObject
@end

typedef void (^StateChangedBlock)(ApplicationState);
typedef void (^AuthInitiatedBlock)(NSString* uri);
typedef void (^AuthDoneBlock)(BOOL, Credentials*);
typedef void (^TreeAboutToChangeBlock)();
typedef void (^TreeModelChangedBlock)();

@interface MailerAppCore : NSObject
@property(nonatomic, copy) StateChangedBlock stateChangedBlock;
@property(nonatomic, copy) AuthInitiatedBlock authInitiatedBlock;
@property(nonatomic, copy) AuthDoneBlock authDoneBlock;
@property(nonatomic, copy) TreeAboutToChangeBlock treeAboutToChangeBlock;
@property(nonatomic, copy) TreeModelChangedBlock treeModelChangedBlock;

- (instancetype)init;
- (void)startEventLoop;
- (void)stopEventLoop;

// TODO: consider using NSError instead of bool.
- (void)asyncRunWithCompletionBlock:(void (^)(BOOL))cb;
- (void)asyncRequestGmailAuth:(void (^)(BOOL, NSString*))cb;
- (void)asyncWaitAuthDone:(void (^)(BOOL, Credentials*))cb;
- (void)asyncAcceptCreds:(Credentials*)creds completionCB:(void (^)(BOOL))cb;
- (void)asyncTestCreds:(Credentials*)creds completionCB:(void (^)(BOOL))cb;
- (ApplicationState)getState;

@end

#endif /* MailerAppCore_h */
