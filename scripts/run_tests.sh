#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
RAT="${RAT:-$DIR/../cmake-build-release/rat}"
TIMEOUT=3600

### Function to execute a set of tests (given as argument)
function exec_tests() {
    local tests=("$@")
    for test in "${tests[@]}"
    do
        extension="${test##*.}"
        ### skip .md and .cat extensions
        if [[ "${extension}" == "md" || "${extension}" == "cat" ]]; then
            continue
        fi
        output=$(timeout "${TIMEOUT}" /usr/bin/time "${RAT}" "${test}" 2>&1)
        exit_code=$?
        time=$(echo "${output}" | awk '/user/ { print $3 }')

        if [[ ${exit_code} -eq 0 ]]; then
            result=$(echo "${output}" | awk '/Answer: [TF]/ { printf "%s",$6" " }')
        elif [[ ${exit_code} -eq 124 ]]; then
            time="${TIMEOUT}.00"
            result="Timeout"
        else
            result="Error"
        fi

        printf "| %-40s | %-10s | %-10s |\n" "${test##*/}" "${time}" "${result}"
    done
}

### Actual test cases

echo "================================================================="
echo "================ Running Kater equivalence tests ================"
echo "================================================================="
kater_equiv=(
    "eq-co_1" "eq-co_2" "eq-eco-paper" "eq-ra1-ra2" "eq-ra1-ra3_1" 
    "eq-ra1-ra3_2" "eq-rc11-rc112" "eq-sc-scfm_1" "eq-sc-scfm_2" 
    "eq-tso-tsofm"
    )
kater_equiv=("${kater_equiv[@]/%/.kat}")
kater_equiv=("${kater_equiv[@]/#/${DIR}/../evaluation/kater_translated/tests/equivalence/}")

exec_tests "${kater_equiv[@]}"

echo "================================================================="
echo "================ Running Kater compilation tests ================"
echo "================================================================="
kater_compilation=(
    ### True
    "comp-imm-arm8" "comp-imm-tso" "comp-power-power-simpl" 
    "comp-rc11-arm8" "comp-rc11-imm" "comp-rc11-power-weak" 
    "comp-rc11f-power-simpl-full" "comp-rc11rw-power-simpl-full" 
    "comp-rc11s-arm8" 
    ### False
    "comp-c11-power-simpl" "comp-c11-power-weak"
    )
kater_compilation=("${kater_compilation[@]/%/.kat}")
kater_compilation=("${kater_compilation[@]/#/${DIR}/../evaluation/kater_translated/tests/compilation/}")

exec_tests "${kater_compilation[@]}"

echo "================================================================="
echo "============== Running generic memory model tests ==============="
echo "================================================================="
mm_tests=(
    "generic/uniproc_properties" "arm8/arm_mca" "arm8/arm_oota" 
    "imm/imm_mca" "imm/imm_oota" "tso/tso_mca" "tso/tso_oota"
    )
mm_tests=("${mm_tests[@]/#/${DIR}/../evaluation/}")

exec_tests "${mm_tests[@]}"

echo "================================================================="
echo "====================== Running LKMM tests ======================="
echo "================================================================="
lkmm_tests=("${DIR}/../evaluation/lkmm/*")

exec_tests "${lkmm_tests[@]}"
