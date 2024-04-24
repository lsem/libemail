//
//  LoginViewController.swift
//  Mailer
//
//  Created by semkiv on 17.04.2024.
//

import Cocoa

protocol LoginViewControllerDelegate {
    func loginGmailClicked()
}

class LoginViewController: NSViewController {
    var uri: String = ""
    var delegate: LoginViewControllerDelegate? = nil
    var sharedAppCore: MailerAppCore? = nil

    @IBOutlet weak var closeButtonClicked: NSButton!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        // Do view setup here.
    }
    
    @IBAction func closeButtonReallyClicked(_ sender: Any) {
        print("loginWindow::closeButtonReallyClicked")
        if let delegate = delegate {
            delegate.loginGmailClicked()
        }
        if let core = self.sharedAppCore {
            print("calling asyncRequestGmailAuth")
            core.asyncRequestGmailAuth { succeded, uri in
                if succeded {
                    print("asyncRequestGmailAuth succeeded")
                    NSWorkspace.shared.open(URL(string: uri!)!)
                    core.asyncWaitAuthDone { succeded, creds in
                        print("got creds, lets test them next")
                        core.asyncTestCreds(creds) { succeded in
                            if (succeded) {
                                print("creds are OK")
                                core.asyncAcceptCreds(creds) { succeeded in
                                    if succeded {
                                        print("creds accepted")
                                    } else {
                                        // TODO:
                                        print("creds not accepted")
                                    }
                                }
                            } else {
                                print("creds are not OK")
                            }
                        }
                    }
                } else {
                    print("asyncRequestGmailAuth failed")
                }
            }
        }
    }
}
