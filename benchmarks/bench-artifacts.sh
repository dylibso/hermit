#!/bin/bash
#set -x	

only_custom=false
remove_artifacts_after_bench=false
script_folder_name=$(basename "$(dirname "$(readlink -f "$0")")")/bench-artifacts
mkdir -p $script_folder_name
#uncomment line below if you wan't to remove
#stored bench stats from previous runs
#rm -rf $script_folder_name/*.json

# Iterate through the command-line arguments
for arg in "$@"; do
    if [ "$arg" == "--only-custom" ]; then
        only_custom=true
        break
    fi
done

run_hyperfine(){
    #https://github.com/sharkdp/hyperfine/issues/94#issuecomment-432708897
    #samples are small, silence the warning msg.
    export_file="benchmarks/bench-artifacts/benchmark_$1_$(date +%s%3N).json"
    hyperfine \
        --export-json="$export_file" \
        -N \
        --min-runs 10 \
        --warmup=3 \
        --time-unit=millisecond \
        --command-name="bench of $1" "$2" 2> /dev/null
}

# Check if --only-custom was passed
if [ "$only_custom" == false ]; then
    # Benchmark build/cat.hermit.com, build/count_vowels.hermit.com and  build/cowsay.hermit.com examples.
    run_hyperfine "Cat" "build/cat.hermit.com src/cat/cat.c"
    run_hyperfine "Count_vowels" "echo eeeUIaoopaskjdfhiiioozzmmmwze | build/count_vowels.hermit.com"
    run_hyperfine "Cowsay" "build/cowsay.hermit.com Hermooooooooot"
fi

if [ "$only_custom" == true ]; then
cat <<EOF
# to benchmark using custom artifact
# place your cli binary in "./benchmarks/bench-artifacts/custom/"
# set values of "example_name" and "cmd"
# run commands below
export example_name="my_cli"
export cmd="path/to/my_cli.com params of my_cli.com"
export stats_file="benchmarks/bench-artifacts/benchmark_\$example_name_\$(date +%s%3N).json"
hyperfine \\
    --export-json="\$stats_file" \\
    --warmup=3 \\
    --time-unit=millisecond \\
    --command-name="bench of \$example_name" "\$cmd" 
EOF
fi