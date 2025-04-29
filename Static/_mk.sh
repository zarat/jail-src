# This script will create a binary "jail", statically compiled using "libJail.a"
# apt install g++ dos2unix -y
# dos2unix _mk.sh
# sh _mk.sh

g++ main.cpp -o jail -I. -L. -lJail
