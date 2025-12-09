g++ -c main.cpp -I. -L. -lJail -lgdiplus -lole32 -std=c++14
g++ -static-libgcc -static-libstdc++ -shared main.o -o GDI.dll -I. -L. -lJail -lgdiplus -lole32 -std=c++14

pause
