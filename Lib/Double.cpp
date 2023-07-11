#include "Jail.h"

#include <string>

namespace JAIL {

    void scDoubleParse(JObject *c, void *) {
    
        // usually it will always be a string
        if(c->getParameter("str")->isString()) {
            
            std::string str = c->getParameter("str")->getString();
            double val = strtod(str.c_str(), NULL);
            c->getReturnVar()->setDouble( val );
            
        }
        
        else if(c->getParameter("str")->isInt()) {
            
            int str = c->getParameter("str")->getInt();
            double val = (double)str;
            c->getReturnVar()->setDouble( val );
            
        }
        
        else if(c->getParameter("str")->isChar()) {
            
            int str = c->getParameter("str")->getChar();
            double val = (double)str;
            c->getReturnVar()->setDouble( val );
            
        }
        
        else if(c->getParameter("str")->isDouble()) {
            
            double d = c->getParameter("str")->getDouble();
            c->getReturnVar()->setDouble( d );
            
        }
        
        // set it to undefined if none has matched
        else
            c->getReturnVar()->setUndefined();        
        
    }
    
    void registerDouble(JInterpreter *interpreter) {
        
        interpreter->addNative("function Double.parse(str)", scDoubleParse, 0);
        
    }

}