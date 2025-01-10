g++ -c pugixml.cpp main.cpp -L. -ljail --std=c++17
g++ -shared pugixml.o main.o -o XML.dll -L. -ljail --std=c++17

copy XML.dll ..\XML.dll

pause