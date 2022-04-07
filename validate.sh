#!/bin/bash
set -e

student_stat_dir=student_outs
benchmarks=( bwaves603 gcc602 mcf605 perlbench600 )
configs=( big med med_noint tiny )

flags_big='-R 64 -S 4 -C 15 -A 4 -M 3 -L 3 -F 8 -W 16'
flags_med='-R 32 -S 3 -C 13 -A 3 -M 2 -L 2 -F 4 -W 8'
flags_med_noint='-R 32 -S 3 -C 13 -A 3 -M 2 -L 2 -F 4 -W 8 -D'
flags_tiny='-R 16 -S 2 -C 10 -A 1 -M 1 -L 1 -F 2 -W 2'

name_big=Big
name_med=Medium
name_med_noint='Medium without interrupts'
name_tiny=Tiny

banner() {
    local message=$1
    printf '%s\n' "$message"
    yes = | head -n ${#message} | tr -d '\n'
    printf '\n'
}

student_stat_path() {
    local config=$1
    local benchmark=$2

    printf '%s' "${student_stat_dir}/${config}_${benchmark}.out"
}

ta_stat_path() {
    local config=$1
    local benchmark=$2

    printf '%s' "ref_outs/${config}_${benchmark}.out"
}

benchmark_path() {
    benchmark=$1
    printf '%s' "traces/${benchmark}_2M.trace"
}

human_friendly_config() {
    local config=$1
    local name_var=name_$config
    printf '%s' "${!name_var}"
}

generate_stats() {
    local config=$1
    local benchmark=$2

    local flags_var=flags_$config
    if ! bash run.sh ${!flags_var} -I "$(benchmark_path "$benchmark")" &>"$(student_stat_path "$config" "$benchmark")"; then
        printf 'Exited with nonzero status code. Please check the log file %s for details\n' "$(student_stat_path "$config" "$benchmark")"
    fi
}

generate_stats_and_diff() {
    local config=$1
    local benchmark=$2

    printf '==> Running %s... ' "$benchmark"
    generate_stats "$config" "$benchmark"
    if diff -u "$(ta_stat_path "$config" "$benchmark")" "$(student_stat_path "$config" "$benchmark")"; then
        printf 'Matched!\n'
    else
        local flags_var=flags_$config
        printf '\nPlease examine the differences printed above. Benchmark: %s. Config name: %s. Flags to procsim used: %s\n\n' "$benchmark" "$config" "${!flags_var} -I $(benchmark_path "$benchmark")"
    fi
}

main() {
    mkdir -p "$student_stat_dir"

    if [[ $# -gt 0 ]]; then
        local use_configs=( "$@" )

        for config in "${use_configs[@]}"; do
            local flags_var=flags_$config
            if [[ -z ${!flags_var} ]]; then
                printf 'Unknown configuration %s. Available configurations:\n' "$config"
                printf '%s\n' "${configs[@]}"
                return 1
            fi
        done
    else
        local use_configs=( "${configs[@]}" )
    fi

    local first=1
    for config in "${use_configs[@]}"; do
        if [[ $first -eq 0 ]]; then
            printf '\n'
        else
            local first=0
        fi

        banner "Testing $(human_friendly_config "$config")..."

        local flags_var=flags_$config
        printf 'procsim flags: %s\n\n' "${!flags_var}"

        for benchmark in "${benchmarks[@]}"; do
            generate_stats_and_diff "$config" "$benchmark"
        done
    done
}

main "$@"
