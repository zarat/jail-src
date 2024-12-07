#include "Jail.h"

#include <string>
#include <iostream> 

using namespace JAIL;

extern "C" {

    __declspec(dllexport) void scPrompt(JAIL::JObject *c, void *) {

        printf("%s", c->getParameter("str")->getString().c_str());
		std::string str; 
        std::getline(std::cin, str);
        c->getReturnVar()->setString(str.c_str());

    }

    __declspec(dllexport) void registerLib(JAIL::JInterpreter *interpreter) {
        
        interpreter->addNative("function Test.prompt(str)", scPrompt, 0);

    }

}
