g++ -c main.cpp -I. -L. -lJail -lgdiplus -lole32 -std=c++14
g++ -shared main.o -o GDI.dll -I. -L. -lJail -lgdiplus -lole32 -std=c++14

pause