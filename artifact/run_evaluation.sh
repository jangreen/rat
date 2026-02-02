#!/bin/bash

RAT="/root/rat/build/rat"
KATER="/root/kater/Release/kater"
EVALDIR="/root/rat/evaluation"

TIMEOUT=3600

### Functions to execute a set of tests (given as argument)
function exec_rat() {
    echo "---------- Running RAT ----------"
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
        time=$(echo "${output}" | awk '/user/ { print substr($1, 1, length($1)-4) }')

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

function exec_kater() {
    if [ ! -f ${KATER} ]; then
        echo "Couldn't find Kater executable: skipping Kater"
        return
    fi
    echo "---------- Running KATER ----------"
    local tests=("$@")
    for test in "${tests[@]}"
    do
        extension="${test##*.}"
        ### skip .md and .cat extensions
        if [[ "${extension}" == "md" || "${extension}" == "cat" ]]; then
            continue
        fi
        output=$(timeout "${TIMEOUT}" /usr/bin/time "${KATER}" "${test}" 2>&1)
        exit_code=$?
        time=$(echo "${output}" | awk '/user/ { print substr($1, 1, length($1)-4) }')

        if [[ ${exit_code} -eq 0 ]]; then
            result="True"
        elif [[ ${exit_code} -eq 6 ]]; then
            result="False"
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
equiv_rat=(
    "eq-co_1" "eq-co_2" "eq-eco-paper" "eq-ra1-ra2" "eq-ra1-ra3_1" 
    "eq-ra1-ra3_2" "eq-rc11-rc112" "eq-sc-scfm_1" "eq-sc-scfm_2" 
    "eq-tso-tsofm"
    )
equiv_rat=("${equiv_rat[@]/%/.kat}")
equiv_rat=("${equiv_rat[@]/#/${EVALDIR}/kater_translated/tests/equivalence/}")

equiv_kater=(
    "eq-co" "eq-eco-paper" "eq-ra1-ra2" "eq-ra1-ra3" "eq-rc11-rc112" 
    "eq-sc-scfm" "eq-tso-tsofm"
)
equiv_kater=("${equiv_kater[@]/%/.kat}")
equiv_kater=("${equiv_kater[@]/#/${EVALDIR}/kater_original/tests/equivalence/}")

exec_kater "${equiv_kater[@]}"
exec_rat "${equiv_rat[@]}"

echo "================================================================="
echo "================ Running Kater compilation tests ================"
echo "================================================================="
comp_tests=(
    ### True
    "comp-imm-arm8" "comp-imm-tso" "comp-power-power-simpl" 
    "comp-rc11-arm8" "comp-rc11-imm" "comp-rc11-power-weak" 
    "comp-rc11f-power-simpl-full" "comp-rc11rw-power-simpl-full" 
    "comp-rc11s-arm8" 
    ### False
    "comp-c11-power-simpl" "comp-c11-power-weak"
    )
comp_tests=("${comp_tests[@]/%/.kat}")
comp_tests_kater=("${comp_tests[@]/#/${EVALDIR}/kater_original/tests/compilation/}")
comp_tests_rat=("${comp_tests[@]/#/${EVALDIR}/kater_translated/tests/compilation/}")

exec_kater "${comp_tests_kater[@]}"
exec_rat "${comp_tests_rat[@]}"

echo "================================================================="
echo "============== Running generic memory model tests ==============="
echo "================================================================="
mm_tests=(
    "generic/uniproc_properties" "arm8/arm_mca" "arm8/arm_oota" 
    "imm/imm_mca" "imm/imm_oota" "tso/tso_mca" "tso/tso_oota"
    )
mm_tests=("${mm_tests[@]/#/${EVALDIR}/}")

exec_rat "${mm_tests[@]}"

echo "================================================================="
echo "====================== Running LKMM tests ======================="
echo "================================================================="
lkmm_dir=${EVALDIR}/lkmm/*
lkmm_tests=(${lkmm_dir})

exec_rat "${lkmm_tests[@]}"
