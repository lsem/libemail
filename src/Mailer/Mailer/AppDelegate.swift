//
//  AppDelegate.swift
//  Mailer
//
//  Created by semkiv on 30.03.2024.
//

import Cocoa

// The task for today is to really finalize decision on how state changes are going to be handled by Cocoa part.
// We want to have nicely placed and functional windows for Main window and for Login window. And whenever app decices that it needs to have login, they should be changed. We want to have a button telling the app to invalidate credentials, remove them and close current connection as if it can happen in the production.

// On UI preservation: https://bignerdranch.com/blog/cocoa-ui-preservation-yall/

@main
class AppDelegate: NSObject, NSApplicationDelegate, CoreApplicationLoginDelegate,
    LoginWindowControllerDelegate
{
    var core: MailerAppCore? = nil

    lazy var mainWindowController: MainWindowController = {
        let instance = MainWindowController()
        return instance
    }()
    lazy var loginWindowController: LoginWindowController = {
        let instance = LoginWindowController()
        instance.coreAppLoginDelegate = self
        instance.delegate = self
        return instance
    }()

    // MARK: LoginWindowControllerDelegate
    func authDoneWithResult(_ success: Bool, _ creds: Credentials?) {
        print("APP: auth done with result: \(success)")
        if success {
            DispatchQueue.main.asyncAfter(deadline: .now() + 2) {
                self.core?.asyncAcceptCreds(
                    creds!,
                    completionCB: { res in
                        if res {
                            print("APP: creds accepted")
                            // We expect that the app will take the creds and will switch state accordingly..
                            // Theoretically, we should now open another, intermediate window telling customer what is going to happen next.
                            // Question: what if the app will not transition into established state and we will stuck?

                        } else {
                            print("APP: ERROR: creds are not accepted")
                            // TODO: do something.
                        }
                    })
            }
        }
    }

    func showMainWindow() {
        self.mainWindowController.showWindow(nil)
        self.mainWindowController.window?.center()
    }

    func hideMainWindow() {
        self.mainWindowController.window?.close()
        // TODO: how we are supposed to remove it?
    }

    func showLoginWindow() {
        self.loginWindowController.showWindow(nil)
        self.loginWindowController.window?.center()
    }

    func hideLoginWindow() {
        self.loginWindowController.window?.close()
        // TODO: how we are supposed to remove it?
    }

    // MARK: CoreApplicationLoginDelegate
    func asyncRequestGmailAuth(_ completionCallback: @escaping (Bool, String?) -> Void) {
        if let core = self.core {
            core.asyncRequestGmailAuth { succeded, maybeURI in
                completionCallback(succeded, maybeURI!)
            }
        }
    }
    func asyncWaitAuthDone(_ completionCallback: @escaping (Bool, Credentials?) -> Void) {
        if let core = self.core {
            core.asyncWaitAuthDone { succeeded, credentials in
                completionCallback(succeeded, credentials)
            }
        }
    }
    func asyncTestCreds(_ creds: Credentials, _ completionCallback: @escaping (Bool) -> Void) {
        if let core = self.core {
            core.asyncTestCreds(creds) { succeded in
                completionCallback(succeded)
            }
        }
    }
    func asyncAcceptCreds(_ creds: Credentials, _ completionCallback: @escaping (Bool) -> Void) {
        if let core = self.core {
            core.asyncAcceptCreds(creds) { succeded in
                completionCallback(succeded)
            }
        }
    }

    // MARK: LoginWindowControllerDelegate
    func mailerInstance() -> MailerAppCore? {
        return self.core
    }

    func coreCallback__stateChanged(_ s: ApplicationState) {
        print("APP/CORE: application state changed to \(s)")
        if s == .valueLoginRequired {
            hideMainWindow()
            showLoginWindow()
        } else if s == .valueIMAPEstablished {
            showMainWindow()
            hideLoginWindow()
        } else {
            // Other states? What if login accepted but imap is not establishes if there is no internet?
            // Other problem: what if we cannot auth because of different reasons, including no internet?
        }
    }

    func coreCallback__AuthInitiated(_ uri: String) {
        print("APP/CORE: Core requested authentication, there URI is: \(uri)")
    }

    func coreCallback__treeAboutToChange() {
        print("APP/CORE: Tree about to change")
    }
    func coreCallback__treeModelChanged() {
        print("APP/CORE: Tree Model Changed")
    }

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        self.core = MailerAppCore()

        if let core = self.core {
            // Core must already exist because view controllers will get a reference to it as part of they loading/initialization.

            // TODO: decide if we need first open MainWindow and only then create and start the app.
            // Alternatively we can create some sort of SplashWindow for this short period of time but this is not a modern way. More modern way would be to present Main window but have some kind of mode in it which is disabled and greyed-out.

            self.mainWindowController.showWindow(nil)
            self.mainWindowController.window?.center()
            //self.loginWindowController.showWindow(nil)
            //self.loginWindowController.window?.center()

            core.stateChangedBlock = { state in
                self.coreCallback__stateChanged(state)
            }
            core.authInitiatedBlock = { uri in
                self.coreCallback__AuthInitiated(uri!)
            }
            core.authDoneBlock = { succeeded, creds in
            }
            core.treeAboutToChangeBlock = {
                self.coreCallback__treeAboutToChange()
            }
            core.treeModelChangedBlock = {
                self.coreCallback__treeModelChanged()
            }

            // TODO: implement more callbacks, especiall those that update UI.

            core.startEventLoop()
            core.asyncRun(completionBlock: { succeded in
                if succeded {
                    print("core is ruunning")
                } else {
                    print("failed running mailer core")
                    self.terminateApplicationCausedByCriticalError(
                        message: "Failed running Core application")
                }
            })
        } else {
            terminateApplicationCausedByCriticalError(
                message: "Failed creating an instance of application core")
        }
    }

    func terminateApplicationCausedByCriticalError(message: String) {
        let alert = NSAlert()
        alert.messageText = "\(message).\nThis is fatal error and application will be closed"
        alert.alertStyle = NSAlert.Style.critical
        alert.runModal()
        // TODO: deliver information to develoeprs. Ask user persmissions for sending this  (via email?).
        NSApplication.shared.terminate(nil)
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
        if let core = self.core {
            // TODO: here should be some timeout after which we terminate no matter what
            core.stopEventLoop()
        }
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        return true
    }
}
