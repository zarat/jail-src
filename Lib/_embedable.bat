set src=Char.cpp Integer.cpp Double.cpp String.cpp Array.cpp Object.cpp Jail_Functions.cpp Jail_MathFunctions.cpp

set obj=Char.o Integer.o Double.o String.o Array.o Object.o Jail_Functions.o Jail_MathFunctions.o

g++ -c Jail.cpp %src% main_embedable.cpp

g++ -shared Jail.o %obj% main_embedable.o -o jail.dll

pause