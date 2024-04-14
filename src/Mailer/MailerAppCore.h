//
//  MailerAppCore.h
//  Mailer
//
//  Created by semkiv on 13.04.2024.
//

#ifndef MailerAppCore_h
#define MailerAppCore_h

#import <Foundation/Foundation.h>

typedef void (^AuthInitiatedBlock)(NSString* uri);

@interface MailerAppCore : NSObject 
  @property (nonatomic, copy) AuthInitiatedBlock  authInitiatedBlock;

- (instancetype) init;
- (void)startEventLoop;
- (void)stopEventLoop;

// TODO: consider using NSError instead of bool.
- (void)asyncRunWithCompletionBlock:(void (^)(BOOL))block;
@end

#endif /* MailerAppCore_h */

