# Benchmark sightglass driver for SpiderMonkey 

Experimental project to evaluate regressions in SM againt [sightglass tests](https://github.com/bytecodealliance/sightglass) and compare itself with Cranelift.

## Build

The project is using SpiderMonkey embedding. See https://github.com/mozilla-spidermonkey/spidermonkey-embedding-examples/blob/esr91/docs/Building%20SpiderMonkey.md on how to build the library

Rough steps to build mozjs library:
1. Clone Firefox source code, e.g. `git clone --depth 1 -b beta https://github.com/mozilla/gecko-dev`
2. Install all prereqs required to [build FF](https://firefox-source-docs.mozilla.org/setup/index.html), and recommended to run `./mach boostrap` in the cloned folder to configure for desktop builds
3. Make a build folder and configure to build the engine, e.g.:

```
mkdir _build && cd _build
# for macos, edit ../gecko-dev/build/moz.configure/pkg.configure line 19 to remove `"OSX", `
# see https://bugzilla.mozilla.org/show_bug.cgi?id=1776255
sh ../gecko-dev/js/src/configure \
    --disable-jemalloc --with-system-zlib --with-intl-api \
    --enable-release --enable-optimize --prefix=$PWD/../mozjs
make -j 8
make install
```

Back to main library build: just make a copy of Makefile and adjust the variables, then `make`.

You may need `export LD_LIBRARY_PATH=/path/to/mozjs/lib` before running with the sightglass.

## Running

The documentation on running sightglass can be found at https://github.com/bytecodealliance/sightglass#running-the-full-benchmark-suite

