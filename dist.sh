rm -rf $1
mkdir $1
cp *.h *.c $1
tar -zcvf $1.tar.gz $1
rm -rf $1
