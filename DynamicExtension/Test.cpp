#include "Jail.h"

#include <string>

using namespace JAIL;

extern "C" {

    __declspec(dllexport) void scTest(JAIL::JObject *c, void *) {

        printf("Hello moon");

    }

    __declspec(dllexport) void registerTest(JAIL::JInterpreter *interpreter) {
        
        interpreter->addNative("function Test.test(v)", scTest, 0);

    }

}
