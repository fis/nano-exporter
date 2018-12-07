#! /bin/bash

failed=0

for suite in "$@"; do
    echo "$suite:"
    if ./$suite; then
        echo "...pass"
    else
        echo "...FAIL"
        failed=1
    fi
done

if (($failed)); then
    printf "\x1b[31;1mTESTS FAILED\x1b[0m\n"
    exit 1
fi

printf "\x1b[32;1mall tests passed\x1b[0m\n"
exit 0
