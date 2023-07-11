#include <iostream> 
#include <string.h> 
#include <math.h>

extern "C" {

    __declspec(dllexport) char *__construct() {    
        char *tmp = (char *)malloc(256);        
        strcpy(tmp, "Msin_nvv;Mcos_nvv;Mtan_nvv;Masin_nvv;Macos_nvv;Matan_nvv;Masinh_nvv;Macosh_nvv;Matanh_nvv;Mlog_nvv;Mlog10_nvv;Mexp_nvv;Mpow_nvvv;");
        return tmp;                              
    }
    
    __declspec(dllexport) void* Msin(void *data) {             
        char *str = (char *)data;
        double d = sin( std::stod(str) );
        std::string newStr = std::to_string(d);
        int len = strlen(newStr.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)newStr.c_str());
        ret[len] = '\0';
        return (void *)ret;        
    }
    
    __declspec(dllexport) void* Mcos(void *data) {     
        char *str = (char *)data;
        double d = cos( std::stod(str) );
        std::string newStr = std::to_string(d);
        int len = strlen(newStr.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)newStr.c_str());
        ret[len] = '\0';
        return (void *)ret;             
    }
    
    __declspec(dllexport) void* Mtan(void *data) {     
        char *str = (char *)data;
        double d = tan( std::stod(str) );
        //printf("\n it would be %f\n", d);
        std::string newStr = std::to_string(d);
        int len = strlen(newStr.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)newStr.c_str());
        ret[len] = '\0';
        return (void *)ret;                 
    }
    
    __declspec(dllexport) void* Masin(void *data) {     
        char *str = (char *)data;
        double d = asin( std::stod(str) );
        std::string newStr = std::to_string(d);
        int len = strlen(newStr.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)newStr.c_str());
        ret[len] = '\0';
        return (void *)ret;               
    }
    
    __declspec(dllexport) void* Macos(void *data) {     
        char *str = (char *)data;
        double d = acos( std::stod(str) );
        std::string newStr = std::to_string(d);
        int len = strlen(newStr.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)newStr.c_str());
        ret[len] = '\0';
        return (void *)ret;                 
    }
    
    __declspec(dllexport) void* Matan(void *data) {     
        char *str = (char *)data;
        double d = atan( std::stod(str) );
        std::string newStr = std::to_string(d);
        int len = strlen(newStr.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)newStr.c_str());
        ret[len] = '\0';
        return (void *)ret;             
    }
    
    __declspec(dllexport) void* Masinh(void *data) {     
        char *str = (char *)data;
        double d = asinh( std::stod(str) );
        std::string newStr = std::to_string(d);
        int len = strlen(newStr.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)newStr.c_str());
        ret[len] = '\0';
        return (void *)ret;               
    }
    
    __declspec(dllexport) void* Macosh(void *data) {     
        char *str = (char *)data;
        double d = acosh( std::stod(str) );
        std::string newStr = std::to_string(d);
        int len = strlen(newStr.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)newStr.c_str());
        ret[len] = '\0';
        return (void *)ret;                
    }
    
    // nvv
    __declspec(dllexport) void* Matanh(void *data) {         
        char *str = (char *)data;
        double d = atanh( std::stod(str) );
        std::string newStr = std::to_string(d);
        int len = strlen(newStr.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)newStr.c_str());
        ret[len] = '\0';
        return (void *)ret;                
    }
    
    __declspec(dllexport) void* Mlog(void *data) {     
        char *str = (char *)data;
        double d = log( std::stod(str) );
        std::string newStr = std::to_string(d);
        int len = strlen(newStr.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)newStr.c_str());
        ret[len] = '\0';
        return (void *)ret;              
    }
    
    __declspec(dllexport) void* Mlog10(void *data) {     
        char *str = (char *)data;
        double d = log10( std::stod(str) );
        std::string newStr = std::to_string(d);
        int len = strlen(newStr.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)newStr.c_str());
        ret[len] = '\0';
        return (void *)ret;               
    }
    
    // returns Math.E to the power of data
    __declspec(dllexport) void* Mexp(void *data) {     
        char *str = (char *)data;
        double d = exp( std::stod(str) );
        std::string newStr = std::to_string(d);
        int len = strlen(newStr.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)newStr.c_str());
        ret[len] = '\0';
        return (void *)ret;             
    }
    
    __declspec(dllexport) void* Mpow(void *data1, void *data2) {     
        double a = *(double *)data1;
        double b = *(double *)data2;
        double d = std::pow(a, b);
        std::string r = std::to_string( d ); 
        //printf("%s", r.c_str());
        return (void *)r.c_str();               
    }

}