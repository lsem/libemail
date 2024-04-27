//
//  LoginWindowController.swift
//  Mailer
//
//  Created by semkiv on 27.04.2024.
//

import Cocoa

class LoginWindowController: NSWindowController {
    convenience init() {
        self.init(windowNibName: "LoginWindowController")
    }

    override func windowDidLoad() {
        super.windowDidLoad()

        // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
        self.contentViewController = LoginWindowViewController()
        
    }
    
}
