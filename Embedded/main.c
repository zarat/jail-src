#include <stdio.h> 
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <shlwapi.h> // use -lshlwapi
#include <conio.h>
                   
typedef int (__cdecl *JAILPROC)(char *); // Jail, interpret
typedef char* (__cdecl *GETLASTERRORPROC)();
typedef char* (__cdecl *EXTCONSTRUCTOR)();

HANDLE dll; 
JAILPROC func; // Jail.Jail(), jail.interpret() 
GETLASTERRORPROC errorfunc; // Jail.getLastError() 

void load(char *type, char *name, char *func) {
    HANDLE ext_dll = NULL;
    ext_dll = LoadLibrary(name);
    if(NULL != ext_dll) {       
        ( GetProcAddress(dll, type) )(func, GetProcAddress(ext_dll, func) );
    }
}

// todo
void loadExtensionDlls() {
    
    char selfdir[128] = {0};     
    getcwd(selfdir, sizeof(selfdir));
    strcat(selfdir, "\\libs\\");
    DIR *d;
    struct dirent *dir;    
    d = opendir(selfdir);
        
    if (d) {
    
        while ((dir = readdir(d)) != NULL) {

            char cmp[128] = {0};           
            sprintf(cmp, "%s%s", selfdir, dir->d_name);
            
            HANDLE test_dll = LoadLibrary( cmp );
            
            if(NULL != test_dll) {
            
                EXTCONSTRUCTOR pr = (EXTCONSTRUCTOR) GetProcAddress(test_dll, "__construct");                                
                
                // if there is no constructor, skip it
                if(NULL == pr)  
                    continue; 

                char *exportedFunctions = (pr)(); 
                   
                // strtok in a loop
                // https://stackoverflow.com/questions/4693884/nested-strtok-function-problem-in-c
                char *string = exportedFunctions; 
                char *token  = strchr(string, ';');
                while (token != NULL) {
                    *token++ = '\0';
                    char *extensionName, *extensionSignature;                    
                    char *token2 = strtok(string, "_");
                    int i = 0;
                    while (token2 != NULL) {
                        if(i == 0)
                            extensionName = token2;
                        else if(i == 1)
                            extensionSignature = token2;
                        token2 = strtok(NULL, "_");
                        i++;
                    }                 
                    if(strcmp(extensionSignature, "vv") == 0) 
                        load("setVVCallBack", cmp, extensionName);                    
                    if(strcmp(extensionSignature, "nv") == 0) 
                        load("setNVCallBack", cmp, extensionName);                    
                    if(strcmp(extensionSignature, "nvv") == 0) 
                        load("setNVVCallBack", cmp, extensionName);                    
                    if(strcmp(extensionSignature, "nvvv") == 0) 
                        load("setNVVVCallBack", cmp, extensionName);                    
                    if(strcmp(extensionSignature, "nvvvv") == 0) 
                        load("setNVVVVCallBack", cmp, extensionName);                    
                    
                    string = token;
                    token = strchr(string, ';');                    
                }
                
            }
 
        }
        
    } 
    
}

int main( int argc, char **argv ) { 

    if(argc < 2)
    {
        printf("Usage: jail <file>");
        return 1;
    }

    dll = LoadLibrary("jail.dll");  
    func = (JAILPROC) GetProcAddress(dll, "Jail");     
    (func);
    
    func = (JAILPROC) GetProcAddress(dll, "interpret");
    
    loadExtensionDlls();  
    
    // read text file with code    
    FILE* f;
    f = fopen(argv[1], "r");
    fseek(f, 0L, SEEK_END); 
    int len = ftell(f); 
    rewind(f);
    char c, buffer[len];    
    int i = 0;
    while( fread(&c, sizeof(char), 1, f) > 0 ) 
        buffer[i++] = c;
    fclose(f);
    buffer[i] = '\0';

    // execute the interpret function           
    int result = (func)(buffer); 
 
    // show errors if any
    if(!result) {
    
        errorfunc = (GETLASTERRORPROC) GetProcAddress(dll, "getLastError");        
        char* buffer = (errorfunc)();        
        printf(buffer);
        
    }
        
    FreeLibrary(dll); 
    
    return 0;

}