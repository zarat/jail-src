@echo off


del *.o 


mkdir libs


echo compiling std
g++ -c std.cpp 
windres std_versioninfo.rc std_versioninfo.o
g++ -shared std.o std_versioninfo.o -o libs/std.dll


echo compiling math
g++ -c math.cpp
windres math_versioninfo.rc math_versioninfo.o
g++ -shared math.o math_versioninfo.o -o libs/math.dll


echo compiling net
g++ -c net.cpp -lws2_32
windres net_versioninfo.rc net_versioninfo.o
g++ -shared net.o net_versioninfo.o -o libs/net.dll -lws2_32


echo compiling executable
windres versioninfo.rc versioninfo.o
gcc main.c versioninfo.o -o jail.exe -Wall -L. -lJail -lshlwapi


del *.o


pause