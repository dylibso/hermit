# Hermit
## Actually Portable WASM Executables

### Setup

Clone with submodules so you get WAMR and the forked dockerfile-parser-rs (the hermitfile parser) i.e: `git clone --recurse-submodules`

Bootstrap pest (dockerfile-parser-rs dependency):

`cd dockerfile-parser-rs/third-party/pest && cargo build --package pest_bootstrap`

Clone the Cosmopolitan libc and setup `cosmocc` as mentioned in [Getting Started](https://github.com/jart/cosmopolitan/#getting-started). If `cosmocc` isn't in your `PATH` after adding a new shell, maybe append to `.bashrc` instead.

Create this dummy header: `touch /opt/cosmo/libc/isystem/sys/timeb.h`

### Build

`./build_hermit.sh` configures and builds with cmake to the `build` dir.

### Demo hermits

After building, try out:

`./build/cowsay.hermit.com 'Hello, Dylibso!'`

or

`echo aeiou | ./build/count_vowels.hermit.com`
