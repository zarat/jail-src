## apt install dos2unix
# dos2unix mk.sh
# sh mk.sh

g++ -c Jail.cpp -std=c++11 -I.
ar crf libJail.a Jail.o
cp libJail.a ../Static
