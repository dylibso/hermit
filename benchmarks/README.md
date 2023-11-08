## Benchmarks

The purpose of this tool is to benchmark the performance of the `hermit` CLI.This will let us track performance changes from one release to another.

Example of current benchmarks with a VM(4xCPU : Intel(R) Core(TM) i7-10750H CPU @ 2.60GHz, RAM 6GB) :
    
- Debug mode
```log
Benching with cat.
Benchmark 1: hermit.com with cat
  Time (mean ± σ):     1737.8 ms ± 103.2 ms    [User: 1725.2 ms, System: 12.1 ms]
  Range (min … max):   1595.7 ms … 1915.7 ms    10 runs

Benching with count_vowels.
Benchmark 1: hermit.com with count_vowels
  Time (mean ± σ):     50272.0 ms ± 2673.0 ms    [User: 50248.9 ms, System: 19.0 ms]
  Range (min … max):   47441.7 ms … 54378.3 ms    10 runs

Benching with cowsay.
Benchmark 1: hermit.com with cowsay
  Time (mean ± σ):     30065.3 ms ± 1294.7 ms    [User: 30047.6 ms, System: 15.0 ms]
  Range (min … max):   28296.4 ms … 32019.9 ms    10 runs
```
    
- Release mode
```log
Benching with cat.
Benchmark 1: hermit.com with cat
  Time (mean ± σ):      46.0 ms ±   4.6 ms   [User: 44.2 ms, System: 1.8 ms]
  Range (min … max):    40.0 ms …  60.3 ms    67 runs

Benching with count_vowels.
Benchmark 1: hermit.com with count_vowels
  Time (mean ± σ):     795.7 ms ±  33.1 ms    [User: 789.6 ms, System: 6.0 ms]
  Range (min … max):   734.3 ms … 841.0 ms    10 runs

Benching with cowsay.
Benchmark 1: hermit.com with cowsay
  Time (mean ± σ):     472.0 ms ±  18.6 ms    [User: 471.0 ms, System: 1.0 ms]
  Range (min … max):   447.8 ms … 508.2 ms    10 runs
```

## Requirements

- Install [Hyperfine](https://github.com/sharkdp/hyperfine) : `cargo install --locked hyperfine` 

  More info can be found in [installation docs](https://github.com/sharkdp/hyperfine#installation).

- Build Hermit, preferably in release mode.

## Usage

### Hermit-cli

Run `./benchmarks/bench-cli.sh`, this will run benchmarks using hermit-cli with available samples.

Default samples used during the bench :
- [cat](/src/cat/)
- [count_vowels](/src/count_vowels/)
- [cowsay](/src/cowsay/)

You can provide your own samples to benchmark against it :
- Create a folder under `benchmarks/bench-cli/custom/` with the '$name' of your sample (ex.: benchmarks/custom/my_cli)
- Create a HermitFile under `benchmarks/bench-cli/custom/$name/` (ex.: benchmarks/custom/my_cli/HermitFile)
- Run `./benchmarks/bench-cli.sh --only-custom`

### Cli binaires produced by Hermit-cli

Run `./benchmarks/bench-artifacts.sh`, this will run benchmarks using cli binaries produced by hermit-cli.

Default samples used during the bench :
- [cat](/src/cat/)
- [count_vowels](/src/count_vowels/)
- [cowsay](/src/cowsay/)

You can benchmark your own samples, follow instructions provided by: `./benchmarks/bench-artifacts.sh --only-custom`