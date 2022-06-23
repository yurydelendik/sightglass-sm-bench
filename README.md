# Benchmark sightglass driver for SpiderMonkey 

Experimental project to evaluate regressions in SM againt [sightglass tests](https://github.com/bytecodealliance/sightglass) and compare itself with Cranelift.

## Build

The project is using SpiderMonkey embedding. See https://github.com/mozilla-spidermonkey/spidermonkey-embedding-examples/blob/esr91/docs/Building%20SpiderMonkey.md on how to build the library

Tested using the esr102 branch, there are couple of issues present:
- https://bugzilla.mozilla.org/show_bug.cgi?id=1776255
- https://bugzilla.mozilla.org/show_bug.cgi?id=1776254

## Running

The documentation on running sightglass can be found at https://github.com/bytecodealliance/sightglass#running-the-full-benchmark-suite

