src="Char.cpp Integer.cpp Double.cpp String.cpp Array.cpp Object.cpp Jail_Functions.cpp Jail_MathFunctions.cpp"

obj="Char.o Integer.o Double.o String.o Array.o Object.o Jail_Functions.o Jail_MathFunctions.o"

g++ -c Jail.cpp $src -std=c++11 -I.

ar crf libJail.a Jail.o $obj

cp libJail.a ../Static
