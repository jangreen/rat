test() {
    name=$1
    result=$2
    passed=$(./build/CatInfer "./tests/${name}" | grep $result | wc -l)
    space='              '
    printf "%s%s%s\n" $name "${space:${#name}}" $passed
}

# no assumptions
test test1 True
test test2 True
test test3 True
test test4 True
test test9 True
test hard True
test test-inv True
test test2f False

# with assumptions
test test0 True