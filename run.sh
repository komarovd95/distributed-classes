rm events.log
rm pipes.log

clang-3.5 -std=c99 -Wall -pedantic *.c
./a.out -p $1
