# Hermit
A toolkit for creating Actually Portable WASM Executables, called hermits. See [blogpost](https://dylib.so).

## Downloads

See [releases]().

## Usage

`./hermit.com [-f <path_to_Hermitfile] [-o <output_path>]`

If a path to a `Hermitfile` is not provided, it tries to load `Hermitfile` from the current directory. If an `output_path` is not provided, the hermit is written to `wasm.com` in the currently directly. On Unix-like operating systems you must `chmod +x wasm.com` to make it executable. This is required because WASI does not have a `chmod` function.

## Hermitfile syntax

* `FROM <path_to_wasm_without_quotes>` - mandatory, instructs the hermit cli what Wasm module to use. Path can be relative to the location of the `Hermitfile`.
* `MAP <path>` - maps a directory (and subdirectories) into the WASI filesystem used by the Wasm module. Can be used multiple times. You cannot map the `.` and `/` at the same time, see `ENV_PWD_IS_HOST_CWD`.
* `ENV KEY=VALUE` - declares an environment variable for the Wasm. Can be used multiple times.
* `ENV_PWD_IS_HOST_CWD` - passes the current working directory on the host into the Wasm via the environment variable `$PWD`. This works around a limitation in WASI that there's no way to initialize the current working directory when combined with adding `chdir(getenv("PWD"))` or equivalent into the beginning of your Wasm program.
* `ENV_EXE_NAME_IS_HOST_EXE_NAME` - passes the path to the currently running executable into the Wasm via the environment variable `EXE_NAME`. This was implemented to enable implementing the hermit cli as a hermit.

## Limitations
* Uses Wasm Micro Runtime (WAMR) in interpreter mode for the Wasm runtime.
    * Wasm runs slow
    * Wasm with SIMD is unsupported
* In order for the Wasm to inherit the current directory from the host, it must set it itself, possibly using `ENV_PWD_IS_HOST_CWD` and loading from `$PWD`.
* Network isn't implemented yet. WAMR has support so it probably isn't a hard change.
* External functionality is limited to what WASI supports.
* Only x86_64 supported right now, but it probably could be extended to use `fatcosmocc` instead of `cosmocc`.

## Building

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
