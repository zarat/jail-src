#include <sstream>
#include <fstream>
#include <map>

#ifdef _WIN32
#include <windows.h> 
#endif

#include <stdio.h>
#include <string.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "Jail.h"

using namespace std;

char* readCode(char* file) {
    
    FILE* f;
    f = fopen(file, "r");
    fseek(f, 0L, SEEK_END); 
    int len = ftell(f); 
    rewind(f);
    char c, buffer[len];    
    int i = 0;
    while( fread(&c, sizeof(char), 1, f) > 0 ) 
        buffer[i++] = c;
    fclose(f);
    buffer[i] = '\0';
    return strdup(buffer);
    
}

void eval(JAIL::JObject *c, void *data) {
    JAIL::JInterpreter *interpreter; // = reinterpret_cast<JAIL::JInterpreter *>(data);
    std::string code = c->getParameter("code")->getString();
    JAIL::JLink result = interpreter->evaluateComplex(code);
    c->setReturnVar(result.var);       
}

void exec(JAIL::JObject *c, void *data) {
    JAIL::JInterpreter *interpreter = reinterpret_cast<JAIL::JInterpreter *>(data);
    std::string code = c->getParameter("code")->getString();
    interpreter->execute(code);       
}

void debug(JAIL::JObject *c, void *data) {
    std::ostringstream oss;
    c->getParameter("a")->getJSON(oss);
    c->getReturnVar()->setString(oss.str());       
}

void print(JAIL::JObject *c, void *data) {
    printf("%s", c->getParameter("str")->getString().c_str());       
}

int main(int argc, char **argv) {                         


    bool isCaseInsensitive = false;
    int opt;
    enum { EXEC_MODE, , INTERACTIVE_MODE, SCRIPT_MODE } mode = SCRIPT_MODE;

    while ((opt = getopt(argc, argv, "ei")) != -1) {
        switch (opt) {
        case 'e': mode = EXEC_MODE; break;
        case 'i': mode = INTERACTIVE_MODE; break;
        default:
            //fprintf(stderr, "Usage: %s [-e] [file...]\n", argv[0]);
            //exit(EXIT_FAILURE);
            break;
        }
    }
    
    if(argc < 2) {
        printf("Usage: jail <scriptfile>\n");
        return 1;
    }
    
    // create a new interpreter instance
    JAIL::JInterpreter interpreter;
    
    // parse command line arguments and add as global variable "args"
    JAIL::JObject *args = new JAIL::JObject();
    args->setArray();
    for(int i = 0; i < argc; i++) 
        args->setArrayIndex(i, new JAIL::JObject(argv[i]));
    interpreter.root->addChild("args", args);
    
    // register buildt-in classes
    JAIL::registerChar(&interpreter);
    JAIL::registerInteger(&interpreter);
    JAIL::registerDouble(&interpreter);
    JAIL::registerString(&interpreter);
    JAIL::registerArray(&interpreter);
    JAIL::registerObject(&interpreter);
    
    // register custom functions 
    JAIL::registerFunctions(&interpreter);
    JAIL::registerMathFunctions(&interpreter);
    
    // register custom inline functions
    interpreter.addNative("function jail.eval(code)", eval, &interpreter);
    interpreter.addNative("function jail.exec(code)", exec, &interpreter);
    interpreter.addNative("function jail.debug(a)", debug, 0);
    interpreter.addNative("function print(str)", print, 0);
    
    char *code;
    
    if(mode == EXEC_MODE) {
        code = argv[2];
    } 
    
    else
        code = readCode(argv[1]);

    // add a global variable for testing purposes
    //interpreter.root->addChild("returnvalue", new JAIL::JObject("0", JAIL::VARIABLE_INTEGER));

    try {
        
        interpreter.execute(code);

    } catch (JAIL::Exception *e) { 
    
        printf("%s", (char *)e->text.c_str());
        return 0;
        
    } 
    
    // get the global variable from script context
    //printf("retVal: %d", interpreter.root->getParameter("returnvalue")->getInt());
    
    return 1;  
                       
}   
