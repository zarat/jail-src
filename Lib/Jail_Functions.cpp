#ifdef _WIN32
#define _WIN32_WINNT 0x501
#endif

#include <unistd.h> //getcwd(), chdir()

#include "Jail_Functions.h"

#include <string>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>

#ifdef _WIN32
#include <conio.h> // getch()
#else
#include <termios.h> // getchar()
#endif

#include <iomanip> // base64
#include <map>
#include <regex>
#include <iterator>

#include <dirent.h> // readdir()

namespace JAIL {

struct OpenFile {
    FILE * file;
};

std::map<int, OpenFile> openFiles;

    //struct OpenFile { FILE *file; };
    
    //std::map<int, OpenFile> openFiles;
    
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
    
    // ----------------------------------------------- Actual Functions
    
    /**
     * typeOf()
     */
    void scTypeOf(JObject *c, void *) {
        JObject *v = c->getParameter("v");
        if(v->isObject())
            c->getReturnVar()->setString("object");
        else if(v->isFunction())
            c->getReturnVar()->setString("function");
        else if(v->isArray())
            c->getReturnVar()->setString("array");
        else if(v->isString())
            c->getReturnVar()->setString("string");
        else if(v->isDouble())
            c->getReturnVar()->setString("double");
        else if(v->isInt())
            c->getReturnVar()->setString("int");
        else if(v->isChar())
            c->getReturnVar()->setString("char");
        else if(v->isNull())
            c->getReturnVar()->setString("null");
        else if(v->isUndefined())
            c->getReturnVar()->setString("undefined");
        else if(v->isNative())
            c->getReturnVar()->setString("native");  
    }

    void scConsoleRead(JObject *c, void *) {   
        int ch;
        #ifdef _WIN32
        ch = getch();
        #else
        ch = getchar();
        #endif
        c->getReturnVar()->setInt(ch);
    } 

    //todo
    void scConsoleReadLine(JObject *c, void *) {   
        std::string str; 
        std::getline(std::cin, str);
        c->getReturnVar()->setString(str.c_str());
    }

    void scConsoleWrite(JObject *c, void *) {   
        std::string str = c->getParameter("str")->getString(); 
        std::cout << str;
        c->getReturnVar()->setString(str.c_str());                   
    }
    
    //todo
    void scConsoleWriteByte(JObject *c, void *) {      
        printf("%c", c->getParameter("ch")->getInt());   
    }
    
    // system() 
    void scProcessExec(JObject *c, void *) {
        char buffer[128];
        std::string result = "";
        std::string cmd = c->getParameter("cmd")->getString();
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) 
            throw new JAIL::Exception("Cannot read/write to system process.");
        try {
            while (fgets(buffer, sizeof buffer, pipe) != NULL) {
                result += buffer;
            }
        } catch (...) {
            pclose(pipe);
            throw new JAIL::Exception("Cannot read/write to system process.");
        }
        pclose(pipe);
        c->getReturnVar()->setString(result);
    }
    
    /** 
     * getEnv(str)
     * get environment JObject
     */
    void scGetEnv(JObject *c, void *data) {
    
        char *p = (char *)c->getParameter("str")->getString().c_str();
        c->getReturnVar()->setString(getenv(p));
        
    }
    
    /**
     * setEnv() 
     * set environment JObject
     */
    void scSetEnv(JObject *c, void *data) {
        char *k = (char*)c->getParameter("k")->getString().c_str();
        char *v = (char*)c->getParameter("v")->getString().c_str();
        setenv(k, v, 1); // 1 for overwrite
        c->getReturnVar()->setInt(1);
    }
    
    /** 
     * get time as array (hour, min, sec, millisec, microsec)
     */
    void scSystemTime(JObject *c, void *) {       
        
        // high precision time struct
        struct timeval now;
        gettimeofday(&now, NULL);

        // get localtime from timestamp (sec since 1.1.1970)
        struct tm *ptm = localtime((const time_t *)&now.tv_sec);
        
        JObject *v = new JObject();
        v->setArray();
        
        v->setArrayIndex(0, new JObject((int)ptm->tm_hour));
        v->setArrayIndex(1, new JObject((int)ptm->tm_min));
        v->setArrayIndex(2, new JObject((int)ptm->tm_sec));
        
        // milli and microseconds
        v->setArrayIndex(3, new JObject((int)now.tv_usec / 1000));
        v->setArrayIndex(4, new JObject((int)now.tv_usec));
        
        c->getReturnVar()->setArray(v->getArray());   
             
    }
    
    // %d.%m.%Y %H:%M:%S
    void scSystemTimeF(JObject *c, void *) {       
        
        char *fmtstr = (char*)c->getParameter("str")->getString().c_str();
        time_t currentTime = time(NULL);
        struct tm* timeInfo = localtime(&currentTime);
        
        char fmt[50];
        
        strftime(fmt, sizeof(fmt), fmtstr, timeInfo);

        c->getReturnVar()->setString(fmt);       
         
    }
    
    void scSystemDate(JObject *c, void *) {       
        
        time_t currentTime = time(NULL);
        struct tm* timeInfo = localtime(&currentTime);
        
        char day[50];
        char month[50];
        char year[50];
        
        strftime(day, sizeof(day), "%d", timeInfo);
        strftime(month, sizeof(month), "%m", timeInfo);
        strftime(year, sizeof(year), "%Y", timeInfo);

        JObject *v = new JObject();
        v->setArray();
        v->setArrayIndex(0, new JAIL::JObject( atoi(day) ) );
        v->setArrayIndex(1, new JAIL::JObject( atoi(month) ) );
        v->setArrayIndex(2, new JAIL::JObject( atoi(year) ) );
        c->setReturnVar(v);
        //c->getReturnVar()->setArray(v->getArray());       
         
    }
    
    /**
     * import()
     * import, read and eval file from disk
     */
    void scImportFile(JObject *c, void *data) {    
        JInterpreter *JInterpreter = reinterpret_cast<JAIL::JInterpreter *>(data);
        std::string filename = c->getParameter("src")->getString();
        std::ifstream readfile(filename);
        std::string line;
        if(readfile.is_open()) {
            std::stringstream ss;
            while(getline(readfile, line)) 
                ss << line.c_str() << "\n";
            std::string code = ss.str().c_str();
            JInterpreter->execute(code);       
            readfile.close();        
        }             
    }
    
    /*
    void scJSONStringify(JObject *c, void *) {
        std::ostringstream result;
        c->getParameter("obj")->getJSON(result);
        c->getReturnVar()->setString(result.str());
    }
    */
    
    /*
    void scRegex(JObject *c, void *) {    
        std::string var = c->getParameter("data")->getString();
    	const std::regex r(c->getParameter("regex")->getString());  
    	std::smatch sm;
        JObject *result = c->getReturnVar();  
        result->setArray();
    	int i = 0;
        while(regex_search(var, sm, r)) {
    		result->setArrayIndex(i, new JObject(sm[0], VARIABLE_STRING));        
            i++;
            var = sm.suffix().str();
        }
    }
    */
    
    void scBase64Encode(JObject *c, void *) {
        std::string var = c->getParameter("data")->getString();
        c->getReturnVar()->setString(base64_encode(var));    
    }
    
    void scBase64Decode(JObject *c, void *) {
        std::string var = c->getParameter("data")->getString();
        c->getReturnVar()->setString(base64_decode(var));
    }
    
    void scMd5(JObject *c, void *data) { 
        std::string var = c->getParameter("data")->getString();
        std::stringstream md5string;
        md5string << std::hex << std::uppercase << std::setfill('0');
        for (const auto &byte: var)
            md5string << std::setw(2) << (int)byte;
        c->getReturnVar()->setString(md5string.str().c_str());
    }
    
    void scExec(JObject *c, void *data) {
        JInterpreter *tinyJS = (JInterpreter *)data;
        std::string str = c->getParameter("jsCode")->getString();
        tinyJS->execute(str);
    }
    
    void scEval(JObject *c, void *data) {
        JInterpreter *tinyJS = (JInterpreter *)data;
        std::string str = c->getParameter("jsCode")->getString();
        c->setReturnVar(tinyJS->evaluateComplex(str).var);
    }
    
    void scOpenFile(JObject *c, void *data) {
        std::string filename = c->getParameter("src")->getString().c_str();
        std::string mode = c->getParameter("mode")->getString().c_str();
        
        FILE *f;
        std::string r;
        
        if(f = fopen(filename.c_str(), mode.c_str())) {
            struct OpenFile of { f };
            int idx = openFiles.size();
            openFiles[idx] = of;
            r = std::to_string(idx);      
        }
        else
            r = std::to_string(-1);
        c->getReturnVar()->setString(r.c_str());
    }
    
    void scReadFile(JObject *c, void *data) {

        int fileindex = std::stoi(c->getParameter("src")->getString());
        OpenFile _f = openFiles[fileindex];
        FILE *f = _f.file;
        fseek(f, 0L, SEEK_END); 
        int len = ftell(f); 
        rewind(f);
        char ch, buf[len];    
        int i = 0;
        while( fread(&ch, sizeof(char), 1, f) > 0 ) 
            buf[i++] = ch;
        buf[i++] = '\0';        
        c->getReturnVar()->setString(buf);
        
    }
    
    // returns an array of integers
    void scReadFileC(JObject *c, void *data) {

        int fileindex = std::stoi(c->getParameter("src")->getString());
        
        OpenFile _f = openFiles[fileindex];
        FILE *f = _f.file;        
        fseek(f, 0L, SEEK_END); 
        int len = ftell(f);         
        rewind(f);
        
        unsigned char ch;
        //unsigned char *buf = (unsigned char *)malloc(len);    
        unsigned int i = 0;
        
        JObject *result = new JObject();
        result->setArray();

        while( fread(&ch, sizeof(unsigned char), 1, f) > 0 ) { 

            result->setArrayIndex(i++, new JObject(std::to_string(ch), VARIABLE_INTEGER));

        }

        c->setReturnVar(result);
        
    } 
    
    void scWriteFile(JObject *c, void *data) {
    
        int fileindex = std::stoi(c->getParameter("src")->getString());
        std::string content = c->getParameter("data")->getString();   
        OpenFile _f = openFiles[fileindex];
        FILE *f = _f.file;
        // todo: error handling!
        fwrite(content.c_str(), sizeof(char), strlen(content.c_str()), f); 
        c->getReturnVar()->setInt(1);
        
    }
    
	void scWriteFileC(JObject *c, void *data) {

		int fileindex = c->getParameter("src")->getInt();

		OpenFile _f = openFiles[fileindex];
		FILE *f = _f.file;

		JObject *array = c->getParameter("data");
		int len = array->getArrayLength();
		unsigned char buffer[len] = { 0 };

		JLink *v = array->firstChild;
		int i = 0;
		while (v) {
			buffer[i] = (unsigned char)array->getArrayIndex(i)->getInt(); //(unsigned char)v->var->getInt();
			i++;
			v = v->nextSibling;
		}

		fwrite(buffer, sizeof(unsigned char), len, f);

		c->getReturnVar()->setInt(1);

	}

    void scCloseFile(JObject *c, void *data) {
    
        int fileindex = std::stoi(c->getParameter("src")->getString());
        FILE *f = openFiles[fileindex].file;
        fclose(f); 
        // todo: error handling
        openFiles.erase(fileindex);
        std::string r = std::to_string(1);
        c->getReturnVar()->setString(r.c_str());
        
    }
    
    // ok
    // @return array
    void scReadDir(JAIL::JObject *c, void *data) {
    
        std::string path = c->getParameter("path")->getString();
        JAIL::JObject *result = new JAIL::JObject();
        result->setArray();        
        int i = 0;
        DIR *d;
        d = opendir(path.c_str());
        struct dirent *dir;       
        if(d) 
            while ((dir = readdir(d)) != NULL) 
                result->setArrayIndex(i++, new JAIL::JObject(dir->d_name));  
        c->setReturnVar(result);
        
    }
    
    // ok
    void scCwd(JAIL::JObject *c, void* data) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            c->getReturnVar()->setString(cwd);
        } 
        else
            c->getReturnVar()->setUndefined();
    }
    
    // ok
    void scChDir(JAIL::JObject *c, void* data) {
        chdir(c->getParameter("str")->getString().c_str());
    }
	
    void scExit(JAIL::JObject *c, void* data) {
        exit(c->getParameter("code")->getInt());
    }

    void registerFunctions(JAIL::JInterpreter *interpreter) {
    
        // input/output
        // todo
        interpreter->addNative("function jail.read()", scConsoleRead, 0);
        interpreter->addNative("function jail.readLine()", scConsoleReadLine, 0);
        //interpreter->addNative("function print(str)", scConsoleWrite, 0);
        //interpreter->addNative("function printc(ch)", scConsoleWriteByte, 0);

        interpreter->addNative("function jail.fopen(src, mode)", scOpenFile, 0);
        interpreter->addNative("function jail.fread(src)", scReadFile, 0);
        interpreter->addNative("function jail.freadc(src)", scReadFileC, interpreter);
        interpreter->addNative("function jail.fwrite(src, data)", scWriteFile, 0);
		interpreter->addNative("function jail.fwritec(src, data)", scWriteFileC, 0);
        interpreter->addNative("function jail.fclose(src)", scCloseFile, 0);
        

        interpreter->addNative("function jail.system(cmd)", scProcessExec, 0);
        interpreter->addNative("function jail.getEnv(str)", scGetEnv, 0);
        interpreter->addNative("function jail.setEnv(k, v)", scSetEnv, 0);    
        interpreter->addNative("function jail.time()", scSystemTime, 0);
        interpreter->addNative("function jail.timef(str)", scSystemTimeF, 0);
        interpreter->addNative("function jail.date()", scSystemDate, 0);
        interpreter->addNative("function jail.typeOf(v)", scTypeOf, interpreter);
        interpreter->addNative("function jail.import(src)", scImportFile, interpreter);
    
        // @deprecated
        interpreter->addNative("function jail.parse(jsCode)", scEval, interpreter);        

        interpreter->addNative("function jail.atob(data)", scBase64Encode, 0);
        interpreter->addNative("function jail.btoa(data)", scBase64Decode, 0); 
        interpreter->addNative("function jail.md5(data)", scMd5, 0);
        
        interpreter->addNative("function jail.readDir(path)", scReadDir, 0);
        interpreter->addNative("function jail.cwd()", scCwd, 0);
        interpreter->addNative("function jail.chdir(str)", scChDir, 0);
	    
	   interpreter->addNative("function jail.exit(code)", scExit, 0);
        
    }

};
