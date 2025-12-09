g++ -c mysql_connector.cpp example.cpp -Iinclude -Llib -lmysql32
g++ -static-libgcc -static-libstdc++ -shared mysql_connector.o example.o -o mysql.dll -Iinclude -Llib -L. -L.. -ljail -l:libmysql32.lib

@rem g++ mysql_connector.cpp example.cpp -o mysql -Iinclude -Llib -l:libmysql32.lib

copy .\mysql.dll ..\mysql.dll
copy .\lib\libmysql32.dll ..\libmysql.dll

pause
