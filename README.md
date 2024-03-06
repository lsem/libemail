libemail is email library in C++.

https://doc.qt.io/qt-6.2/macos.html


# Building

 Ubuntu

`cmake -B bld-debug -GNinja -DCMAKE_BUILD_TYPE=Debug`

```
git submodule init --recursive --update
sudo apt install ninja-build
sudo apt install pkg-config 
sudo apt install libssl-dev
sudo apt install python3-distutils
sudo apt install qt5-default
sudo apt install qtwebengine5-dev
```

# MacOS
```
export SDKROOT=(xcrun --sdk macosx --show-sdk-path)
```

For Gcc 13 one need:
https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93082


# Without NODEJS module
Project supposed to be build with simplest cmake command 
```
cmake -B bld .
```

But if you don't have nodejs environment and/or don't need to build nodejs module, you can skip this with option:
-DEXCLUDE_NODE_MODULE=on

If you want to use specific nodejs version this can be specifed as well with option NODE_JS_BINARY:
-DNODE_JS_BINARY=/opt/my-custom-node/bin/node
