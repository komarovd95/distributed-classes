rm events.log
rm pipes.log
rm a.out

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$PWD"
clang-3.5 -std=c99 -Wall -pedantic *.c -L. -lruntime
LD_PRELOAD=$PWD/libruntime.so ./a.out -p $1 $2
