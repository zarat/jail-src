#include "Jail.h"

#include <string>
#include <algorithm>
#include <cctype>

namespace JAIL {

    static inline void ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
    }
    static inline void rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), s.end());
    }
    static inline void trim(std::string &s) {
        ltrim(s);
        rtrim(s);
    }
    
    // String.trim()
    void scStringTrim(JObject *c, void *) {
        if( c->getParameter("this")->isObject() ) {                        
            // search value
            std::string str;        
            JLink *vl = c->getParameter("this")->firstChild;
            vl = c->getParameter("this")->firstChild;
            while(vl) {        
                if(vl->name.compare("value") == 0) {
                    str = vl->var->getString();
                    trim(str);
                    vl->var->setString(str);
                    c->getReturnVar()->setString(str);
                    break;
                }
                vl = vl->nextSibling;        
            }                
        }        
        else {
            std::string str = c->getParameter("this")->getString();
            trim(str);
            c->getParameter("this")->setString(str);
            c->getReturnVar()->setString(str);        
        }    
    }
    
    // String.ltrim()
    void scStringLTrim(JObject *c, void *) {
        if( c->getParameter("this")->isObject() ) {                        
            // search value
            std::string str;        
            JLink *vl = c->getParameter("this")->firstChild;
            vl = c->getParameter("this")->firstChild;
            while(vl) {        
                if(vl->name.compare("value") == 0) {
                    str = vl->var->getString();
                    ltrim(str);
                    vl->var->setString(str);
                    c->getReturnVar()->setString(str);
                    break;
                }
                vl = vl->nextSibling;        
            }                
        }        
        else {        
            std::string str = c->getParameter("this")->getString();
            ltrim(str);
            c->getParameter("this")->setString(str);
            c->getReturnVar()->setString(str);        
        }
    }
    
    // String.rtrim()
    void scStringRTrim(JObject *c, void *) {
        if( c->getParameter("this")->isObject() ) {            
            // search value
            std::string str;        
            JLink *vl = c->getParameter("this")->firstChild;
            vl = c->getParameter("this")->firstChild;
            while(vl) {        
                if(vl->name.compare("value") == 0) {
                    str = vl->var->getString();
                    rtrim(str);
                    vl->var->setString(str);
                    c->getReturnVar()->setString(str);
                    break;
                }
                vl = vl->nextSibling;        
            }                
        }        
        else {        
            std::string str = c->getParameter("this")->getString();
            rtrim(str);
            c->getParameter("this")->setString(str);
            c->getReturnVar()->setString(str);        
        }
    }
    
    void scStringSplit1(JObject *c, void *) {
        if( c->getParameter("this")->isObject() ) {            
            // search value
            std::string str;        
            JLink *vl = c->getParameter("this")->firstChild;
            vl = c->getParameter("this")->firstChild;
            while(vl) {        
                if(vl->name.compare("value") == 0) {
                    str = vl->var->getString();
                    break;
                }
                vl = vl->nextSibling;        
            }
            std::string sep = c->getParameter( "separator")->getString();        
            JObject *result = c->getReturnVar();
            result->setArray();       
            int length = 0;
            if ( str.size() > 1 && sep.size() > 0 ) {
                size_t pos = str.find(sep);        
                while (pos != std::string::npos) {
                  result->setArrayIndex(length++, new JObject(str.substr(0,pos)));
                  str = str.substr(pos+1);
                  pos = str.find(sep);
                }    
                result->setArrayIndex(length++, new JObject(str));
            }
            else
                result->setArrayIndex(0, new JObject(str));                   
        } else {
            std::string str = c->getParameter("this")->getString();
            std::string sep = c->getParameter("separator")->getString();        
            JObject *result = c->getReturnVar();
            result->setArray();        
            int length = 0;
            if ( str.size() > 1 && sep.size() > 0 ) {
                size_t pos = str.find(sep);        
                while (pos != std::string::npos) {
                  result->setArrayIndex(length++, new JObject(str.substr(0,pos)));
                  str = str.substr(pos+1);
                  pos = str.find(sep);
                }    
                result->setArrayIndex(length++, new JObject(str)); 
            }
            else
                result->setArrayIndex(0, new JObject(str)); 
                
            c->setReturnVar(result);              
        }     
    }
    
    void scStringSplit(JObject *c, void *data) {
        std::string str = c->getParameter("this")->getString();
        std::string sep = c->getParameter("sep")->getString();
        JObject *result = c->getReturnVar();
        result->setArray();
        int length = 0;    
        size_t pos = str.find(sep);
        while (pos != std::string::npos) {
          result->setArrayIndex(length++, new JObject(str.substr(0,pos)));
          str = str.substr(pos+1);
          pos = str.find(sep);
        }    
        if (str.size() > 0) 
            result->setArrayIndex(length++, new JObject(str));          
        c->setReturnVar(result);        
    }
    
    // String.indexOf() 
    // returns an array of all occurancies
    // todo object
    void scStringIndexOf(JObject *c, void *) {        
        std::string str = c->getParameter("this")->getString();
        std::string sep = c->getParameter("search")->getString();
        JObject *result = c->getReturnVar();
        result->setArray();
        int length = 0;
        size_t pos = str.find(sep);
        bool found = false;
        while (pos != std::string::npos) {
            if(!found)
                found = true;
            result->setArrayIndex(length++, new JObject((int)pos));
            pos = str.find(sep, pos + sep.size());
        }
        if(!found)
            c->getReturnVar()->setInt(-1);            
    }
    
    // todo object
    void scStringSubstring(JObject *c, void *) {
        std::string str = c->getParameter("this")->getString();
        int lo = c->getParameter("start")->getInt();
        int hi = c->getParameter("end")->getInt();    
        int l = hi - lo;
        if (l > 0 && lo >= 0 && lo + l <= (int)str.length() )
            c->getReturnVar()->setString(str.substr(lo, l));
        else
            c->getReturnVar()->setString("");
    }
    
    // modifies original string and returns a modified copy too
    void scStringReplace(JObject *c, void *) {        
        
        std::string str = c->getParameter("this")->getString();    
        std::string from = c->getParameter("from")->getString();
        std::string to = c->getParameter("to")->getString();
        size_t start_pos = str.find(from);
        if(start_pos != std::string::npos) {
            str.replace(start_pos, from.length(), to);
        }    
        c->getReturnVar()->setString(str); 
                
    }
    
    // modifies original string and returns a modified copy too
    void scStringReplaceAll(JObject *c, void *) {        
        
        std::string str = c->getParameter("this")->getString();    
        std::string from = c->getParameter("from")->getString();
        std::string to = c->getParameter("to")->getString();
        size_t start_pos = 0;
        while(1) {
            start_pos = str.find(from);
            if(start_pos != std::string::npos)
                str.replace(start_pos, from.length(), to);
            else
                break;
        }    
        c->getReturnVar()->setString(str);
             
    }
    
    void scStringCharAt(JObject *c, void *) {
        
        std::string str = c->getParameter("this")->getString();
        int p = c->getParameter("pos")->getInt();
        if (p>=0 && p<(int)str.length())
          c->getReturnVar()->setString(str.substr(p, 1));
        else
          c->getReturnVar()->setUndefined();
          
    }

    void scStringCharCodeAt(JObject *c, void *) {
    
        std::string str = c->getParameter("this")->getString();
        int p = c->getParameter("pos")->getInt();
        if (p>=0 && p<(int)str.length())
          c->getReturnVar()->setInt(str.at(p));
        else
          c->getReturnVar()->setUndefined();
          
    }

    void scStringFromCharCode(JObject *c, void *) {
        char str[2];
        str[0] = c->getParameter("char")->getInt();
        str[1] = 0;
        c->getReturnVar()->setString(str);
    }
    
	void scStringToLower(JObject *c, void *) {
		std::string str = c->getParameter("this")->getString();		
		std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
		//c->getParameter("this")->setString(str.c_str());
		c->getReturnVar()->setString(str.c_str());
	}

	void scStringToUpper(JObject *c, void *) {
		std::string str = c->getParameter("this")->getString();
		std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::toupper(c); });
		//c->getParameter("this")->setString(str.c_str());
		c->getReturnVar()->setString(str.c_str());
	}

    void registerString(JInterpreter *interpreter) {
    
        interpreter->addNative("function String.trim()", scStringTrim, 0);
        interpreter->addNative("function String.ltrim()", scStringLTrim, 0);
        interpreter->addNative("function String.rtrim()", scStringRTrim, 0);                       
        interpreter->addNative("function String.indexOf(search)", scStringIndexOf, 0); 
        interpreter->addNative("function String.substring(start, end)", scStringSubstring, 0);               
        interpreter->addNative("function String.charAt(pos)", scStringCharAt, 0);
        interpreter->addNative("function String.charCodeAt(pos)", scStringCharCodeAt, 0); 
        //interpreter->addNative("function String.fromCharCode(char)", scStringFromCharCode, 0);
        interpreter->addNative("function String.split(sep)", scStringSplit, interpreter);       
        interpreter->addNative("function String.replace(from, to)", scStringReplace, 0);
        interpreter->addNative("function String.replaceAll(from, to)", scStringReplaceAll, 0);
		interpreter->addNative("function String.toLower()", scStringToLower, 0);
		interpreter->addNative("function String.toUpper()", scStringToUpper, 0);

    }

}
