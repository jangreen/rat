test() {
    name=$1
    result=$2
    passed=$(./build/CatInfer "./tests/${name}" | grep $result | wc -l)
    space='              '
    printf "%s%s%s\n" $name "${space:${#name}}" $passed
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

# with assumptions
test test0 True
test test5 True
test test6 True
test test7 True
test test10 True
