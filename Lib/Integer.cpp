#include "Jail.h"

#include <stdio.h>
#include <time.h>

namespace JAIL {

// helper
int generateRandomNumber() {
    
    static int initialized = 0;
    if (!initialized) {
        srand(time(NULL));
        initialized = 1;
    }  
    int randomNumber = rand();
    return randomNumber;
    
}

void scMathRand(JObject *c, void *) {
    
    srand(generateRandomNumber());    
    int i = generateRandomNumber();    
    c->getReturnVar()->setInt(i);
    
}

void scMathRandInRange(JObject *c, void *) {
    int min = c->getParameter("min")->getInt();
    int max = c->getParameter("max")->getInt();       
    srand(generateRandomNumber());        
    int val = min + (int)( rand() % (1 + (max - min) ) );
    c->getReturnVar()->setInt(val);
}

void scIntegerParseInt(JObject *c, void *) {
    
    if(c->getParameter("str")->isString()) {
        std::string str = c->getParameter("str")->getString();
        int val = strtol(str.c_str(), 0, 0);
        c->getReturnVar()->setInt( val );
    }
    
    else if(c->getParameter("str")->isDouble()) {
        std::string s = std::to_string( c->getParameter("str")->getDouble() );
        int i = strtol( s.c_str(), 0, 0 );
        c->getReturnVar()->setInt( i );
    }
    
    else if(c->getParameter("str")->isInt()) {
        int str = c->getParameter("str")->getInt();
        c->getReturnVar()->setInt(str);
    }
    
    else if(c->getParameter("str")->isChar()) {
        int str = c->getParameter("str")->getChar();
        c->getReturnVar()->setInt(str);
    }
    
    else
        c->getReturnVar()->setUndefined();
}

void registerInteger(JInterpreter *interpreter) {

    interpreter->addNative("function Integer.rand()", scMathRand, 0);
    interpreter->addNative("function Integer.range(min, max)", scMathRandInRange, 0);    
    //interpreter->addNative("function Integer.fromChar(ch)", scCharToInt, 0);
    interpreter->addNative("function Integer.parse(str)", scIntegerParseInt, 0); // string to int

}

}
