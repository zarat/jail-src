#include "Jail.h"

#include <string>

#include <iostream> 

using namespace JAIL;

extern "C" {

    __declspec(dllexport) void scReadLine(JAIL::JObject *c, void *) {

        std::string str; 
        std::getline(std::cin, str);
        c->getReturnVar()->setString(str.c_str());

    }
	
	__declspec(dllexport) void scPrintLine(JAIL::JObject *c, void *) {

        const char *str = c->getParameter("v")->getString().c_str();
		printf("Test.printline(%s)", str);

    }

    __declspec(dllexport) void registerLib(JAIL::JInterpreter *interpreter) {
        
        interpreter->addNative("function Test.readline()", scReadLine, 0);
		interpreter->addNative("function Test.printline(v)", scPrintLine, 0);

    }

}
