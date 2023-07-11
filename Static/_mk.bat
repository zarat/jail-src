del *.exe

@rem g++ -c Jail_Functions.cpp -I.
@rem ar rcs functions.a Jail_Functions.o 

@rem g++ -c Jail_MathFunctions.cpp -I.
@rem ar rcs mathfunctions.a Jail_MathFunctions.o

g++ -o jail.exe main.cpp jail.a 

pause