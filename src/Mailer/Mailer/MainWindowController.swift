//
//  MainWindowController.swift
//  Mailer
//
//  Created by semkiv on 27.04.2024.
//

import Cocoa

// Interesting links:
// https://forums.developer.apple.com/forums/thread/89498

class MainWindowController: NSWindowController, NSWindowDelegate {
    convenience init() {
        // ViewController base calss has this behaviour so we make it consistent with reasonable convention.
        self.init(windowNibName: "MainWindowController")
    }

    override func windowDidLoad() {

        // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file
        super.windowDidLoad()

        self.window?.delegate = self

        // TODO: this is temporary solution. What really want is to have window positioned in best possible way when applications starts. This should probably be done somehow with standard methods for Cocoa apps.
        if let screenSize = NSScreen.main?.frame.size {
            window?.minSize = CGSize(
                width: screenSize.width * 0.75, height: screenSize.height * 0.75)
        }
        // Additionally, we can center window from here but we have to call makeKeyAndOrderFront(nil) before it to place it into the list of windows (windowDidLoad() executed before it is added to the list).

        self.contentViewController = MainWindowViewController()

    }
}
