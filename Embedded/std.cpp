#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <conio.h> // getch
#include <iomanip> // base64

#include <regex>
#include <iterator>
#include <map>

using namespace std;

struct OpenFile {
    FILE * file;
};

std::map<int, OpenFile> openFiles;

static std::string base64_encode(const std::string &in) {
    std::string out;
    int val=0, valb=-6;
    for (char c : in) {
        val = (val<<8) + c;
        valb += 8;
        while (valb>=0) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val>>valb)&0x3F]);
            valb-=6;
        }
    }
    if (valb>-6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val<<8)>>(valb+8))&0x3F]);
    while (out.size()%4) out.push_back('=');
    return out;
}

static std::string base64_decode(const std::string &in) {
    std::string out;
    std::vector<int> T(256,-1);
    for (int i=0; i<64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i; 
    int val=0, valb=-8;
    for (char c : in) {
        if (T[c] == -1) break;
        val = (val<<6) + T[c];
        valb += 6;
        if (valb>=0) {
            out.push_back(char((val>>valb)&0xFF));
            valb-=8;
        }
    }
    return out;
}

extern "C" {

    __declspec(dllexport) char *__construct() {                  
        // there must be a semicolon at the end!
        char str[] = "read_nv;readline_nv;print_vv;openfile_nvvv;readfile_nvv;writefile_nvvv;closefile_vv;regexp_nvvv;regexpreplace_nvvvv;atob_nvv;btoa_nvv;output_nv;";
        return strdup(str);        
                             
    }

    __declspec(dllexport) void* read() {            
        int ch = getch();
        char *ret = (char *)malloc(128);
        std::string s = std::to_string(ch);
        strcpy(ret, s.c_str());
        ret[strlen(s.c_str())] = '\0'; 
        return (void *)ret;           
    } 

    __declspec(dllexport) void* readline() {            
        size_t len = 128, bufLen;
        char *tmp = (char *)malloc(len);
        if(tmp == NULL)
            exit(1);
        bufLen = getline(&tmp, &len, stdin);
        tmp[bufLen - 1] = '\0';   
        return (void *)tmp;           
    }

    __declspec(dllexport) void print(void *data) {            
        printf("%s", (char *)data);                    
    }

    __declspec(dllexport) void* openfile(void *data, void *data2) { 
        char *filename = (char*)data;
        char *mode = (char*)data2;
        FILE *f;
        std::string r;
        if(f = fopen(filename, mode)) {
            struct OpenFile of { f };
            int idx = openFiles.size();
            openFiles[idx] = of;
            r = std::to_string(idx);      
        }
        else
            r = std::to_string(-1);
        char *res = (char *)malloc(strlen(r.c_str()));
        strcpy(res, r.c_str());
        return (void *)res;
    }

    __declspec(dllexport) void* readfile(void *data) { 
        int fileindex = std::stoi((char*)data);
        OpenFile _f = openFiles[fileindex];
        FILE *f = _f.file;
        fseek(f, 0L, SEEK_END); 
        int len = ftell(f); 
        rewind(f);
        char c, buf[len];    
        int i = 0;
        while( fread(&c, sizeof(char), 1, f) > 0 ) 
            buf[i++] = c;
        buf[i++] = '\0';
        
        return (void *)strdup(buf);            
    }

    __declspec(dllexport) void* writefile(void *data, void *data2) { 
    
        int fileindex = std::stoi((char*)(void *)data);
        char *content = (char *)(void *)data2;   
        OpenFile _f = openFiles[fileindex];
        FILE *f = _f.file;
        // todo: error handling!
        fwrite(content, sizeof(char), strlen(content), f); 
        std::string r = std::to_string(1);
        return (void *)(char *)r.c_str();
                   
    }
    
    __declspec(dllexport) void* closefile(void *data) { 
        int fileindex = std::stoi((char*)data);
        FILE *f = openFiles[fileindex].file;
        fclose(f); 
        // todo: error handling
        openFiles.erase(fileindex);
        std::string r = std::to_string(1);
        return (void *)(char *)r.c_str();           
    }  
    
    __declspec(dllexport) void* regexp(void *data, void *data2) {    
        std::string var = (char *)data;
    	const std::regex r((char *)data2);  
    	std::smatch sm;
        std::stringstream result;
    	int i = 0;
        result << "[";
        while(regex_search(var, sm, r)) {    		
            if(i > 0)
                result << ",";
            result << "\"" << sm[0] << "\"";        
            i++;
            var = sm.suffix().str();            
        }        
        result << "]";
        int len = strlen(result.str().c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)result.str().c_str());
        ret[len] = '\0';         
        return (void *)(char *)ret;       
    } 
    
    __declspec(dllexport) void* regexpreplace(void *data, void *data2, void *data3) {            
        std::string mystr = (char *)data;  
        std::regex regexp((char *)data2);
        std::string repl = (char *)data3;         
        std::string result;
        regex_replace(back_inserter(result), mystr.begin(), mystr.end(), regexp, repl);
        int len = strlen(result.c_str());
        char *ret = (char *)malloc(len + 1);
        strcpy(ret, (char *)result.c_str());
        ret[len] = '\0';         
        return (void *)(char *)ret;    
    } 

    __declspec(dllexport) void* atob(void *data) {
        char *ret = (char *)data;
        std::string r = base64_encode(ret);
        int len = strlen(r.c_str());
        char *res = (char *)malloc(len + 1);
        strcpy(res, r.c_str());
        res[len] = '\0';
        return (void *)res;       
    }

    __declspec(dllexport) void* btoa(void *data) {
        char *ret = (char *)data;
        std::string r = base64_decode(ret); 
        int len = strlen(r.c_str());
        char *res = (char *)malloc(len + 1);
        strcpy(res, r.c_str());
        res[len] = '\0';
        return (void *)res;        
    }
    
    __declspec(dllexport) void* output() { 
        char *str = (char *)malloc(128);
        strcpy(str, "{ a:1.1, b:2, c:3.1 }");           
        return (void *)str;                    
    }

}

