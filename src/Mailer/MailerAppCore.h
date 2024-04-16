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
typedef void (^AuthDoneBlock)(BOOL);
typedef void (^TreeAboutToChangeBlock)();
typedef void (^TreeModelChangedBlock)();


@interface MailerAppCore : NSObject 
  @property (nonatomic, copy) AuthInitiatedBlock  authInitiatedBlock;
  @property (nonatomic, copy) AuthDoneBlock  authDoneBlock;
  @property (nonatomic, copy) TreeAboutToChangeBlock  treeAboutToChangeBlock;
  @property (nonatomic, copy) TreeModelChangedBlock  treeModelChangedBlock;


- (instancetype) init;
- (void)startEventLoop;
- (void)stopEventLoop;

// TODO: consider using NSError instead of bool.
- (void)asyncRunWithCompletionBlock:(void (^)(BOOL))block;

@end

#endif /* MailerAppCore_h */

