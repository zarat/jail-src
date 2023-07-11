#include "Jail.h"

#include <string>

namespace JAIL {

    void scCharParse(JObject *c, void *) {

        // an int is a char..
        if(c->getParameter("str")->isInt()) {
            int str = c->getParameter("str")->getInt();
            c->getReturnVar()->setChar(str);
        }
        
        // get the first character
        else if(c->getParameter("str")->isString()) {
            std::string str = c->getParameter("str")->getString();
            int val = str[0];
            c->getReturnVar()->setChar(val);
        }
        
        // a char is a char
        else if(c->getParameter("str")->isChar()) {
            char str = c->getParameter("str")->getChar();
            c->getReturnVar()->setChar(str);
        }
        
        // to prevent errors we always set a value
        // lets make it undefined if none has matched
        else
            c->getReturnVar()->setUndefined();

    }
    
    void registerChar(JInterpreter *interpreter) {
        
        interpreter->addNative("function Char.parse(str)", scCharParse, 0);

    }

}