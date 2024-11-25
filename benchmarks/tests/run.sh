time=0
layout="%s%s%s"

result() {
    output=$1 
    passed=$(echo $output | grep $result | wc -l)
    durations=$(echo $output | grep -Eo 'Duration: *[0-9\.]+' | grep -o '[0-9\.]*')
    space='                  '
    printf "$layout" $name "${space:${#name}}" $passed

    while IFS= read -r line; do
      printf "  %s" $line
      time=$(echo $time + $line | bc)
    done <<< "$durations"

    printf "\n"
}

run() {
    name=$1
    result=$2
    output=$(./cmake-build-relwithdebinfo/rat "./benchmarks/${name}")
    result "$output"
}

# sets
run tests/set-algebra-laws True
run tests/set-emptiness True
run tests/set-base True
run tests/set True
run tests/set2 True

# no assumptions
run tests/hard True
run tests/norm True
run tests/test-inv True
run tests/test1 True
run tests/test2 True
run tests/test2f False
run tests/test3 True
run tests/test3f False
run tests/test4 True
run tests/test4f False
run tests/test9 True
run tests/id5 True
run tests/id6 True
run tests/id7 True
run tests/t True
run tests/t2 True
run tests/t3 True
run tests/t4 True
run tests/topevent False
run tests/diamond True

# with assumptions
run tests/test0 True
run tests/test5 True
run tests/test6 True
run tests/test7 True
run tests/id True
run tests/id2 True
run tests/id3 True
run tests/id4 True
run tests/setid True
run tests/setid2 False
run tests/setDist True
run tests/topEvent False
run tests/multiple True
run tests/tt True
run tests/tt2 True
run tests/eco1f False
run tests/eco2f False
run tests/eco3f False
run tests/eco True
run tests/eco2 True

# memorymodels
run memorymodels/uniproc+rfi_po True

# kater
run kater/kater_3_1-eco True
#run kater/kater_3_1-eco-n True
run kater/kater_3_2-ra True
run kater/kater_3_3-ra True
#run kater/kater_3_3-ra-2 True
#run kater/kater_3_3-ra-n True

printf "\nElapsed time: %s\n" $time