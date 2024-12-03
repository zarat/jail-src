#ifdef _WIN32
#define _WIN32_WINNT 0x501
#include <cstdarg>
#include <sstream>
#include <cstdio> // Für vsnprintf
#endif

#include <unistd.h> //getcwd(), chdir()

//#include "Jail_Functions.h"
#include "Jail.h"

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

using namespace JAIL;

extern "C" {

struct OpenFile {
    FILE * file;
};

std::map<int, OpenFile> openFiles;

    //struct OpenFile { FILE *file; };
    
    //std::map<int, OpenFile> openFiles;
    
    __declspec(dllexport) std::string base64_encode(const std::string &in) {
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
    
    __declspec(dllexport)  std::string base64_decode(const std::string &in) {
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
    __declspec(dllexport) void scTypeOf(JAIL::JObject *c, void *) {
        JAIL::JObject *v = c->getParameter("v");
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

    __declspec(dllexport) void scConsoleRead(JAIL::JObject *c, void *) {
        int ch;
        #ifdef _WIN32
        ch = getch();
        #else
        ch = getchar();
        #endif
        c->getReturnVar()->setInt(ch);
    } 

    //todo
    __declspec(dllexport) void scConsoleReadLine(JAIL::JObject *c, void *) {
        std::string str; 
        std::getline(std::cin, str);
        c->getReturnVar()->setString(str.c_str());
    }


    __declspec(dllexport) void scConsoleWrite(JAIL::JObject *c, void *) {
        std::string str = c->getParameter("str")->getString(); 
        std::cout << str;
        c->getReturnVar()->setString(str.c_str());                   
    }
	
	__declspec(dllexport) void scConsolePrintf(JAIL::JObject *c, void *) {
		// Holen des Format-Strings aus den Parametern
		std::string format = c->getParameter("format")->getString();

		// Den Parameter `args` als Array holen
		JAIL::JObject *args = c->getParameter("args");
		int argCount = args->getArrayLength();

		// Konvertiere JAIL::JObject-Werte in ein C++-Stringformat
		std::ostringstream formattedString;
		size_t formatLen = format.length();
		size_t argIndex = 0;

		for (size_t i = 0; i < formatLen; ++i) {
		if (format[i] == '%' && (i + 1 < formatLen) && (format[i + 1] != ' ') && (format[i + 1] != '%') ) {
				char specifier = format[i + 1];
				if (argIndex < argCount) {
					JAIL::JObject *arg = args->getArrayIndex(argIndex);
					switch (specifier) {
						case 'd':
							formattedString << arg->getInt();
							break;
						case 'f':
							formattedString << arg->getDouble();
							break;
						case 's':
							formattedString << arg->getString();
							break;
						default:
							formattedString << specifier;
							break;
					}
					++argIndex;
					++i; // Überspringe den Format-Spezifizierer
				} else {
					// Wenn nicht genügend Argumente vorhanden sind, füge das Literal hinzu
					formattedString << '%';
				}
			} else {
				formattedString << format[i];
			}
		}

		// Die formatierte Nachricht ausgeben
		//std::cout << formattedString.str();
		//printf("%s", formattedString.str().c_str());

		// Rückgabewert setzen, falls benötigt
		c->getReturnVar()->setString(formattedString.str());
	}
    
    //todo
    __declspec(dllexport) void scConsoleWriteByte(JAIL::JObject *c, void *) {
        printf("%c", c->getParameter("ch")->getInt());   
    }
    
    // system() 
    __declspec(dllexport) void scProcessExec(JAIL::JObject *c, void *) {
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
     * get environment JAIL::JObject
     */
    __declspec(dllexport) void scGetEnv(JAIL::JObject *c, void *data) {
    
        char *p = (char *)c->getParameter("str")->getString().c_str();
        c->getReturnVar()->setString(getenv(p));
        
    }
    
    /**
     * setEnv() 
     * set environment JAIL::JObject
     */
    __declspec(dllexport) void scSetEnv(JAIL::JObject *c, void *data) {
        char *k = (char*)c->getParameter("k")->getString().c_str();
        char *v = (char*)c->getParameter("v")->getString().c_str();
        setenv(k, v, 1); // 1 for overwrite
        c->getReturnVar()->setInt(1);
    }
    
    /** 
     * get time as array (hour, min, sec, millisec, microsec)
     */
    __declspec(dllexport) void scSystemTime(JAIL::JObject *c, void *) {
        
        // high precision time struct
        struct timeval now;
        gettimeofday(&now, NULL);

        // get localtime from timestamp (sec since 1.1.1970)
        struct tm *ptm = localtime((const time_t *)&now.tv_sec);
        
        JAIL::JObject *v = new JAIL::JObject();
        v->setArray();
        
        v->setArrayIndex(0, new JAIL::JObject((int)ptm->tm_hour));
        v->setArrayIndex(1, new JAIL::JObject((int)ptm->tm_min));
        v->setArrayIndex(2, new JAIL::JObject((int)ptm->tm_sec));
        
        // milli and microseconds
        v->setArrayIndex(3, new JAIL::JObject((int)now.tv_usec / 1000));
        v->setArrayIndex(4, new JAIL::JObject((int)now.tv_usec));
        
        c->getReturnVar()->setArray(v->getArray());   
             
    }
    
    // %d.%m.%Y %H:%M:%S
    __declspec(dllexport) void scSystemTimeF(JAIL::JObject *c, void *) {
        
        char *fmtstr = (char*)c->getParameter("str")->getString().c_str();
        time_t currentTime = time(NULL);
        struct tm* timeInfo = localtime(&currentTime);
        
        char fmt[50];
        
        strftime(fmt, sizeof(fmt), fmtstr, timeInfo);

        c->getReturnVar()->setString(fmt);       
         
    }
    
    __declspec(dllexport) void scSystemDate(JAIL::JObject *c, void *) {
        
        time_t currentTime = time(NULL);
        struct tm* timeInfo = localtime(&currentTime);
        
        char day[50];
        char month[50];
        char year[50];
        
        strftime(day, sizeof(day), "%d", timeInfo);
        strftime(month, sizeof(month), "%m", timeInfo);
        strftime(year, sizeof(year), "%Y", timeInfo);

        JAIL::JObject *v = new JAIL::JObject();
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
    __declspec(dllexport) void scImportFile(JAIL::JObject *c, void *data) {
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
    void scJSONStringify(JAIL::JObject *c, void *) {
        std::ostringstream result;
        c->getParameter("obj")->getJSON(result);
        c->getReturnVar()->setString(result.str());
    }
    */
    
    /*
    void scRegex(JAIL::JObject *c, void *) {    
        std::string var = c->getParameter("data")->getString();
    	const std::regex r(c->getParameter("regex")->getString());  
    	std::smatch sm;
        JAIL::JObject *result = c->getReturnVar();  
        result->setArray();
    	int i = 0;
        while(regex_search(var, sm, r)) {
    		result->setArrayIndex(i, new JAIL::JObject(sm[0], VARIABLE_STRING));        
            i++;
            var = sm.suffix().str();
        }
    }
    */
    
    __declspec(dllexport) void scBase64Encode(JAIL::JObject *c, void *) {
        std::string var = c->getParameter("data")->getString();
        c->getReturnVar()->setString(base64_encode(var));    
    }
    
    __declspec(dllexport) void scBase64Decode(JAIL::JObject *c, void *) {
        std::string var = c->getParameter("data")->getString();
        c->getReturnVar()->setString(base64_decode(var));
    }
    
    __declspec(dllexport) void scMd5(JAIL::JObject *c, void *data) {
        std::string var = c->getParameter("data")->getString();
        std::stringstream md5string;
        md5string << std::hex << std::uppercase << std::setfill('0');
        for (const auto &byte: var)
            md5string << std::setw(2) << (int)byte;
        c->getReturnVar()->setString(md5string.str().c_str());
    }
    
    __declspec(dllexport) void scExec(JAIL::JObject *c, void *data) {
        JAIL::JInterpreter *tinyJS = (JAIL::JInterpreter *)data;
        std::string str = c->getParameter("jsCode")->getString();
        tinyJS->execute(str);
    }
    
    __declspec(dllexport) void scEval(JAIL::JObject *c, void *data) {
        JAIL::JInterpreter *tinyJS = (JAIL::JInterpreter *)data;
        std::string str = c->getParameter("jsCode")->getString();
        c->setReturnVar(tinyJS->evaluateComplex(str).var);
    }
	
	__declspec(dllexport) void scDebug(JAIL::JObject *c, void *data) {
		std::ostringstream oss;
		c->getParameter("a")->getJSON(oss);
		c->getReturnVar()->setString(oss.str());       
	}
    
    __declspec(dllexport) void scOpenFile(JAIL::JObject *c, void *data) {
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
    
    __declspec(dllexport) void scReadFile(JAIL::JObject *c, void *data) {

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
    __declspec(dllexport) void scReadFileC(JAIL::JObject *c, void *data) {

        int fileindex = std::stoi(c->getParameter("src")->getString());
        
        OpenFile _f = openFiles[fileindex];
        FILE *f = _f.file;        
        fseek(f, 0L, SEEK_END); 
        int len = ftell(f);         
        rewind(f);
        
        unsigned char ch;
        //unsigned char *buf = (unsigned char *)malloc(len);    
        unsigned int i = 0;
        
        JAIL::JObject *result = new JAIL::JObject();
        result->setArray();

        while( fread(&ch, sizeof(unsigned char), 1, f) > 0 ) { 

            result->setArrayIndex(i++, new JAIL::JObject(std::to_string(ch), VARIABLE_INTEGER));

        }

        c->getReturnVar()->setArray(result->getArray());
        
    } 
	
	__declspec(dllexport) void scReadFileRangeC(JAIL::JObject *c, void *data) {
		int fileindex = std::stoi(c->getParameter("src")->getString());
		int start = std::stoi(c->getParameter("start")->getString());
		int end = std::stoi(c->getParameter("end")->getString());
		
		OpenFile _f = openFiles[fileindex];
		FILE *f = _f.file;
		
		// Dateigröße ermitteln
		fseek(f, 0L, SEEK_END);
		int len = ftell(f);
		
		if (start < 0 || start >= len || end < 0 || end >= len || start > end) {
			throw std::invalid_argument("Ungültiger Bereich.");
		}
		
		// Startposition setzen
		fseek(f, start, SEEK_SET);
		
		unsigned char ch;
		unsigned int i = 0;

		JAIL::JObject *result = new JAIL::JObject();
		result->setArray();
		
		// Bereich lesen
		while (ftell(f) <= end && fread(&ch, sizeof(unsigned char), 1, f) > 0) {
			result->setArrayIndex(i++, new JAIL::JObject(std::to_string(ch), VARIABLE_INTEGER));
		}
		
		c->getReturnVar()->setArray(result->getArray());
	}

    
    __declspec(dllexport) void scWriteFile(JAIL::JObject *c, void *data) {
    
        int fileindex = std::stoi(c->getParameter("src")->getString());
        std::string content = c->getParameter("data")->getString();   
        OpenFile _f = openFiles[fileindex];
        FILE *f = _f.file;
        // todo: error handling!
        fwrite(content.c_str(), sizeof(char), strlen(content.c_str()), f); 
        c->getReturnVar()->setInt(1);
        
    }
    
    __declspec(dllexport) void scWriteFileC(JAIL::JObject *c, void *data) {

		int fileindex = c->getParameter("src")->getInt();

		OpenFile _f = openFiles[fileindex];
		FILE *f = _f.file;

		JAIL::JObject *array = c->getParameter("data");
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
	
	__declspec(dllexport) void scWriteFileRangeC(JAIL::JObject *c, void *data) {

		int fileindex = c->getParameter("src")->getInt();
		OpenFile _f = openFiles[fileindex];
		FILE *f = _f.file;
		
		int start = c->getParameter("pos")->getInt();
		
		JAIL::JObject *array = c->getParameter("data");
		int len = array->getArrayLength();
		char buffer[len] = { 0 };
		JLink *v = array->firstChild;
		int i = 0;
		while (v) {
			buffer[i] = (char)array->getArrayIndex(i)->getInt();
			i++;
			v = v->nextSibling;
		}
		fseek(f, start, SEEK_SET);
		fwrite(buffer, sizeof(char), len, f);
		c->getReturnVar()->setInt(1);
	
	}


    __declspec(dllexport) void scCloseFile(JAIL::JObject *c, void *data) {
    
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
    __declspec(dllexport) void scReadDir(JAIL::JObject *c, void *data) {
    
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
    __declspec(dllexport) void scCwd(JAIL::JObject *c, void* data) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            c->getReturnVar()->setString(cwd);
        } 
        else
            c->getReturnVar()->setUndefined();
    }
    
    // ok
    __declspec(dllexport) void scChDir(JAIL::JObject *c, void* data) {
        chdir(c->getParameter("str")->getString().c_str());
    }
	
    __declspec(dllexport) void scExit(JAIL::JObject *c, void* data) {
        exit(c->getParameter("code")->getInt());
    }
	
	__declspec(dllexport) void scTest(JAIL::JObject *c, void *data) {
        
		JAIL::JObject *arr = new JAIL::JObject();
		arr->setArray();	
		arr->setArrayIndex(0, new JAIL::JObject(42)); 
		arr->setArrayIndex(1, new JAIL::JObject("Text")); 
		JLink *u = arr->firstChild;
		int i = 0;
		while(u) {
			JObject* element = arr->getArrayIndex(i);
			//printf("%d %s\n", i, element->getString().c_str());
			i++;
			u = u->nextSibling;
		}
		//c->getReturnVar()->setArray(arr->getArray());

		JAIL::JObject *obj = new JAIL::JObject();
		obj->addChild("key0", arr);
		obj->addChild("key1", new JAIL::JObject(42));
		obj->addChild("key2", new JAIL::JObject("Text"));  
		JAIL::JLink *linkToRemove = obj->findChild("key1");
		if (linkToRemove) {
			obj->removeLink(linkToRemove);
		}
		//c->setReturnVar(obj);
		
		JObject *self = c->getParameter("this");
		JLink *v = self->firstChild;
		while (v) {  
			printf("%s -> %s\n", v->name.c_str(), v->var->getString().c_str());
			v = v->nextSibling;
		}
		
		
		JAIL::JInterpreter *interpreter = (JAIL::JInterpreter *)data;
		obj->addChild("b", arr);
		interpreter->root->addChild("t", obj);
		
		/*
		JObject *obj = c->getParameter("obj");  
		JObject *result = new JObject();
		result->setArray();  
		JLink *v = self->firstChild;
		int i = 0;
		while (v) {  
		if (!v->var->equals(obj)) {        
		result->setArrayIndex(i, v->var);        
		i++;
		}
		v = v->nextSibling;
		}    
		c->setReturnVar(result);
		*/
		
    }

    __declspec(dllexport) void registerLib(JAIL::JInterpreter *interpreter) {
    
        // input/output
        // todo
        interpreter->addNative("function Std.read()", scConsoleRead, 0);
        interpreter->addNative("function Std.readLine()", scConsoleReadLine, 0);
		//interpreter->addNative("function Std.scanf(format)", scConsoleScanf, 0);
        interpreter->addNative("function Std.print(str)", scConsoleWrite, 0);
        interpreter->addNative("function Std.printc(ch)", scConsoleWriteByte, 0);
		interpreter->addNative("function Std.format(format, args)", scConsolePrintf, 0);
		
		interpreter->addNative("function Std.eval(code)", scEval, interpreter);
		interpreter->addNative("function Std.exec(code)", scExec, interpreter);
		interpreter->addNative("function Std.debug(a)", scDebug, 0);

        interpreter->addNative("function Std.fopen(src, mode)", scOpenFile, 0);
        interpreter->addNative("function Std.fread(src)", scReadFile, 0);
        interpreter->addNative("function Std.freadc(src)", scReadFileC, interpreter);
		interpreter->addNative("function Std.freadrc(src, start, end)", scReadFileRangeC, interpreter);
        interpreter->addNative("function Std.fwrite(src, data)", scWriteFile, 0);
		interpreter->addNative("function Std.fwritec(src, data)", scWriteFileC, 0);
		interpreter->addNative("function Std.fwriterc(src, pos, data)", scWriteFileRangeC, 0);
        interpreter->addNative("function Std.fclose(src)", scCloseFile, 0);
        

        interpreter->addNative("function Std.system(cmd)", scProcessExec, 0);
        interpreter->addNative("function Std.getEnv(str)", scGetEnv, 0);
        interpreter->addNative("function Std.setEnv(k, v)", scSetEnv, 0);    
        interpreter->addNative("function Std.time()", scSystemTime, 0);
        interpreter->addNative("function Std.timef(str)", scSystemTimeF, 0);
        interpreter->addNative("function Std.date()", scSystemDate, 0);
        interpreter->addNative("function Std.typeOf(v)", scTypeOf, interpreter);
        interpreter->addNative("function Std.import(src)", scImportFile, interpreter);
    
        // @deprecated
        interpreter->addNative("function Std.parse(jsCode)", scEval, interpreter);        

        interpreter->addNative("function Std.atob(data)", scBase64Encode, 0);
        interpreter->addNative("function Std.btoa(data)", scBase64Decode, 0); 
        interpreter->addNative("function Std.md5(data)", scMd5, 0);
        
        interpreter->addNative("function Std.readDir(path)", scReadDir, 0);
        interpreter->addNative("function Std.cwd()", scCwd, 0);
        interpreter->addNative("function Std.chdir(str)", scChDir, 0);
	    
	   interpreter->addNative("function Std.exit(code)", scExit, 0);
	   interpreter->addNative("function Object.test(obj)", scTest, interpreter);
        
    }

};
