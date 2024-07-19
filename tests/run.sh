time=0
layout="%s%s%s       %s   %s   %s\n"

result() {
    output=$1 
    passed=$(echo $output | grep $result | wc -l)
    durations=$(echo $output | grep -Eo 'Duration: *[0-9\.]+' | grep -o '[0-9\.]*')
    space='                  '
    printf "$layout" $name "${space:${#name}}" $passed $durations

    while IFS= read -r line; do
    time=$(echo $time + $line | bc)
    done <<< "$durations"
}

test() {
    name=$1
    result=$2
    output=$(./cmake-build-relwithdebinfo/CatInfer "./tests/${name}")
    result "$output"
}

proof() {
    name=$1
    result=$2
    output=$(./cmake-build-relwithdebinfo/CatInfer "./proofs/${name}")
    result "$output"  
}


# no assumptions
test hard True
test norm True
test test-inv True
test test1 True
test test2 True
test test2f False
test test3 True
test test3f False
test test4 True
test test4f False
test test9 True
test id5 True
test id6 True
test id7 True

# with assumptions
test test0 True
test test5 True
test test6 True
test test7 True
test id True
test id2 True
test id3 True
test id4 True
test eco1f False
test eco2f False
test eco3f False
test eco True
test eco2 True

# proofs
proof kater_3_1-eco True
proof kater_3_1-eco-n True
proof kater_3_2-ra True
proof kater_3_3-ra True
#proof kater_3_3-ra-2 True
#proof kater_3_3-ra-n True

printf "\nElapsed time: %s\n" $time