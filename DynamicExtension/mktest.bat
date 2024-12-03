g++ -c Test.cpp
g++ -shared Test.o -o Test.dll -L. -ljail

g++ -c Jail_Functions.cpp
g++ -shared Jail_Functions.o -o Std.dll -L. -ljail

g++ -c Jail_MathFunctions.cpp
g++ -shared Jail_MathFunctions.o -o Math.dll -L. -ljail
