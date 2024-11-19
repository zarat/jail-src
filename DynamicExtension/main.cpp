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

#ifdef _WIN32
#include <windows.h>
typedef HMODULE lib_t; // Verwende den Windows-Typ HMODULE
#else
#include <dlfcn.h>
typedef void* lib_t;
#endif

lib_t MyLoadLib(const char* lib) {
    #ifdef _WIN32
    return LoadLibraryA(lib);
    #else 
    return dlopen(lib, RTLD_LAZY);
    #endif 
}

void MyUnloadLib(lib_t lib) {
    #ifdef _WIN32
    FreeLibrary(lib);
    #else 
    dlclose(lib);
    #endif 
}

void* MyLoadProc(lib_t lib, const char* proc) {
#ifdef _WIN32
    FARPROC procAddr = GetProcAddress(lib, proc);
    return (void*)procAddr;
#else
    return dlsym(lib, proc);
#endif
}

class MyLibrary {
public:
    MyLibrary(const char* lib) {
        libHandle = MyLoadLib(lib);
    }

    ~MyLibrary() {
        MyUnloadLib(libHandle);
    }

    void* GetFunction(const char* proc) {
        return MyLoadProc(libHandle, proc);
    }

private:
    lib_t libHandle;
};

typedef void (*DebugFunction)(JAIL::JInterpreter*);

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

std::vector<std::string> getDllFiles(const std::string& folderPath) {
    std::vector<std::string> dllFiles;
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile((folderPath + "\\*.dll").c_str(), &findFileData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            dllFiles.emplace_back(folderPath + "\\" + findFileData.cFileName);
        } while (FindNextFile(hFind, &findFileData));
        FindClose(hFind);
    }
    return dllFiles;
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
    
    

    /*
	// Load Plugins
    MyLibrary myLib1("./plugins/Test1.dll");
    DebugFunction debugFn1 = (DebugFunction)myLib1.GetFunction("registerLib");
    if (debugFn1) {
        debugFn1(&interpreter);
    } 

    MyLibrary myLib2("./plugins/Test2.dll");
	DebugFunction debugFn2 = (DebugFunction)myLib2.GetFunction("registerLib");
    if (debugFn2) {
        debugFn2(&interpreter);
    } 
	*/
	
	/*
	// Load plugins from array
    const char* dlls[] = { "./plugins/Test1.dll", "./plugins/Test2.dll" };
    size_t dllCount = sizeof(dlls) / sizeof(dlls[0]);
	vector<MyLibrary*> loadedLibraries;

    for (size_t i = 0; i < dllCount; i++) {
        
		MyLibrary* myLib = new MyLibrary(dlls[i]);
        
		if (!myLib->IsValid()) {
            //printf("Failed to load library: %s\n", dlls[i]);
            delete myLib;
            continue;
        }
		DebugFunction debugFn = (DebugFunction)myLib->GetFunction("registerLib");
        if (debugFn) {
            debugFn(&interpreter);
            loadedLibraries.push_back(myLib);
        } else {
            //printf("Failed to find 'registerLib' in %s\n", dlls[i]);
            delete myLib;
        }
    }
	*/
	
	
	// Load plugins from directory
	std::string pluginFolder = "./";
	std::vector<std::string> dllFiles = getDllFiles(pluginFolder);
    std::vector<MyLibrary*> loadedLibraries;
    for (const std::string& dllPath : dllFiles) {
        MyLibrary* myLib = new MyLibrary(dllPath.c_str());    
        if (!myLib->IsValid()) {
            delete myLib;
            continue;
        }
        DebugFunction debugFn = (DebugFunction)myLib->GetFunction("registerLib");
        if (debugFn) {
            debugFn(&interpreter);
            loadedLibraries.push_back(myLib);
        } else {
            delete myLib;
        }
    }



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

    for (MyLibrary* lib : loadedLibraries) {
        delete lib;
    }
    
    // get the global variable from script context
    //printf("retVal: %d", interpreter.root->getParameter("returnvalue")->getInt());
    
    return 1;  
                       
}   
