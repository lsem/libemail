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
class AppDelegate: NSObject, NSApplicationDelegate, LoginWindowControllerDelegate,
    LoginViewControllerDelegate
{
    var core: MailerAppCore? = nil

    lazy var mainWindowController: MainWindowController = {
        let instance = MainWindowController()
        return instance
    }()
    lazy var loginWindowController: LoginWindowController = {
        let instance = LoginWindowController()
        instance.delegate = self
        return instance
    }()

    // MARK: LoginViewControllerDelegate
    func viewControllerCreated(instance: LoginWindowViewController) {
        instance.delegate = self
    }

    // MARK: LoginViewControllerDelegate
    func mailerInstance() -> MailerAppCore {
        return self.core!
    }
    func loginGmailClicked() {
        print("loginGmailClicked")
    }

    func createControllerOrDie(byID: String) -> NSViewController! {
        if let mainStoryboard = NSStoryboard.main {
            if let windowNS = mainStoryboard.instantiateController(withIdentifier: byID)
                as? NSViewController
            {
                return windowNS
            } else {
                terminateApplicationCausedByCriticalError(
                    message: "Cannot instantiate controller with name \(byID)")
            }
        } else {
            terminateApplicationCausedByCriticalError(
                message: "Cannot access main storyboard of the app")
        }
        return nil
    }

    func coreCallback__stateChanged(_ s: ApplicationState) {
        print("APP/CORE: application state changed to \(s)")
        if s == .valueLoginRequired {
        } else if s == .valueIMAPEstablished {
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
            self.loginWindowController.showWindow(nil)
            self.loginWindowController.window?.center()

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
