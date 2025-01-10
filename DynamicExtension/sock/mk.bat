g++ -c main.cpp -L. -ljail -lws2_32 
g++ -shared main.o -o Socket.dll -L. -ljail -lws2_32
copy Socket.dll ..\Socket.dll
pause