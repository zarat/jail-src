#include "Jail.h"
#include "DataTypes.h"
#include "Jail_Functions.h"
#include "Jail_MathFunctions.h"

#include <stdio.h>
#include <sstream>
#include <fstream>
#include <map>

#ifdef _WIN32
#include <windows.h>
#define EXPORT __declspec(dllexport)
typedef HANDLE lib_t;
#else
#include <dlfcn.h>
typedef void* lib_t;
#define EXPORT __attribute__ ((visibility ("default")))
#endif

using namespace std;

// test
// <ret>_<name>_<params>

typedef void (*VoidVoidCallBackFunction)(void *a); // like print which receives parameter but does not modify the return buffer
typedef void* (*NonVoidCallBackFunction)(); // like read which does NOT receive parameters but DOES modify the return buffer
typedef void* (*NonVoidVoidCallBackFunction)(void *a); // like atob which receives parameters and does modify the return buffer
typedef void* (*NonVoidVoidVoidCallBackFunction)(void *a, void *b); // like split(str, tok) which takes 2 parameter and does modify the return buffer
typedef void* (*NonVoidVoidVoidVoidCallBackFunction)(void *a, void *b, void *c);

std::map<char *, VoidVoidCallBackFunction> registeredVoidVoidCallBackFunctions; 
std::map<char *, NonVoidCallBackFunction> registeredNonVoidCallBackFunctions;
std::map<char *, NonVoidVoidCallBackFunction> registeredNonVoidVoidCallBackFunctions;
std::map<char *, NonVoidVoidVoidCallBackFunction> registeredNonVoidVoidVoidCallBackFunctions; // split(str, tok)
std::map<char *, NonVoidVoidVoidVoidCallBackFunction> registeredNonVoidVoidVoidVoidCallBackFunctions;

// void callback like print(any)
void voidVoidCallBack(JAIL::JObject *c, void *data) {    
    for(auto it = registeredVoidVoidCallBackFunctions.begin(); it != registeredVoidVoidCallBackFunctions.end(); it++)         
        if( it->first == c->getParameter("self")->getString() ) {                    
            if(c->getParameter("a")->isInt())
                (*it->second)( (void *)(char *)std::to_string(c->getParameter("a")->getInt()).c_str() );                                       
            else if(c->getParameter("a")->isDouble())                             
                (*it->second)( (void *)(char *)std::to_string(c->getParameter("a")->getDouble()).c_str() );                                                       
            else 
                (*it->second)( (void *)c->getParameter("a")->getString().c_str() );                                 
        }                        
}

// nv
// like read()
// we always expect a (void *) to a (char *)
void nonVoidCallBack(JAIL::JObject *c, void *data) {        
    char* buffer = (char *)malloc(4096 * 4096);             
    for(auto it = registeredNonVoidCallBackFunctions.begin(); it != registeredNonVoidCallBackFunctions.end(); it++) 
        if( it->first == c->getParameter("self")->getString() ) 
            strcpy(buffer, (char *)(void *)(*it->second)()); 
    c->getReturnVar()->setString( buffer );
    free(buffer);                                                                        
}

// nvv
// non void VOID callback like atob(any) => return
// we always send (char *) into and expect (void *)(char *) from the dll
void nonVoidVoidCallBack(JAIL::JObject *c, void *data) {        
    char *buffer = (char *)malloc(4096 * 4096);    
    for(auto it = registeredNonVoidVoidCallBackFunctions.begin(); it != registeredNonVoidVoidCallBackFunctions.end(); it++)         
        if( it->first == c->getParameter("self")->getString() ) {            
            if(c->getParameter("a")->isInt()) {
                char *asString = (char *)std::to_string( c->getParameter("a")->getInt() ).c_str();                
                strcpy(buffer, (char *)(void *)(*it->second)( (void *)asString ) );               
                c->getReturnVar()->setString( strdup(buffer) );                            
            }                                   
            else if(c->getParameter("a")->isDouble()) {                                      
                char *asString = (char *)std::to_string( c->getParameter("a")->getDouble() ).c_str();                
                strcpy(buffer, (char *)(void *)(*it->second)( (void *)asString ) );                
                c->getReturnVar()->setString( strdup(buffer) );                                                                
            }                                    
            else {                        
                strcpy(buffer, (char *)(void *)(*it->second)( (void *)c->getParameter("a")->getString().c_str() ));                
                c->getReturnVar()->setString( strdup(buffer) );                                
            }                                                    
        } 
    free(buffer);                   
}

// non void void VOID callback like split(str, tok)
// todo returnvar
void nonVoidVoidVoidCallBack(JAIL::JObject *c, void *data) {            
    char *a = (char *)malloc(4096 * 4096);
    char *b = (char *)malloc(4096 * 4096);               
    strcpy(a, (char *)c->getParameter("a")->getString().c_str());                      
    strcpy(b, (char *)c->getParameter("b")->getString().c_str());           
    char *buffer = (char *)malloc(4096 * 4096);             
    for(auto it = registeredNonVoidVoidVoidCallBackFunctions.begin(); it != registeredNonVoidVoidVoidCallBackFunctions.end(); it++) 
        if( it->first == c->getParameter("self")->getString() )                      
            strcpy(buffer, (char *)(void *)(*it->second)( (void *)a, (void *)b ) );                                           
    c->getReturnVar()->setString( strdup(buffer) );
    free(a);
    free(b);
    free(buffer);              
}

// non void void void VOID callback
// 3 parameter
void nonVoidVoidVoidVoidCallBack(JAIL::JObject *co, void *data) {            
    char *a = (char *)malloc(4096 * 4096);
    char *b = (char *)malloc(4096 * 4096);
    char *c = (char *)malloc(4096 * 4096);               
    strcpy(a, (char *)co->getParameter("a")->getString().c_str());                      
    strcpy(b, (char *)co->getParameter("b")->getString().c_str());
    strcpy(c, (char *)co->getParameter("c")->getString().c_str());           
    char *buffer = (char *)malloc(4096 * 4096);             
    for(auto it = registeredNonVoidVoidVoidVoidCallBackFunctions.begin(); it != registeredNonVoidVoidVoidVoidCallBackFunctions.end(); it++) 
        if( it->first == co->getParameter("self")->getString() )                      
            strcpy(buffer, (char *)(void *)(*it->second)( (void *)a, (void *)b, (void *)c ) );                                           
    co->getReturnVar()->setString( strdup(buffer) );
    free(a);
    free(b);
    free(c);
    free(buffer);              
}

void scEval(JAIL::JObject *c, void *data) {
    JAIL::JInterpreter *interpreter = reinterpret_cast<JAIL::JInterpreter *>(data);
    std::string str = c->getParameter("code")->getString();
    c->setReturnVar(interpreter->evaluateComplex(str).var);      
}

void scExec(JAIL::JObject *c, void *data) {
    JAIL::JInterpreter *interpreter = reinterpret_cast<JAIL::JInterpreter *>(data);
    std::string code = c->getParameter("code")->getString();
    interpreter->execute(code);     
}

void debug(JAIL::JObject *c, void *data) {
    std::ostringstream oss;
    c->getParameter("a")->getJSON(oss);
    c->getReturnVar()->setString(oss.str());       
}

JAIL::JInterpreter interpreter;
char *lastError;

extern "C" {

    // like print
    EXPORT void setVVCallBack(char *funcName, void (*funcPtr)(void *)) {    
        registeredVoidVoidCallBackFunctions.emplace((char*)funcName, funcPtr);
        std::stringstream ss;
        ss << "function " << funcName << "(a) { ext_vv(\"" << funcName << "\", a); };";
        interpreter.addNative("function ext_vv(self, a)", voidVoidCallBack, 0);
        interpreter.execute(ss.str().c_str());
        //printf("registered '%s'.\n", ss.str().c_str());                                 
    }

    // like read
    EXPORT void setNVCallBack(char *funcName, void* (*funcPtr)()) {            
        registeredNonVoidCallBackFunctions.emplace((char*)funcName, funcPtr); 
        std::stringstream ss;
        ss << "function " << funcName << "() { return ext_nv(\"" << funcName << "\"); };";
        interpreter.addNative("function ext_nv(self)", nonVoidCallBack, 0);
        interpreter.execute(ss.str().c_str());
        //printf("registered '%s'.\n", ss.str().c_str());                                
    }
    
    EXPORT void setNVVCallBack(char *funcName, void* (*funcPtr)(void *)) {            
        registeredNonVoidVoidCallBackFunctions.emplace((char *)funcName, funcPtr);
        std::stringstream ss;
        ss << "function " << funcName << "(a) { return ext_nvv(\"" << funcName << "\", a); };";
        interpreter.addNative("function ext_nvv(self, a)", nonVoidVoidCallBack, 0); 
        interpreter.execute(ss.str().c_str());
        //printf("registered '%s'.\n", ss.str().c_str());                                
    }
    
    EXPORT void setNVVVCallBack(char *funcName, void* (*funcPtr)(void *, void *)) {            
        registeredNonVoidVoidVoidCallBackFunctions.emplace((char *)funcName, funcPtr);
        std::stringstream ss;
        ss << "function " << funcName << "(a, b) { return ext_nvvv(\"" << funcName << "\", a, b); };";
        interpreter.addNative("function ext_nvvv(self, a, b)", nonVoidVoidVoidCallBack, 0); 
        interpreter.execute(ss.str().c_str());
        //printf("registered '%s'.\n", ss.str().c_str());                              
    }
    
    EXPORT void setNVVVVCallBack(char *funcName, void* (*funcPtr)(void *, void *, void *)) {            
        registeredNonVoidVoidVoidVoidCallBackFunctions.emplace((char *)funcName, funcPtr);
        std::stringstream ss;
        ss << "function " << funcName << "(a, b, c) { return ext_nvvvv(\"" << funcName << "\", a, b, c); };";
        interpreter.addNative("function ext_nvvvv(self, a, b, c)", nonVoidVoidVoidVoidCallBack, 0); 
        interpreter.execute(ss.str().c_str());
        //printf("registered '%s'.\n", ss.str().c_str());                              
    }
    
    EXPORT int Jail() {                               
        return 1;       
    }
    
    EXPORT void reset() {
        interpreter.reset();
    }
    
    EXPORT char* getLastError() {
        return lastError;
    }

    EXPORT int interpret(char *code) {                         
  
        registerChar(&interpreter); 
        registerInteger(&interpreter);
        registerDouble(&interpreter);
        registerString(&interpreter);
        registerObject(&interpreter);
        registerArray(&interpreter);
        registerFunctions(&interpreter); 
        registerMathFunctions(&interpreter);
            
        interpreter.addNative("function jail.eval(code)", scEval, &interpreter);
        interpreter.addNative("function jail.exec(code)", scExec, &interpreter);
        interpreter.addNative("function jail.debug(a)", debug, &interpreter);

        try {
            interpreter.execute(code);
        } catch (JAIL::Exception *e) { 
            lastError = (char *)e->text.c_str();
            return 0;
        } 
        return 1;                     
    }
    

    
    EXPORT void setChar(char *name, char value) {         
        JAIL::JLink *vl = interpreter.root->findChildOrCreateByPath(name);
        vl->var->setChar(value);                                         
    } 
    
    EXPORT void setInt(char *name, int value) {         
        JAIL::JLink *vl = interpreter.root->findChildOrCreateByPath(name);
        vl->var->setInt(value);                                         
    }
    
    EXPORT void setDouble(char *name, double value) {        
        JAIL::JLink *vl = interpreter.root->findChildOrCreateByPath(name);
        vl->var->setDouble(value);                                    
    }
    
    EXPORT void setString(char *name, char *value) {        
        JAIL::JLink *vl = interpreter.root->findChildOrCreateByPath(name);
        vl->var->setString(value);                                 
    }
    
    EXPORT char getChar(char *name) {            
        JAIL::JLink *vl = interpreter.root->findChildOrCreateByPath(name);           
        return vl->var->getChar();                  
    }
    
    EXPORT int getInt(char *name) {            
        JAIL::JLink *vl = interpreter.root->findChildOrCreateByPath(name);           
        return vl->var->getInt();                  
    }
    
    EXPORT double getDouble(char *name) {            
        JAIL::JLink *vl = interpreter.root->findChildOrCreateByPath(name);
        return vl->var->getDouble();                   
    }
    
    EXPORT char* getString(char *name) {            
        JAIL::JLink *vl = interpreter.root->findChildOrCreateByPath(name);
        int len = strlen(vl->var->getString().c_str());
        char *buffer = (char *)malloc(len);
        strncpy(buffer, vl->var->getString().c_str(), len);
        buffer[len] = '\0';
        return buffer; 
    }
    
    EXPORT char* test() {
        std::ostringstream symbols;
        interpreter.root->getJSON(symbols);
        return (char *)strdup(symbols.str().c_str());
    }
    
}    