g++ -c Test.cpp
g++ -static-libgcc -static-libstdc++ -shared Test.o -o Test.dll -L. -ljail
