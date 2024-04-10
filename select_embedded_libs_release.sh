#!/usr/bin/env bash

mkdir -p _build/embedded_libs/
cp _build/release/dist/lib/libmailer_poc.dylib _build/embedded_libs/
cp _build/release/dist/lib/libffi.8.dylib _build/embedded_libs/
cp _build/release/dist/lib/libgmime-3.0.0.dylib _build/embedded_libs/
