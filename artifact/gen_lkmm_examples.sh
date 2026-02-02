#!/bin/bash

RAT="/root/rat/build/rat"
OUTPUTDIR="/root/rat/artifact/output/"

# Safety timeout (should not be reached)
TIMEOUT=60

### Execute rat on parameter 1 and generate counterexample with name given by param 2
function exec_rat() {
    test=$1
    filename=$2
    # Run RAT
    echo "--- Running ${test}"
    output=$(timeout "${TIMEOUT}" "${RAT}" "${test}" 2>&1)
    exit_code=$?
    if [[ ${exit_code} -eq 0 ]]; then
        # WARNING: We assume the result was "False"
        outputfile="${OUTPUTDIR}${filename}"
        cp "/root/rat/output/counterexample.dot" "${outputfile}.dot"
        dot -Tpng "${outputfile}.dot" > "${outputfile}.png"
        echo "Output: ${outputfile}"
    elif [[ ${exit_code} -eq 124 ]]; then
        printf "Timeout ${TIMEOUT}.00\n"
    else
        printf "Error\n"
    fi
}

lkmm_dir="/root/rat/evaluation/lkmm/"
exec_rat "${lkmm_dir}lkmm_v00_in_v01" "fig05"
exec_rat "${lkmm_dir}lkmm_ppo_v02_vs_v03" "fig09"
