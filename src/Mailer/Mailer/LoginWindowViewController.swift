//
//  LoginWindowViewController.swift
//  Mailer
//
//  Created by semkiv on 27.04.2024.
//

import Cocoa

protocol LoginViewControllerDelegate: AnyObject {
    func mailerInstance() -> MailerAppCore
}

class LoginWindowViewController: NSViewController {
    var sharedAppCore: MailerAppCore?
    weak var delegate: LoginViewControllerDelegate?
    @IBOutlet weak var usernameTextField: NSTextField!
    @IBOutlet weak var passwordTextField: NSTextField!
    @IBOutlet weak var imapServerHostTextField: NSTextField!
    @IBOutlet weak var imapServerPortTextField: NSTextField!

    override func viewDidLoad() {
        super.viewDidLoad()
    }

    @IBAction func googleButtonAction(_ sender: Any) {
        print("LoginWindowViewController: google action")
        processGoogleAuthAction()
    }

    @IBAction func hotmailButtonAction(_ sender: Any) {
        print("LoginWindowViewController: hotmail action")
    }

    @IBAction func plainIMAPLoginAction(_ sender: Any) {
        print("LoginWindowViewController: plain IMAP action")
    }

    func processGoogleAuthAction() {
        guard let delegate = self.delegate else {
            print("LoginWindowViewController: no delegate")
            return
        }

        let core = delegate.mailerInstance()
        // QUESTION: are these core and delegate going to be magically retained because they are copied into each closure?

        print("calling asyncRequestGmailAuth")
        core.asyncRequestGmailAuth { succeded, uri in
            if !succeded {
                print("ERROR: asyncRequestGmailAuth failed")
                // TODO: ...
                return
            }
            print("asyncRequestGmailAuth succeeded")
            // TODO: use completion handler instead of assuming it is always successful,
            // use (NSWorkspace.shared.open(URL(string: uri!)!, configuration: NSWorkspace.OpenConfiguration) for this.
            // TODO: we can also possibly track completion of the browser.
            NSWorkspace.shared.open(URL(string: uri!)!)
            core.asyncWaitAuthDone { succeded, creds in
                if !succeded {
                    print("ERROR: asyncWaitAuthDone failed")
                    // TODO: ...
                    return
                }
                print("got creds, lets test them next")

                NSRunningApplication.current.activate(options: [
                    .activateAllWindows, .activateIgnoringOtherApps,
                ])

                core.asyncTestCreds(creds) { succeded in
                    if !succeded {
                        print("ERROR: asyncTestCreds failed")
                        // TODO: ...
                        return
                    }

                    print("creds are OK")
                    // Accepting creds will change state of the app to connected state which in turn will close login window.
                    core.asyncAcceptCreds(creds) { succeeded in
                        if succeded {
                            print("creds accepted")
                        } else {
                            // TODO:
                            print("creds not accepted")
                        }
                    }
                }
            }

        }
    }

}
