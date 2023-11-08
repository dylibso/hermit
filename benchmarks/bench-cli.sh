#!/bin/bash
#set -x	

only_custom=false
remove_artifacts_after_bench=false
script_folder_name=$(basename "$(dirname "$(readlink -f "$0")")")/bench-cli
mkdir -p $script_folder_name
rm -rf $script_folder_name/*.com
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

# Check if --only-custom was passed
if [ "$only_custom" == false ]; then
    # Benchmark build/hermit.com using `cat`, `count_vowels` and `cowsay` examples.
    examples_paths=("src/cat" "src/count_vowels" "src/cowsay")
    for example_path in "${examples_paths[@]}"; do
        example_name=$(basename "$example_path")
        printf "Benching with ${example_name} ${example_path}.\n"

        export_file="${script_folder_name}/benchmark_${example_name}_$(date +%s%3N).json"
        hyperfine \
            --export-json="$export_file" \
            --warmup=3 \
            --time-unit=millisecond \
            --command-name="hermit.com with ${example_name}" "build/hermit.com -f ${example_path}/Hermitfile -o ${script_folder_name}/${example_name}.com" \
            --cleanup "(${remove_artifacts_after_bench} && rm -rf ${script_folder_name}/${example_name}.com) || true"
    done

fi

# Benchmark build/hermit.com using custom examples.
if [ "$only_custom" == true ]; then
    custom_examples_paths=($(ls -d $script_folder_name/custom/* 2>/dev/null))
    for example_path in "${custom_examples_paths[@]}"; do
        example_name=$(basename "$example_path")
        printf "Benching with ${example_name}.\n"

        export_file="${example_path}/benchmark_${example_name}_$(date +%s%3N).json"
        hyperfine \
            --export-json="$export_file" \
            --warmup=3 \
            --time-unit=millisecond \
            --command-name="hermit.com with ${example_name}" "build/hermit.com -f ${example_path}/Hermitfile -o ${example_path}/${example_name}.com" \
            --cleanup "(${remove_artifacts_after_bench} && rm -rf ${example_path}/${example_name}.com) || true"
    done
fi