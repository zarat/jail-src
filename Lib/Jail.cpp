#include "Jail.h"
#include <assert.h>

#ifndef ASSERT
  #define ASSERT(X) assert(X)
#endif
/* Frees the given link IF it isn't owned by anything else */
#define CLEAN(x) { JLink *__v = x; if (__v && !__v->owned) { delete __v; } }
/* Create a LINK to point to VAR and free the old link.
BUT this is more clever - it tries to keep the old link if it's not owned to save allocations */
#define CREATE_LINK(LINK, VAR) { if (!LINK || LINK->owned) LINK = new JLink(VAR); else LINK->replaceWith(VAR); }

#include <string>
#include <string.h>
#include <sstream>
#include <cstdlib>
#include <stdio.h>

#if defined(_WIN32) && !defined(_WIN32_WCE)
#ifdef _DEBUG
   #ifndef DBG_NEW
      #define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
      #define new DBG_NEW
   #endif
#endif
#endif

#ifdef __GNUC__
  #define vsprintf_s vsnprintf
  #define sprintf_s snprintf
  #define _strdup strdup
#endif

#ifdef _WIN32_WCE
  #include <strsafe.h>
  #define sprintf_s StringCbPrintfA
#endif

// ----------------------------------------------------------------------------------- Namespace

namespace JAIL {

// ----------------------------------------------------------------------------------- Memory Debug

#ifdef DEBUG
  #define DEBUG_MEMORY 1
#endif

#if DEBUG_MEMORY

std::vector<JObject*> allocatedVars;
std::vector<JLink*> allocatedLinks;

void mark_allocated(JObject *v) {
    allocatedVars.push_back(v);
}

void mark_deallocated(JObject *v) {
    for (size_t i=0;i<allocatedVars.size();i++) {
      if (allocatedVars[i] == v) {
        allocatedVars.erase(allocatedVars.begin()+i);
        break;
      }
    }
}

void mark_allocated(JLink *v) {
    allocatedLinks.push_back(v);
}

void mark_deallocated(JLink *v) {
    for (size_t i=0;i<allocatedLinks.size();i++) {
      if (allocatedLinks[i] == v) {
        allocatedLinks.erase(allocatedLinks.begin()+i);
        break;
      }
    }
}

void show_allocated() {
    for (size_t i=0;i<allocatedVars.size();i++) {
      printf("ALLOCATED, %d refs\n", allocatedVars[i]->getRefs());
      allocatedVars[i]->trace("  ");
    }
    for (size_t i=0;i<allocatedLinks.size();i++) {
      printf("ALLOCATED LINK %s, allocated[%d] to \n", allocatedLinks[i]->name.c_str(), allocatedLinks[i]->var->getRefs());
      allocatedLinks[i]->var->trace("  ");
    }
    allocatedVars.clear();
    allocatedLinks.clear();
}
#endif 

// ----------------------------------------------------------------------------------- Utils

bool isWhitespace(char ch) {
    return (ch==' ') || (ch=='\t') || (ch=='\n') || (ch=='\r');
}

bool isNumeric(char ch) {
    return (ch >= '0') && (ch <= '9');
}
bool isNumber(const std::string &str) {
    for(size_t i=0;i<str.size();i++)
        if(!isNumeric(str[i])) 
            return false;
    return true;
}
bool isHexadecimal(char ch) {
    return ((ch >= '0') && (ch <= '9')) || ((ch >= 'a') && (ch <= 'f')) || ((ch >= 'A') && (ch <= 'F'));
}
bool isBinary(char ch) {
    return (ch = '0' || ch == '1');
}
bool isAlpha(char ch) {
    return ((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ch=='_';
}

void replace(std::string &str, char textFrom, const char *textTo) {
    int sLen = strlen(textTo);
    size_t p = str.find(textFrom);
    while (p != std::string::npos) {
        str = str.substr(0, p) + textTo + str.substr(p+1);
        p = str.find(textFrom, p+sLen);
    }
}

/// convert the given string into a quoted string suitable for javascript
std::string getJSString(const std::string &str) {
    std::string nStr = str;
    for (size_t i=0;i<nStr.size();i++) {
      const char *replaceWith = "";
      bool replace = true;

      switch (nStr[i]) {
        case '\\': replaceWith = "\\\\"; break;
        case '\n': replaceWith = "\\n"; break;
        case '\t': replaceWith = "\\t"; break;
        case '\r': replaceWith = "\\r"; break;
        case '\a': replaceWith = "\\a"; break;
        case '"': replaceWith = "\\\""; break;
        default: {
          int nCh = ((int)nStr[i]) &0xFF;
          if (nCh < 32 || nCh > 127) {
            char buffer[5];
            sprintf_s(buffer, 5, "\\x%02X", nCh);
            replaceWith = buffer;
          } else replace = false;
        }
      }

      if (replace) {
        nStr = nStr.substr(0, i) + replaceWith + nStr.substr(i+1);
        i += strlen(replaceWith)-1;
      }
    }
    return "\"" + nStr + "\"";
}

/** Is the string alphanumeric */
bool isAlphaNum(const std::string &str) {
    if(str.size() == 0) 
        return true;
    if(!isAlpha(str[0])) 
        return false;    
    for(size_t i=0;i<str.size();i++)
        if(!(isAlpha(str[i]) || isNumeric(str[i])))
            return false;
    return true;
}

// ----------------------------------------------------------------------------------- Exception

Exception::Exception(const std::string &exceptionText) {
    text = exceptionText;
}

// ----------------------------------------------------------------------------------- JLexer

JLexer::JLexer(const std::string &input) {
    data = _strdup(input.c_str());
    dataOwned = true;
    dataStart = 0;
    dataEnd = strlen(data);
    reset();
}

JLexer::JLexer(JLexer *owner, int startChar, int endChar) {
    data = owner->data;
    dataOwned = false;
    dataStart = startChar;
    dataEnd = endChar;
    reset();
}

JLexer::~JLexer(void)
{
    if (dataOwned)
        free((void*)data);
}

void JLexer::reset() {
    dataPos = dataStart;
    tokenStart = 0;
    tokenEnd = 0;
    tokenLastEnd = 0;
    tk = 0;
    tkStr = "";
    getNextCh();
    getNextCh();
    getNextToken();
}

void JLexer::match(int expected_tk) {
    
    if (tk != expected_tk) {
    
        std::ostringstream errorString;
        errorString << "[ERROR] Found '" << getTokenStr(tk) << "', expected '" << getTokenStr(expected_tk) << "' at " << getPosition(tokenStart);
        throw new Exception(errorString.str());
        
    }
    
    getNextToken();
    
}

std::string JLexer::getTokenStr(int token) {
    
    // printable ascii chars
    if (token > 32 && token < 128) {
        char buf[2] = " ";
        buf[0] = (char)token;
        return buf;
    }
    
    switch (token) {
        
        case LEXER_EOF : return "EOF";
        
        // datatypes
        case LEXER_IDENTIFIER : return "IDENTIFIER";
        case LEXER_INTEGER : return "INTEGER";
        case LEXER_DOUBLE : return "DOUBLE";
        case LEXER_STRING : return "STRING";
        case LEXER_CHARACTER : return "CHARACTER";
        
        // operators
        case LEXER_EQUAL : return "==";
        case LEXER_TYPEEQUAL : return "===";
        case LEXER_NEQUAL : return "!=";
        case LEXER_NTYPEEQUAL : return "!==";
        case LEXER_LEQUAL : return "<=";
        case LEXER_LSHIFT : return "<<";
        case LEXER_LSHIFTEQUAL : return "<<=";
        case LEXER_GEQUAL : return ">=";
        case LEXER_RSHIFT : return ">>";
        case LEXER_RSHIFTUNSIGNED : return ">>";
        case LEXER_RSHIFTEQUAL : return ">>=";
        case LEXER_PLUSEQUAL : return "+=";
        case LEXER_MINUSEQUAL : return "-=";
        case LEXER_MULEQUAL : return "*=";
        case LEXER_DIVEQUAL : return "/=";
        case LEXER_PLUSPLUS : return "++";
        case LEXER_MINUSMINUS : return "--";
        case LEXER_AND : return "&";
        case LEXER_OR : return "|";
        case LEXER_XOR : return "^";
        case LEXER_ANDAND : return "&&";
        case LEXER_OROR : return "||";
        
        // keywords
        case LEXER_RESERVED_IF : return "if";
        case LEXER_RESERVED_ELSE : return "else";
        case LEXER_RESERVED_DO : return "do";
        case LEXER_RESERVED_WHILE : return "while";
        case LEXER_RESERVED_FOR : return "for";
        case LEXER_RESERVED_BREAK : return "break";
        case LEXER_RESERVED_CONTINUE : return "continue";
        case LEXER_RESERVED_FUNCTION : return "function";
        case LEXER_RESERVED_RETURN : return "return";
        case LEXER_RESERVED_VAR : return "var";
        case LEXER_RESERVED_TRUE : return "true";
        case LEXER_RESERVED_FALSE : return "false";
        case LEXER_RESERVED_NULL : return "null";
        case LEXER_RESERVED_UNDEFINED : return "undefined";
        case LEXER_RESERVED_NEW : return "new";
        case LEXER_RESERVED_CLASS : return "class";
        case LEXER_RESERVED_PRIVATE : return "private";
        
    } 
        
    // in case of error
    std::ostringstream msg;
    msg << "(" << token << ")??";
    return msg.str();
       
}

void JLexer::getNextCh() {
    currCh = nextCh;
    if (dataPos < dataEnd)
        nextCh = data[dataPos];
    else
        nextCh = 0;
    dataPos++;
}

void JLexer::getNextToken() {
    tk = LEXER_EOF;
    tkStr.clear();

    while (currCh && isWhitespace(currCh)) {             
        getNextCh();
    }
        
    // line comments
    if (currCh=='/' && nextCh=='/') {
        while (currCh && (currCh != '\n')) 
            getNextCh();                                                                 
        getNextToken();
        return;
    } 
    
    if (currCh=='#') {
        while (currCh && (currCh != '\n')) 
            getNextCh();                                                                 
        getNextToken();
        return;
    } 
     
    // block comments
    if (currCh=='/' && nextCh=='*') {
        while (currCh && (currCh!='*' || nextCh!='/')) 
            getNextCh();
        getNextCh();
        getNextCh();
        getNextToken();
        return;
    } 
    // record beginning of this token 
    tokenStart = dataPos-2;
     
    // tokens
    if (isAlpha(currCh)) { 
        // identifiers
        while (isAlpha(currCh) || isNumeric(currCh)) {
            tkStr += currCh;
            getNextCh();
        }
        tk = LEXER_IDENTIFIER; 
        if (tkStr=="if") tk = LEXER_RESERVED_IF;
        else if (tkStr=="else") tk = LEXER_RESERVED_ELSE;
        else if (tkStr=="do") tk = LEXER_RESERVED_DO;
        else if (tkStr=="while") tk = LEXER_RESERVED_WHILE;
        else if (tkStr=="for") tk = LEXER_RESERVED_FOR;
        else if (tkStr=="break") tk = LEXER_RESERVED_BREAK;
        else if (tkStr=="continue") tk = LEXER_RESERVED_CONTINUE;
        else if (tkStr=="function") tk = LEXER_RESERVED_FUNCTION;
        else if (tkStr=="return") tk = LEXER_RESERVED_RETURN;
        else if (tkStr=="var") tk = LEXER_RESERVED_VAR;
        else if (tkStr=="true") tk = LEXER_RESERVED_TRUE;
        else if (tkStr=="false") tk = LEXER_RESERVED_FALSE;
        else if (tkStr=="null") tk = LEXER_RESERVED_NULL;
        else if (tkStr=="undefined") tk = LEXER_RESERVED_UNDEFINED;
        else if (tkStr=="new") tk = LEXER_RESERVED_NEW;
        else if (tkStr=="class") tk = LEXER_RESERVED_CLASS;
        else if (tkStr=="private") tk = LEXER_RESERVED_PRIVATE;
        
    } else if (isNumeric(currCh)) {         
        // Numbers
        bool isHex = false;
        if (currCh=='0') { 
            tkStr += currCh; 
            getNextCh(); 
        }
        if(currCh=='x') {
          isHex = true;
          tkStr += currCh; 
          getNextCh();
        } 
        tk = LEXER_INTEGER;
        while (isNumeric(currCh) || (isHex && isHexadecimal(currCh)) ) {
            
            tkStr += currCh;
            getNextCh();
            
            // convert binary
            if(currCh == 'b') {
                
                int i = stoi(tkStr, 0, 2);                              
                char *buffer = (char*)malloc(128);
                sprintf(buffer, "%d", i);
                tkStr = buffer;
                free(buffer);
                getNextCh();
                break;
            }
            
            // convert octal
            if(currCh == 'o') {
                int i = stoi(tkStr, 0, 8);
                char *buffer = (char*)malloc(128);
                sprintf(buffer, "%d", i);
                tkStr = buffer;
                free(buffer);
                getNextCh();
                break;
            }
            
        }
        if ( !isHex && currCh=='.') {
            tk = LEXER_DOUBLE;
            tkStr += '.';
            getNextCh();
            while (isNumeric(currCh)) {
                tkStr += currCh;
                getNextCh();
            }
        }
        // do fancy e-style floating point
        if ( !isHex && (currCh=='e'||currCh=='E')) {
          tk = LEXER_DOUBLE;
          tkStr += currCh; getNextCh();
          if (currCh=='-') { tkStr += currCh; getNextCh(); }
          while (isNumeric(currCh)) {
             tkStr += currCh; getNextCh();
          } 
        }

    } else if (currCh=='"') {
        // double quoted strings
        getNextCh();
        while (currCh && currCh!='"') {
            if (currCh == '\\') {
                getNextCh();
                switch (currCh) {
                case 'n' : tkStr += '\n'; break;
                case 't' : tkStr += '\t'; break;
                case 'r' : tkStr += '\r'; break;
                case 'a' : tkStr += '\a'; break;
                case '"' : tkStr += '"'; break;
                case '\\' : tkStr += '\\'; break;
                default: tkStr += currCh;
                }
            } else {
                tkStr += currCh;
            }
            getNextCh();
        }
        getNextCh();
        tk = LEXER_STRING;
    } else if (currCh=='\'') {
        
        // single quoted strings
        getNextCh();
        
        //while (currCh && currCh!='\'') {
        
            if (currCh == '\\') {
                getNextCh();
                switch (currCh) {
                case 'n' : tkStr += '\n'; break;
                case 'a' : tkStr += '\a'; break;
                case 'r' : tkStr += '\r'; break;
                case 't' : tkStr += '\t'; break;
                case '\'' : tkStr += '\''; break;
                case '\\' : tkStr += '\\'; break;
                case 'x' : { // hex digits must be 2 digits!
                              char buf[3] = "??";
                              getNextCh(); 
                              buf[0] = currCh;
                              getNextCh(); 
                              buf[1] = currCh;
                              tkStr += (char)strtol(buf,0,16);
                           } break;
                case 'o': /*if (currCh>='0' && currCh<='7')*/ {
                           getNextCh();
                           // octal digits
                           // todo deprecated
                           char buf[4] = "???";
                           buf[0] = currCh;
                           getNextCh(); buf[1] = currCh;
                           getNextCh(); buf[2] = currCh;
                           tkStr += (char)strtol(buf,0,8);
                         }
                 case 'b': /*if (currCh>='0' && currCh<='7')*/ {
                           getNextCh();
                           // octal digits
                           // todo deprecated
                           char buf[8] = "???";
                           buf[0] = currCh;
                           getNextCh(); buf[1] = currCh;
                           getNextCh(); buf[2] = currCh;
                           getNextCh(); buf[3] = currCh;
                           getNextCh(); buf[4] = currCh;
                           getNextCh(); buf[5] = currCh;
                           getNextCh(); buf[6] = currCh;
                           getNextCh(); buf[7] = currCh;
                           tkStr += (char)strtol(buf,0,2);
                         } 
                 default: tkStr += currCh;
                }
            } else {
                tkStr += currCh;
            }
            getNextCh();
             
            if(currCh != '\'') 
                throw new JAIL::Exception("A Char consists of 1 character in length except numbers.");
                
        //}
        
        getNextCh();
        tk = LEXER_CHARACTER; 
        
    } else {
        // single chars
        tk = currCh;
        if (currCh) 
            getNextCh();
        if (tk=='=' && currCh=='=') { // ==
            tk = LEXER_EQUAL;
            getNextCh();
            if (currCh=='=') { // ===
              tk = LEXER_TYPEEQUAL;
              getNextCh();
            }
        } else if (tk=='!' && currCh=='=') { // !=
            tk = LEXER_NEQUAL;
            getNextCh();
            if (currCh=='=') { // !==
              tk = LEXER_NTYPEEQUAL;
              getNextCh();
            }
        } else if (tk=='<' && currCh=='=') {
            tk = LEXER_LEQUAL;
            getNextCh();
        } else if (tk=='<' && currCh=='<') {
            tk = LEXER_LSHIFT;
            getNextCh();
            if (currCh=='=') { // <<=
              tk = LEXER_LSHIFTEQUAL;
              getNextCh();
            }
        } else if (tk=='>' && currCh=='=') {
            tk = LEXER_GEQUAL;
            getNextCh();
        } else if (tk=='>' && currCh=='>') {
            tk = LEXER_RSHIFT;
            getNextCh();
            if (currCh=='=') { // >>=
              tk = LEXER_RSHIFTEQUAL;
              getNextCh();
            } else if (currCh=='>') { // >>>
              tk = LEXER_RSHIFTUNSIGNED;
              getNextCh();
            }
        }  else if (tk=='+' && currCh=='=') {
            tk = LEXER_PLUSEQUAL;
            getNextCh();
        }  else if (tk=='-' && currCh=='=') {
            tk = LEXER_MINUSEQUAL;
            getNextCh();
        }  else if (tk=='*' && currCh=='=') {
            tk = LEXER_MULEQUAL;
            getNextCh();
        }  else if (tk=='/' && currCh=='=') {
            tk = LEXER_DIVEQUAL;
            getNextCh();
        }  else if (tk=='+' && currCh=='+') {
            tk = LEXER_PLUSPLUS;
            getNextCh();
        }  else if (tk=='-' && currCh=='-') {
            tk = LEXER_MINUSMINUS;
            getNextCh();
        } 
        
        else if (tk=='&' && currCh=='&') {
            tk = LEXER_ANDAND;
            getNextCh();
        } else if (tk=='|' && currCh=='|') {
            tk = LEXER_OROR;
            getNextCh();
        } 
        
        else if (tk=='&' && currCh!='&') {
            tk = LEXER_AND;
            getNextCh();
        } else if (tk=='|' && currCh!='|') {
            tk = LEXER_OR;
            getNextCh();
        }
        
        else if (tk=='^') {
            tk = LEXER_XOR;
            getNextCh();
        }

    }
    /* This isn't quite right yet */
    tokenLastEnd = tokenEnd;
    tokenEnd = dataPos-3;
}

std::string JLexer::getSubString(int lastPosition) {
    int lastCharIdx = tokenLastEnd + 1;
    if (lastCharIdx < dataEnd) {
        /* save a memory alloc by using our data array to create the substring */
        char old = data[lastCharIdx];
        data[lastCharIdx] = 0;
        std::string value = &data[lastPosition];
        data[lastCharIdx] = old;
        return value;
    } else {
        return std::string(&data[lastPosition]);
    }
}


JLexer *JLexer::getSubLex(int lastPosition) {
    int lastCharIdx = tokenLastEnd + 1;
    if (lastCharIdx < dataEnd)
        return new JLexer(this, lastPosition, lastCharIdx);
    else
        return new JLexer(this, lastPosition, dataEnd);
}

// todo lines are not counted correctly
std::string JLexer::getPosition(int pos) {
    if (pos<0) pos = tokenLastEnd;
    int line = 1,col = 1;
    for (int i=0; i<pos; i++) {
        //printf(" next pos\n");
        char ch;
        if (i < dataEnd)
            ch = data[i];
        else
            ch = 0;
        col++;
        //printf("%c %d", ch, ch);
        if (ch=='\n' || (ch == '\r' && data[i+1] == '\n') ) {
            line++;
            col = 0;
        }
    }
    char buf[256];
    sprintf_s(buf, 256, "(line: %d, col: %d)", line, col );
    return buf;
}

// ----------------------------------------------------------------------------------- CSCRIPTVARLINK

JLink::JLink(JObject *var, const std::string &varName) : name(varName) {
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    this->nextSibling = 0;
    this->prevSibling = 0;
    this->var = var->ref();
    this->owned = false;

}

JLink::JLink(const JLink &link) : name(link.name) {
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    this->nextSibling = 0;
    this->prevSibling = 0;
    this->var = link.var->ref();
    this->owned = false;

}

JLink::~JLink() {
#if DEBUG_MEMORY
    mark_deallocated(this);
#endif
    var->unref();
}

void JLink::replaceWith(JObject *newVar) {
    JObject *oldVar = var;
    var = newVar->ref();
    oldVar->unref();
}

void JLink::replaceWith(JLink *newVar) {
    if (newVar)
      replaceWith(newVar->var);
    else
      replaceWith(new JObject());
}

int JLink::getIntName() const {
    return atoi(name.c_str());
}

void JLink::setIntName(int n) {
    char sIdx[64];
    sprintf_s(sIdx, sizeof(sIdx), "%d", n);
    name = sIdx;
}

// ----------------------------------------------------------------------------------- JObject

JObject::JObject() {
    refs = 0;
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    init();
    flags = VARIABLE_UNDEFINED;
}

JObject::JObject(const std::string &str) {
    refs = 0;
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    init();
    flags = VARIABLE_STRING;
    stringData = str;
}


JObject::JObject(const std::string &varData, int varFlags) {
    refs = 0;
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    init();
    flags = varFlags;
    if (varFlags & VARIABLE_INTEGER) {
      intData = strtol(varData.c_str(),0,0);
    } else if (varFlags & VARIABLE_DOUBLE) {
      doubleData = strtod(varData.c_str(),0);
    } else if (varFlags & VARIABLE_CHAR) {
      charData = varData.c_str()[0];
    } else
    stringData = varData;
}

JObject::JObject(double val) {
    refs = 0;
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    init();
    setDouble(val); 
}

JObject::JObject(int val) {
    refs = 0;
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    init();
    setInt(val); 
}

JObject::JObject(char val) {
    refs = 0;
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    init();
    setChar(val); 
}

JObject::JObject(const std::vector<unsigned char> &val) {
    refs = 0;
#if DEBUG_MEMORY
    mark_allocated(this);
#endif
    init();
    setArray(val);  
}

JObject::~JObject(void) {
#if DEBUG_MEMORY
    mark_deallocated(this);
#endif
    removeAllChildren();
}

void JObject::init() {
    firstChild = 0;
    lastChild = 0;
    flags = 0;
    jsCallback = 0;
    jsCallbackUserData = 0;
    charData = 0;
    stringData = JAIL_BLANK_DATA;
    intData = 0;
    doubleData = 0;
    hidden = false;
}

JObject *JObject::getReturnVar() {
    return getParameter(JAIL_RETURN_VAR);
}

void JObject::setReturnVar(JObject *var) {
    findChildOrCreate(JAIL_RETURN_VAR)->replaceWith(var);
}


JObject *JObject::getParameter(const std::string &name) {
    return findChildOrCreate(name)->var;
}

JLink *JObject::findChild(const std::string &childName) const {
    JLink *v = firstChild;
    while (v) {
        if (v->name.compare(childName)==0)
            return v;
        v = v->nextSibling;
    }
    return 0;
}

JLink *JObject::findChildOrCreate(const std::string &childName, int varFlags) {
    JLink *l = findChild(childName);
    if (l) return l;

    return addChild(childName, new JObject(JAIL_BLANK_DATA, varFlags));
}

JLink *JObject::findChildOrCreateByPath(const std::string &path) {
  size_t p = path.find('.');
  if (p == std::string::npos)
    return findChildOrCreate(path);

  return findChildOrCreate(path.substr(0,p), VARIABLE_OBJECT)->var->findChildOrCreateByPath(path.substr(p+1));
}

JLink *JObject::addChild(const std::string &childName, JObject *child) {
  if (isUndefined()) {
    flags = VARIABLE_OBJECT;
  }
    // if no child supplied, create one
    if (!child)
      child = new JObject();

    JLink *link = new JLink(child, childName);
    link->owned = true;
    if (lastChild) {
        lastChild->nextSibling = link;
        link->prevSibling = lastChild;
        lastChild = link;
    } else {
        firstChild = link;
        lastChild = link;
    }
    return link;
}

JLink *JObject::addChildNoDup(const std::string &childName, JObject *child) {
    // if no child supplied, create one
    if (!child)
      child = new JObject();

    JLink *v = findChild(childName);
    if (v) {
        v->replaceWith(child);
    } else {
        v = addChild(childName, child);
    }

    return v;
}

void JObject::removeChild(JObject *child) {
    JLink *link = firstChild;
    while (link) {
        if (link->var == child)
            break;
        link = link->nextSibling;
    }
    ASSERT(link);
    removeLink(link);
}

void JObject::removeLink(JLink *link) {
    if (!link) return;
    if (link->nextSibling)
      link->nextSibling->prevSibling = link->prevSibling;
    if (link->prevSibling)
      link->prevSibling->nextSibling = link->nextSibling;
    if (lastChild == link)
        lastChild = link->prevSibling;
    if (firstChild == link)
        firstChild = link->nextSibling;
    delete link;
}

void JObject::removeAllChildren() {
    JLink *c = firstChild;
    while (c) {
        JLink *t = c->nextSibling;
        delete c;
        c = t;
    }
    firstChild = 0;
    lastChild = 0;
}

JObject *JObject::getArrayIndex(int idx) const {
    char sIdx[64];
    sprintf_s(sIdx, sizeof(sIdx), "%d", idx);
    JLink *link = findChild(sIdx);
    if (link) return link->var;
    else return new JObject(JAIL_BLANK_DATA, VARIABLE_NULL); // undefined
}

void JObject::setArrayIndex(int idx, JObject *value) {
    char sIdx[64];
    sprintf_s(sIdx, sizeof(sIdx), "%d", idx);
    JLink *link = findChild(sIdx);

    if (link) {
      if (value->isUndefined())
        removeLink(link);
      else
        link->replaceWith(value);
    } else {
      if (!value->isUndefined())
        addChild(sIdx, value);
    }
}

int JObject::getArrayLength() const {
    int highest = -1;
    if (!isArray()) return 0;

    JLink *link = firstChild;
    while (link) {
      if (isNumber(link->name)) {
        int val = atoi(link->name.c_str());
        if (val > highest) highest = val;
      }
      link = link->nextSibling;
    }
    return highest+1;
}

int JObject::getChildren() const {
    int n = 0;
    JLink *link = firstChild;
    while (link) {
      n++;
      link = link->nextSibling;
    }
    return n;
}

const std::vector<unsigned char> JObject::getArray() const {
    std::vector<unsigned char> out;
    if (isArray()) {
        int arrayLength = getArrayLength();
        out.reserve(arrayLength);
        for (int i = 0; i < arrayLength; ++i) {
            JAIL::JObject *cell = getArrayIndex(i);
            out.push_back(static_cast<unsigned char>(cell->getInt()));
        }
    }
    return out;
}

char JObject::getChar() const {
    /* strtol understands about hex and octal */
    if(isChar() || isInt()) return charData;
    if (isNull()) return 0;
    if (isUndefined()) return 0;
    if (isDouble()) return 0;
    return 0;
}

int JObject::getInt() const {
    /* strtol understands about hex and octal */
    if (isInt()) return intData;
    if(isChar()) return charData;
    if (isNull()) return 0;
    if (isUndefined()) return 0;
    if (isDouble()) return (int)doubleData;
    return 0;
}

double JObject::getDouble() const {
    if (isDouble()) return doubleData;
    if (isInt()) return intData;
    if(isChar()) return (int)charData;
    if (isNull()) return 0;
    if (isUndefined()) return 0;
    return 0; /* or NaN? */
}

const std::string JObject::getString() const {
    /* Because we can't return a string that is generated on demand.
     * I should really just use char* :) */
    static std::string s_null = "null";
    static std::string s_undefined = "undefined";
    if (isInt()) {
      char buffer[32];
      sprintf_s(buffer, sizeof(buffer), "%ld", intData);
      return buffer;
    }
    if (isChar()) {
      char buffer[8];
      sprintf_s(buffer, sizeof(buffer), "%c", charData);
      return buffer;
    }
    if (isDouble()) {
      char buffer[32];
      sprintf_s(buffer, sizeof(buffer), "%f", doubleData);
      return buffer;
    }
    if (isNull()) return s_null;
    if (isUndefined()) return s_undefined;
    // are we just a string here?
    return stringData;
}

void JObject::setInt(int val) {
    flags = (flags&~VARIABLE_TYPEMASK) | VARIABLE_INTEGER;
    intData = val;
    charData = 0;
    doubleData = 0;
    stringData = JAIL_BLANK_DATA;
}

void JObject::setDouble(double val) {
    flags = (flags&~VARIABLE_TYPEMASK) | VARIABLE_DOUBLE;
    doubleData = val;
    intData = 0;
    charData = 0;
    stringData = JAIL_BLANK_DATA;
}

void JObject::setString(std::string str) {
    // name sure it's not still a number or integer
    flags = (flags&~VARIABLE_TYPEMASK) | VARIABLE_STRING;
    stringData = str;
    intData = 0;
    doubleData = 0;
    charData = 0;
}

void JObject::setChar(char val) {
    // name sure it's not still a number or integer
    flags = (flags&~VARIABLE_TYPEMASK) | VARIABLE_CHAR;
    charData = val;
    stringData = "" + val;
    intData = 0;
    doubleData = 0;
}

void JObject::setUndefined() {
    // name sure it's not still a number or integer
    flags = (flags&~VARIABLE_TYPEMASK) | VARIABLE_UNDEFINED;
    stringData = JAIL_BLANK_DATA;
    charData = 0;
    intData = 0;
    doubleData = 0;
    removeAllChildren();
}

void JObject::setArray() {
    // name sure it's not still a number or integer
    flags = (flags&~VARIABLE_TYPEMASK) | VARIABLE_ARRAY;
    stringData = JAIL_BLANK_DATA;
    charData = 0;
    intData = 0;
    doubleData = 0;
    removeAllChildren();
}

void JObject::setArray(const std::vector<unsigned char> &val) {
    // name sure it's not still a number or integer
    flags = (flags&~VARIABLE_TYPEMASK) | VARIABLE_ARRAY;
    stringData = JAIL_BLANK_DATA;
    charData = 0;
    intData = 0;
    doubleData = 0;
    removeAllChildren();
    for (std::vector<unsigned char>::size_type i = 0; i < val.size(); ++i) {
        setArrayIndex(i, new JAIL::JObject(static_cast<int>(val[i])));
    }
}

bool JObject::equals(const JObject *v) {
    JObject *resV = mathsOp(v, LEXER_EQUAL);
    bool res = resV->getBool();
    delete resV;
    return res;
}

bool JObject::isInt() const          { return (flags&VARIABLE_INTEGER)!=0; }
bool JObject::isDouble() const       { return (flags&VARIABLE_DOUBLE)!=0; }
bool JObject::isString() const       { return (flags&VARIABLE_STRING)!=0; }
bool JObject::isChar() const         { return (flags&VARIABLE_CHAR)!=0; }
bool JObject::isNumeric() const      { return (flags&VARIABLE_NUMERICMASK)!=0; }
bool JObject::isFunction() const     { return (flags&VARIABLE_FUNCTION)!=0; }
bool JObject::isObject() const       { return (flags&VARIABLE_OBJECT)!=0; }
bool JObject::isArray() const        { return (flags&VARIABLE_ARRAY)!=0; }
bool JObject::isNative() const       { return (flags&VARIABLE_NATIVE)!=0; }
bool JObject::isUndefined() const    { return (flags & VARIABLE_TYPEMASK) == VARIABLE_UNDEFINED; }
bool JObject::isNull() const         { return (flags & VARIABLE_NULL)!=0; }
bool JObject::isBasic() const        { return firstChild==0; }
bool JObject::isAnything() const     { return isObject() || isFunction(); };

/**
 * Mathematical operations
 * one of the most used functions
 */
JObject *JObject::mathsOp(const JObject *b, int op) {
    
    JObject *a = this;
    
    /**
     * Type equality check
     */
    if (op == LEXER_TYPEEQUAL || op == LEXER_NTYPEEQUAL) {
      // check type first, then call again to check data
      bool eql = ((a->flags & VARIABLE_TYPEMASK) == (b->flags & VARIABLE_TYPEMASK));
      if (eql) {
            JObject *contents = a->mathsOp(b, LEXER_EQUAL);
            if (!contents->getBool()) 
                eql = false;
            if (!contents->refs) 
                delete contents;
      }
      if (op == LEXER_TYPEEQUAL)
            return new JObject(eql);
      else
            return new JObject(!eql);
    }
    
    /**
     * Math on undefined
     * return immediately
     */
    if (a->isUndefined() && b->isUndefined()) {
      
        if (op == LEXER_EQUAL) 
            return new JObject(true);
            
        else if (op == LEXER_NEQUAL) 
            return new JObject(false);
            
        else 
            return new JObject(); // undefined by default
            
    } 
    
    /**
     * If one of both are numeric
     * a Char is also numeris!
     */
    else if ( (a->isNumeric() || a->isUndefined()) && (b->isNumeric() || b->isUndefined()) ) {
    
        // if both are NOT double, we work on integers
        if (!a->isDouble() && !b->isDouble()) {
        
            int da = a->getInt();
            int db = b->getInt();
            
            switch (op) {
            
                case '+':               return new JObject(da + db);
                case '-':               return new JObject(da - db);
                case '*':               return new JObject(da * db);
                case '/':               return new JObject(da / db);
                case '&':               return new JObject(da & db);
                case '|':               return new JObject(da | db);
                case '^':               return new JObject(da ^ db);
                case '%':               return new JObject(da % db);                
                case LEXER_EQUAL:       return new JObject(da == db);
                case LEXER_NEQUAL:      return new JObject(da != db);
                case '<':               return new JObject(da < db);
                case LEXER_LEQUAL:      return new JObject(da <= db);
                case '>':               return new JObject(da > db);
                case LEXER_GEQUAL:      return new JObject(da >= db);
                
                default: throw new Exception("Requested operation "+JLexer::getTokenStr(op)+" not supported on the Integer datatype. (https://jail.lima-city.at/docs/general/datatypes/integer)");
            
            }
            
        } 
        
        // if ONE or even BOTH of them are a double we work on double
        else {
        
            double da = a->getDouble();
            double db = b->getDouble();
            
            switch (op) {
            
                case '+':               return new JObject(da + db);
                case '-':               return new JObject(da - db);
                case '*':               return new JObject(da * db);
                case '/':               return new JObject(da / db);
                
                case LEXER_EQUAL:       return new JObject(da == db);
                case LEXER_NEQUAL:      return new JObject(da != db);
                case '<':               return new JObject(da < db);
                case LEXER_LEQUAL:      return new JObject(da <= db);
                case '>':               return new JObject(da > db);
                case LEXER_GEQUAL:      return new JObject(da >= db);
                
                default: throw new Exception("Requested operation "+JLexer::getTokenStr(op)+" not supported on the Double datatype. (https://jail.lima-city.at/docs/general/datatypes/double)");
            
            }
            
        }
        
    } 
    
    /**
     * Array math
     */
    else if (a->isArray()) {
      
        switch (op) {
        
            /* Just check pointers */
             case LEXER_EQUAL:          return new JObject(a == b);
             case LEXER_NEQUAL:         return new JObject(a != b);
             
             /**
              * Array addition
              * beta
              * todo
              */
             case '+': {                                      
                  JObject *tmp = new JObject();
                  tmp->setArray();                
                  JLink *lhs = a->firstChild;
               
                  int i = 0;                
                  while(lhs) {                    
                      tmp->setArrayIndex(i, lhs->var);                    
                      lhs = lhs->nextSibling;
                      i++;                    
                  }                 
                  return tmp;                
             }
             
             default: throw new Exception("Requested operation "+JLexer::getTokenStr(op)+" not supported on the Array datatype, (https://jail.lima-city.at/docs/general/datatypes/array)");
        
        }
      
    }
     
    /**
     * Object math
     * 
     * If a is an object, see if it has 'value' property, if so change it, not a!
     */
    else if (a->isObject()) { 
       
          //JLink *vl = new JLink(new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT));       
          
          switch (op) {
               
               /* Just check pointers */          
               case LEXER_EQUAL: return new JObject(a==b);
               case LEXER_NEQUAL: return new JObject(a!=b);
               
               /* 
               case '+': { 
                    JLink *child = b->firstChild;
                    while(child) {
                        a->addChildNoDup(child->name, child->var);
                        child = child->nextSibling;
                    }
                    return a;                                 
               }               
               case '-': {                     
                    JLink *lhs = a->firstChild;
                    JLink *rhs = b->firstChild;                    
                    while(rhs) {
                        if(!lhs) 
                            break;                        
                        if(!lhs->name.compare(rhs->name)) 
                            a->removeChild(lhs->var);
                        lhs = lhs->nextSibling;
                        rhs = rhs->nextSibling;
                    }
                    return a;
               }
               case '*': {
                    JLink *lhs = a->firstChild;
                    JLink *rhs = b->firstChild;
                    JObject *tmp;
                    if(a->isObject()) {
                        while(lhs) {
                            if(lhs->name.compare("value") == 0) {
                                if(b->isObject()) {                                    
                                    tmp = new JObject(lhs->var->getInt() * rhs->var->getInt());                                    
                                }                                
                                else
                                    tmp = new JObject(lhs->var->getInt() * b->getInt());                                    
                                break;                                                
                            }                                        
                            lhs = lhs->nextSibling;                                   
                        }                        
                    } else {                    
                        if(b->isObject()) {                                    
                            while(rhs) {                            
                                if(rhs->name.compare("value") == 0) {                                            
                                    tmp = new JObject(a->getInt() * rhs->var->getInt());                                        
                                    break;                                                    
                                }
                                rhs = lhs->nextSibling;
                            }
                        } else {
                            tmp = new JObject(a->getInt() * b->getInt());
                        }
                    }                    
                    return tmp;                                                     
               } 
               */
                            
               default: throw new Exception("Requested operation "+JLexer::getTokenStr(op)+" not supported on the Object datatype, Visit https://jail.lima-city.at/docs/general/datatypes/object for more information.");
          
          }
                    
    }
    

    
    /**
     * String math
     */
    else {
    
        std::string da = a->getString();
        std::string db = b->getString();
         
        switch (op) {
         
            case '+':               return new JObject(da + db, VARIABLE_STRING);
            case LEXER_EQUAL:       return new JObject(da == db);
            case LEXER_NEQUAL:      return new JObject(da != db);
            case '<':               return new JObject(da < db);
            case LEXER_LEQUAL:      return new JObject(da <= db);
            case '>':               return new JObject(da > db);
            case LEXER_GEQUAL:      return new JObject(da >= db);
            
            default: throw new Exception("Requested operation "+JLexer::getTokenStr(op)+" not supported on the string datatype, Visit https://jail.lima-city.at/docs/general/datatypes/string for more information.");
             
        }
       
    }
    
    ASSERT(0);
    return 0;
    
}

void JObject::copySimpleData(const JObject *val) {
    stringData = val->stringData;
    intData = val->intData;
    doubleData = val->doubleData;
    charData = val->charData;
    hidden = val->hidden;
    flags = (flags & ~VARIABLE_TYPEMASK) | (val->flags & VARIABLE_TYPEMASK);
}

void JObject::copyValue(const JObject *val) {
    
    if(val) {        
        copySimpleData(val);
        // remove all current children
        removeAllChildren();
        // copy children of 'val'
        JLink *child = val->firstChild;
        while (child) {            
            JObject *copied;            
            // don't copy the 'parent' object...
            if (child->name != JAIL_PROTOTYPE_CLASS)
                copied = child->var->deepCopy();
            else
                copied = child->var;    
            addChild(child->name, copied);    
            child = child->nextSibling;            
        }
    } 
        
    else 
        setUndefined();
    
}

JObject *JObject::deepCopy() const {

    JObject *newVar = new JObject();
    newVar->copySimpleData(this);    
    
    // copy children
    JLink *child = firstChild;
    while (child) {
        JObject *copied;
        // don't copy the 'parent' object...
        if (child->name != JAIL_PROTOTYPE_CLASS)
            copied = child->var->deepCopy();
        else
            copied = child->var;
        newVar->addChild(child->name, copied);
        child = child->nextSibling;
    }
        
    return newVar;
    
}

void JObject::trace(const std::string &indentStr, const std::string &name) const {
    TRACE("%s'%s' = '%s' %s\n", indentStr.c_str(), name.c_str(), getString().c_str(), getFlagsAsString().c_str());
    std::string indent = indentStr + " ";
    JLink *link = firstChild;
    while (link) {
        link->var->trace(indent, link->name);
        link = link->nextSibling;
    }
}

std::string JObject::getFlagsAsString() const {
    std::string flagstr = "";
    if (flags&VARIABLE_FUNCTION) flagstr = flagstr + "FUNCTION ";
    if (flags&VARIABLE_OBJECT) flagstr = flagstr + "OBJECT ";
    if (flags&VARIABLE_ARRAY) flagstr = flagstr + "ARRAY ";
    if (flags&VARIABLE_NATIVE) flagstr = flagstr + "NATIVE ";
    if (flags&VARIABLE_DOUBLE) flagstr = flagstr + "DOUBLE ";
    if (flags&VARIABLE_INTEGER) flagstr = flagstr + "INTEGER ";
    if (flags&VARIABLE_STRING) flagstr = flagstr + "STRING ";
    if (flags&VARIABLE_CHAR) flagstr = flagstr + "CHAR ";
    return flagstr;
}

std::string& ltrim(std::string& str, const std::string& chars = "\t\n\v\f\r ") {
    str.erase(0, str.find_first_not_of(chars));
    return str;
} 

std::string& rtrim(std::string& str, const std::string& chars = "\t\n\v\f\r ") {
    str.erase(str.find_last_not_of(chars) + 1);
    return str;
} 

std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ") {
    return ltrim(rtrim(str, chars), chars);
}

std::string JObject::getParsableString() const {
  
    // Numbers can just be put in directly
    if(isNumeric())
        return getString();
      
    if (isFunction()) {
    
        std::ostringstream funcStr;
        funcStr << "function (";
        
        // get list of parameters
        JLink *link = firstChild;
        while (link) {
            funcStr << link->name;
            if (link->nextSibling) 
                funcStr << ", ";
            link = link->nextSibling;
        }
        funcStr << ") ";                              
        // add function body
        //funcStr << getString();
        return funcStr.str();
      
    }
    
    // if it is a string then we quote it
    if (isString())
        return getJSString(getString());
    
    // if it is a char we quote it in single quotes
    if (isChar()) {
        std::stringstream ss;
        ss << "'" << getString() << "'";
        return ss.str(); //link->var->getString();    
    }
    
    if (isNull())
        return "null";
        
    return "undefined";
  
}

void JObject::getJSON(std::ostringstream &destination, const std::string &linePrefix) const  {
    
    if (isObject()) {
        std::string indentedLinePrefix = linePrefix + '\t';      
        // first line
        destination << "{\n";      
        // children - handle with bracketed list      
        JLink *link = firstChild;
        while (link) {
            destination << indentedLinePrefix;
            destination << getJSString(link->name);
            destination << ": ";
            link->var->getJSON(destination, indentedLinePrefix);
            link = link->nextSibling;
            if(link) 
                destination  << ",\n";
        }     
        // last line
        destination << "\n" << linePrefix << "}";      
    } 
    
    else if (isArray()) {
        std::string indentedLinePrefix = linePrefix + '\t';     
        destination << "[ ";
        int len = getArrayLength();
        if(len>10000) 
            len=10000; // we don't want to get stuck here!
        for(int i=0;i<len;i++) {
            getArrayIndex(i)->getJSON(destination, indentedLinePrefix);
            if (i<len-1) 
                destination  << ", ";
        }
        // last line
        destination << " " <<  "]";
    } 
    
    else 
        // no children or a function... just write value directly
        destination << getParsableString();
    
}

void JObject::setCallback(Callback callback, void *userdata) {
    jsCallback = callback;
    jsCallbackUserData = userdata;
}

JObject *JObject::ref() {
    refs++;
    return this;
}

void JObject::unref() {
    // todo
    // if (refs<=0) printf("INFO: JObject::unref too early!\n");
    if ((--refs)==0) {
      delete this;
    }
}

int JObject::getRefs() const {
    return refs;
}

// ----------------------------------------------------------------------------------- JInterpreter

JInterpreter::JInterpreter() {
    
    stepping = false;
    l = 0;
    root = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    // Add built-in classes
    charClass = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    intClass = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    doubleClass = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    stringClass = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    arrayClass = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    objectClass = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    
    root->addChild("Char", charClass);
    root->addChild("Integer", intClass);
    root->addChild("Double", doubleClass);
    root->addChild("String", stringClass);
    root->addChild("Array", arrayClass);
    root->addChild("Object", objectClass);
    
    
}

void JInterpreter::reset() {
    
    stepping = false;
    l = 0;
    root = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    
    // Add built-in classes
    charClass = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    intClass = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    doubleClass = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    stringClass = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    arrayClass = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    objectClass = (new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT))->ref();
    
    root->addChild("Char", charClass);
    root->addChild("Integer", intClass);
    root->addChild("Double", doubleClass);
    root->addChild("String", stringClass);
    root->addChild("Array", arrayClass);
    root->addChild("Object", objectClass);
    
    
}

void JInterpreter::Stepping(bool state) {
    stepping = state;
}

JInterpreter::~JInterpreter() {
    
    //ASSERT(!l);
    scopes.clear();
    charClass->unref();
    doubleClass->unref();
    intClass->unref();
    stringClass->unref();
    arrayClass->unref();
    objectClass->unref();
    root->unref();

#if DEBUG_MEMORY
    show_allocated();
#endif

}

void JInterpreter::trace() {
    root->trace();
}


/**
 * Executing given code
 */
void JInterpreter::execute(std::string code) {
    
    JLexer *oldLex = l;
    std::vector<JObject*> oldScopes = scopes;
    l = new JLexer(code);
    
#ifdef JAIL_CALL_STACK
    call_stack.clear();
#endif

    scopes.clear();
    scopes.push_back(root);
    
    try {
        bool execute = true;
        while (l->tk) {
            statement(execute);
        }
        
    } catch (Exception *e) { 
        std::ostringstream msg;
        msg << e->text;
        
#ifdef JAIL_CALL_STACK
        for (int i=(int)call_stack.size()-1;i>=0;i--)
            msg << "\n" << i << ": " << call_stack.at(i);
#endif

        msg << " at " << l->getPosition(); 
        delete l;
        l = oldLex;

        throw new Exception(msg.str());
        
    }
    
    delete l;
    l = oldLex;
    scopes = oldScopes;
    
}

JLink JInterpreter::evaluateComplex(const std::string &code) {
    
    JLexer *oldLex = l;
    std::vector<JObject*> oldScopes = scopes;
    l = new JLexer(code);
    
#ifdef JAIL_CALL_STACK
    call_stack.clear();
#endif

    scopes.clear();
    scopes.push_back(root);
    JLink *v = 0;
    
    try {
    
        bool execute = true;
        do {
            CLEAN(v);
            v = base(execute);
            if (l->tk!=LEXER_EOF) l->match(';');
        } while (l->tk!=LEXER_EOF);
        
    } catch (Exception *e) {
    
        std::ostringstream msg;
        msg << e->text;
        
#ifdef JAIL_CALL_STACK
        for (int i=(int)call_stack.size()-1;i>=0;i--)
          msg << "\n" << i << ": " << call_stack.at(i);
#endif

        msg << " at " << l->getPosition();
        delete l;
        l = oldLex;
  
        throw new Exception(msg.str());
        
    }
    delete l;
    l = oldLex;
    scopes = oldScopes;

    if (v) {
        JLink r = *v;
        CLEAN(v);
        return r;
    }
    // return undefined...
    return JLink(new JObject());
}

std::string JInterpreter::evaluate(const std::string &code) {
    return evaluateComplex(code).var->getString();
}

void JInterpreter::parseFunctionArguments(JObject *funcVar) {  
    l->match('(');    
    while (l->tk!=')') {
        funcVar->addChildNoDup(l->tkStr);
        l->match(LEXER_IDENTIFIER);
        if (l->tk!=')') l->match(',');
    }    
    l->match(')');  
} 

void JInterpreter::addNative(std::string funcDesc, Callback ptr, void *userdata) {
    
    JLexer *oldLex = l;
    l = new JLexer(funcDesc);
    JObject *base = root;
    l->match(LEXER_RESERVED_FUNCTION);
    std::string funcName = l->tkStr;     
    l->match(LEXER_IDENTIFIER);    
    
    /* Check for dots, we might want to do something like function String.substring ... */
    while (l->tk == '.') {
        l->match('.');
        JLink *link = base->findChild(funcName);
        // if it doesn't exist, make an object class
        if (!link) 
              link = base->addChild(funcName, new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT));
        base = link->var;
        funcName = l->tkStr;
        l->match(LEXER_IDENTIFIER);
    }
    
    JObject *funcVar = new JObject(JAIL_BLANK_DATA, VARIABLE_FUNCTION | VARIABLE_NATIVE);
    funcVar->setCallback(ptr, userdata);
    parseFunctionArguments(funcVar);
    delete l;
    l = oldLex;
    base->addChild(funcName, funcVar);
    
} 

void JInterpreter::addNative(std::string returnType, std::string funcDesc, Callback ptr, void *userdata) {
    
    JLexer *oldLex = l;
    l = new JLexer(funcDesc);
    JObject *base = root;        
    
    l->match(LEXER_RESERVED_FUNCTION);
        
    std::string funcName = l->tkStr;

    l->match(LEXER_IDENTIFIER);
    
    /* Check for dots, we might want to do something like function String.substring ... */
    while (l->tk == '.') {
        l->match('.');
        JLink *link = base->findChild(funcName);
        // if it doesn't exist, make an object class
        if (!link) 
          link = base->addChild(funcName, new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT));
        base = link->var;
        funcName = l->tkStr;
        l->match(LEXER_IDENTIFIER);
    }

    JObject *funcVar = new JObject(JAIL_BLANK_DATA, VARIABLE_FUNCTION | VARIABLE_NATIVE);
    funcVar->setCallback(ptr, userdata);
    parseFunctionArguments(funcVar);
    delete l;
    l = oldLex;

    base->addChild(funcName, funcVar);
    
}

// example
// doku
// todo
JLink *JInterpreter::parseFunctionDefinition() {
    
    // actually parse a function...
    l->match(LEXER_RESERVED_FUNCTION);
    std::string funcName = JAIL_TEMP_NAME;
    
    /* we can have functions without names */
    if (l->tk==LEXER_IDENTIFIER) {
        funcName = l->tkStr;
        l->match(LEXER_IDENTIFIER);
    }
    
    JLink *funcVar = new JLink(new JObject(JAIL_BLANK_DATA, VARIABLE_FUNCTION), funcName);
    parseFunctionArguments(funcVar->var);
    int funcBegin = l->tokenStart;
    bool execute = false;
    // get the body
    block(execute);
    funcVar->var->stringData = l->getSubString(funcBegin);
    return funcVar;
    
}

/** 
 * Handle a function call (assumes we've parsed the function name and we're
 * on the start bracket). 'parent' is the object that contains this method,
 * if there was one (otherwise it's just a normnal function).
 */
JLink *JInterpreter::functionCall(bool &execute, JLink *function, JObject *parent) {
    
    if (execute) {
    
        if (!function->var->isFunction()) {
          std::ostringstream msg;
          msg << "Call for unknown function: '" << function->name << "'. (https://jail.lima-city.at/docs/general/functions)";
          throw new Exception(msg.str());
        }
        
        l->match('(');
        
        // create a new symbol table entry for execution of this function
        JObject *functionRoot = new JObject(JAIL_BLANK_DATA, VARIABLE_FUNCTION);
        
        if (parent)
          functionRoot->addChildNoDup("this", parent);
          
        // grab in all parameters
        // todo error handling on parameter count 
        try {
            
            JLink *v = function->var->firstChild; 
            
            while (v) {
                
                JLink *value = base(execute);
                
                if (execute) {
                    if (value->var->isBasic()) {
                      // pass by value
                      functionRoot->addChild(v->name, value->var->deepCopy());
                    } else {
                      // pass by reference
                      functionRoot->addChild(v->name, value->var);
                    }
                }
                 
                CLEAN(value); 
                
                if (l->tk!=')') 
                    l->match(',');
                v = v->nextSibling; 
                
            }
             
        } catch(Exception *e) {
            std::ostringstream msg;
            msg << "Error when calling '" << function->name << "()': Maybe wrong parameter count?\n\t" << e->text;
            throw new Exception(msg.str());
        } 
        
        l->match(')'); 
        
        // setup a return JObject
        JLink *returnVar = NULL;
        
        // execute function!
        // add the function's execute space to the symbol table so we can recurse
        JLink *returnVarLink = functionRoot->addChild(JAIL_RETURN_VAR);
        scopes.push_back(functionRoot);
      
#ifdef JAIL_CALL_STACK
    call_stack.push_back(function->name + " from " + l->getPosition());
#endif
  
        if (function->var->isNative()) {
            ASSERT(function->var->jsCallback);
            function->var->jsCallback(functionRoot, function->var->jsCallbackUserData);
        } 
        else {
            /* we just want to execute the block, but something could
             * have messed up and left us with the wrong JLexer, so
             * we want to be careful here... */
            Exception *Exception = 0;
            JLexer *oldLex = l;
            JLexer *newLex = new JLexer(function->var->getString());
            l = newLex;
            try {
                block(execute);
                // because return will probably have called this, and set execute to false
                execute = true;
            } catch (JAIL::Exception *e) {
                Exception = e;
            }
            delete newLex;
            l = oldLex;
    
            if (Exception)
                throw Exception;
        }
      
#ifdef JAIL_CALL_STACK
    if (!call_stack.empty()) call_stack.pop_back();
#endif
  
        scopes.pop_back();
        
        /* get the real return var before we remove it from our function */
        returnVar = new JLink(returnVarLink->var);
        functionRoot->removeLink(returnVarLink);
        delete functionRoot;
        
        if (returnVar)
            return returnVar;
        else
            return new JLink(new JObject());
        
    } else {
    
        // function, but not executing - just parse args and be done
        l->match('(');
        while (l->tk != ')') {
            JLink *value = base(execute);
            CLEAN(value);
            if (l->tk!=')') l->match(',');
        }
        l->match(')');
        if (l->tk == '{') { // TODO: why is this here?
            block(execute);
        }
        /* function will be a blank scriptvarlink if we're not executing,
         * so just return it rather than an alloc/free */
        return function;
      
    }
   
}

JLink *JInterpreter::factor(bool &execute) {  

    if (l->tk=='(') {
        l->match('(');
        JLink *a = base(execute);
        l->match(')');
        return a;
    }
    
    if (l->tk==LEXER_RESERVED_TRUE) {
        l->match(LEXER_RESERVED_TRUE);
        return new JLink(new JObject(1));
    }
    
    if (l->tk==LEXER_RESERVED_FALSE) {
        l->match(LEXER_RESERVED_FALSE);
        return new JLink(new JObject(0));
    }
    
    if (l->tk==LEXER_RESERVED_NULL) {
        l->match(LEXER_RESERVED_NULL);
        return new JLink(new JObject(JAIL_BLANK_DATA,VARIABLE_NULL));
    }
    
    if (l->tk==LEXER_RESERVED_UNDEFINED) {
        l->match(LEXER_RESERVED_UNDEFINED);
        return new JLink(new JObject(JAIL_BLANK_DATA,VARIABLE_UNDEFINED));
    }
    
    if (l->tk==LEXER_IDENTIFIER) {

        //JLink *a = execute ? findInScopes(l->tkStr) : new JLink(new JObject());
        JLink *a;
        
        if(execute) {
            a = findInScopes(l->tkStr);
        } else {
            a = new JLink(new JObject());
        }

        //printf("0x%08X for %s at %s\n", (unsigned int)a, l->tkStr.c_str(), l->getPosition().c_str());
        /* The parent if we're executing a method call */
        JObject *parent = 0;

        if (execute && !a) {
          
            /* JObject doesn't exist! JavaScript says we should create it
            * (we won't add it here. This is done in the assignment operator)*/
            a = new JLink(new JObject(), l->tkStr);

        }

        //printf("\n[interpreter:factor] got an identifier '%s', execute = %d\n", l->tkStr.c_str(), execute);
        
        l->match(LEXER_IDENTIFIER);

        while (l->tk=='(' || l->tk=='.' || l->tk=='[') {

            if (l->tk=='(') { // ------------------------------------- Function Call
                a = functionCall(execute, a, parent);
            } 
            
            else if (l->tk == '.') { // ------------------------------------- Record Access
                l->match('.');
                if (execute) {
                
                    const std::string &name = l->tkStr;
                    JLink *child = a->var->findChild(name);

                    // copied symbol table, recursive
                    if (!child) 
                        child = findInParentClasses(a->var, name);
  
                    if (!child) {
                        
                        /* if we haven't found this defined yet, use the built-in
                           'length' properly */
                        if (a->var->isArray() && name == "length") {
                            int l = a->var->getArrayLength();
                            child = new JLink(new JObject(l));
                        }
                        else if (a->var->isString() && name == "length") {
                            int l = a->var->getString().size();
                            child = new JLink(new JObject(l));
                        } 
                        else if (a->var->isObject() && (name == "size" || name == "length")) {
                            JLink *v = a->var->firstChild;
                            int l = 0;
                            while(v) {
                                l++;
                                v = v->nextSibling;
                            }
                            child = new JLink(new JObject(l));                      
                        } 
                        else 
                            child = a->var->addChild(name);
                        
                    }
                    
                    parent = a->var;
                    a = child;
                    
                }
                
                l->match(LEXER_IDENTIFIER);
                
            } 
            
            
            else if (l->tk == '[') { // ------------------------------------- Array Access
                l->match('[');
                
                /*
                if(a->var->isString()){                    
                    
                    //printf("array access on string? %s[%s]", a->var->getString().c_str(), l->tkStr.c_str());                    
                    JLink *index = expression(execute);                    
                    
                    //printf("array access on string? %s[%s]", a->var->getString().c_str(), index->var->getString().c_str());                    
                    l->match(']');
                    if (execute) {                    
                        std::stringstream ss;
                        ss << (char)a->var->getString().c_str()[atoi(index->var->getString().c_str())];
                        JLink *child = new JLink(new JObject( ss.str(), VARIABLE_CHAR )); //a->var->findChildOrCreate(index->var->getString());                      
                        parent = a->var;
                        a = child;                      
                    }
                    CLEAN(index);
                    
                } else {
                
                    JLink *index = base(execute);
                    l->match(']');
                    if (execute) {
                        JLink *child = a->var->findChildOrCreate(index->var->getString());
                        parent = a->var;
                        a = child;
                    }
                    CLEAN(index);
                
                }
                */
                JLink *index = base(execute);
                l->match(']');
                if (execute) {
                    JLink *child = a->var->findChildOrCreate(index->var->getString());
                    parent = a->var;
                    a = child;
                }
                CLEAN(index);
                
            } 
            
            else 
                ASSERT(0);
                
        }
           
        return a;
    }
    
    if (l->tk==LEXER_INTEGER || l->tk==LEXER_DOUBLE) {
        JObject *a = new JObject(l->tkStr, ((l->tk==LEXER_INTEGER) ? VARIABLE_INTEGER : VARIABLE_DOUBLE));
        l->match(l->tk);
        return new JLink(a);
    }
    
    if (l->tk==LEXER_STRING) {
        JObject *a = new JObject(l->tkStr, VARIABLE_STRING);
        l->match(LEXER_STRING);
        return new JLink(a);
    }
    
    if (l->tk==LEXER_CHARACTER) {
        JObject *a = new JObject(l->tkStr, VARIABLE_CHAR);
        l->match(LEXER_CHARACTER);
        return new JLink(a);
    }
    
    if (l->tk=='{') {
    
        JObject *contents = new JObject(JAIL_BLANK_DATA, VARIABLE_OBJECT);
        
        /* JSON-style object definition */
        l->match('{');
        
        bool varIsSetPrivate = false;
        
        while (l->tk != '}') {

            std::string id = l->tkStr;
          
            varIsSetPrivate = false;
          
            // we only allow strings or IDs on the left hand side of an initialisation
            if (l->tk==LEXER_STRING) 
                l->match(LEXER_STRING);
            
            else 
                l->match(LEXER_IDENTIFIER);
            
            // keyword:private property attributes
            if(l->tk == LEXER_RESERVED_PRIVATE) {
                //printf("make var private hidden !");
                l->match(LEXER_RESERVED_PRIVATE);
                varIsSetPrivate = true;
            }
            
            l->match(':');
            
            if (execute) {
              JLink *a = base(execute);
              //if(varIsSetPrivate) a->var->hidden = true;
              contents->addChild(id, a->var);
              CLEAN(a);
            }
            
            // no need to clean here, as it will definitely be used
            if (l->tk != '}') 
              l->match(',');
          
        }

        l->match('}');
        
        if(varIsSetPrivate)
            contents->setString("private"); //contents->hidden = true;
            
        return new JLink(contents);
        
    }
    
    if (l->tk=='[') {
        JObject *contents = new JObject(JAIL_BLANK_DATA, VARIABLE_ARRAY);
        /* JSON-style array */
        l->match('[');
        int idx = 0;
        while (l->tk != ']') {
          if (execute) {
            char idx_str[16]; // big enough for 2^32
            sprintf_s(idx_str, sizeof(idx_str), "%d",idx);

            JLink *a = base(execute);
            contents->addChild(idx_str, a->var);
            CLEAN(a);
          }
          // no need to clean here, as it will definitely be used
          if (l->tk != ']') l->match(',');
          idx++;
        }
        l->match(']');
        return new JLink(contents);
    }
    
    if (l->tk==LEXER_RESERVED_FUNCTION) {
      JLink *funcVar = parseFunctionDefinition();
        if (funcVar->name != JAIL_TEMP_NAME)
          TRACE("Functions not defined at statement-level are not meant to have a name");
        return funcVar;
    }
    
    if (l->tk==LEXER_RESERVED_NEW) {

      // new -> create a new object
      l->match(LEXER_RESERVED_NEW);
      
      // get the classname
      std::string className = l->tkStr;
      
      if (execute) {

        // get the identifier before the first dot!
        JLink *objClassOrFunc = findInScopes(className);
        
        //if(!objClassOrFunc) { TRACE("%s is not a valid class name", className.c_str()); return new JLink(new JObject()); }
        
        // eat the classname
        l->match(LEXER_IDENTIFIER);
                
        /* a classnames can have dots */
        while (l->tk == '.') {
            l->match('.');            
            // try to rewrite until there are no dots left
            JLink *lastobjClassOrFunc = objClassOrFunc;
            objClassOrFunc = lastobjClassOrFunc->var->findChildOrCreate(l->tkStr);            
            if(!objClassOrFunc->var->isAnything())
                throw new JAIL::Exception("The missing class '" + l->tkStr + "' was referenced.");            
            l->match(LEXER_IDENTIFIER);            
        }

        // create an empty JLink
        JLink *objLink;

        // create a new instance of the absolute class (including dots)
        JObject *t = objClassOrFunc->var->deepCopy();
        objLink = new JLink(t);

        // add the object prototype        
        // objLink->var->addChild(JAIL_PROTOTYPE_CLASS, objectClass);
        
        // find a constructor with the same name as the class, if any 
        JLink *constructor = objLink->var->findChild(objClassOrFunc->name);
        
        
        // if not found look for '__construct'
        if(!constructor)
            constructor = objLink->var->findChild("__construct"); 
        
        // finally if we have found it, execute it
        if(constructor) {
            //objLink->var->addChild(JAIL_PROTOTYPE_CLASS, objectClass);
        
            bool exec = execute;
            functionCall(execute, constructor, t); // maybe CLEAN(...) ?? have seen no difference
            execute = exec;
        }

        // eat the () if any 
        if (l->tk == '(') {
                                   
            l->match('(');      
            
            // why exactly do we need this here again?
            // i dont know but we do
            if(constructor) {

                bool exec = execute;
                functionCall(execute, constructor, t); 
                execute = exec;
            }
                           
            l->match(')');
                        
        }
        
        return objLink;
        
      } 
      
      else {
      
        l->match(LEXER_IDENTIFIER);
        
        if (l->tk == '(') {
        
          l->match('(');
          l->match(')');
          
        }
        
      }
      
    }
    
    // Nothing we can do here... just hope it's the end...
    if(l->tk == LEXER_EOF)
        l->match(LEXER_EOF);
    else 
        throw new JAIL::Exception("Unexpected end of file! ");
    
    return 0;
    
}

JLink *JInterpreter::unary(bool &execute) {

    JLink *a;
    
    if (l->tk=='!') {
        l->match('!'); // logical not
        a = factor(execute);
        
        if (execute) {
            JObject zero(0);
            JObject *res = a->var->mathsOp(&zero, LEXER_EQUAL);
            CREATE_LINK(a, res);
        }
        
    } 
    else if (l->tk=='~') {
        l->match('~'); // bitwise not
        a = factor(execute);
        
        if (execute) {

            JObject zero(0);
            JObject *res = a->var->mathsOp(&zero, LEXER_EQUAL);
            CREATE_LINK(a, res);
        }
        
        printf("XXX");
        
    } 
    else
        a = factor(execute);
        
    return a;
    
}

JLink *JInterpreter::term(bool &execute) {

    JLink *a = unary(execute);
    
    while (l->tk=='*' || l->tk=='/' || l->tk=='%') {
    
        int op = l->tk;
        l->match(l->tk);
        JLink *b = unary(execute);
        
        if (execute) {
            JObject *res = a->var->mathsOp(b->var, op);
            CREATE_LINK(a, res);
        }
        
        CLEAN(b);
        
    }
    
    return a;
    
}

JLink *JInterpreter::expression(bool &execute) { 

    bool negate = false;
    
    if (l->tk=='-') {
        l->match('-');
        negate = true;
    }
    
    JLink *a = term(execute);
    
    /*
    bool isHidden = a->var->hidden;
    
    if(a->var->hidden) {
        printf("%s.", a->var->getString().c_str());
        a->var->setString("Ooops");
        return a;
    }
    */
    
    if (negate) {
        JObject zero(0);
        JObject *res = zero.mathsOp(a->var, '-');
        CREATE_LINK(a, res);
    }

    while (l->tk=='+' || l->tk=='-' || l->tk==LEXER_PLUSPLUS || l->tk==LEXER_MINUSMINUS ) {
    
        int op = l->tk;
        l->match(l->tk);
        
        if (op==LEXER_PLUSPLUS || op==LEXER_MINUSMINUS) {
        
            if (execute) {
                JObject one(1);
                JObject *res = a->var->mathsOp(&one, op==LEXER_PLUSPLUS ? '+' : '-');
                JLink *oldValue = new JLink(a->var);
                // in-place add/subtract
                a->replaceWith(res);
                CLEAN(a);
                a = oldValue;
            }
            
        }
        
        else {
        
            JLink *b = term(execute);
            
            if (execute) {
                // not in-place, so just replace
                JObject *res = a->var->mathsOp(b->var, op);
                CREATE_LINK(a, res);
            }
            
            CLEAN(b);
            
        }
        
    }
    
    //if(isHidden) a->var->setString("Ooops");
    
    return a;
    
}

JLink *JInterpreter::shift(bool &execute) {

    JLink *a = expression(execute);
    
    if (l->tk==LEXER_LSHIFT || l->tk==LEXER_RSHIFT || l->tk==LEXER_RSHIFTUNSIGNED) {
      
        int op = l->tk;
        l->match(op);
        JLink *b = base(execute);
        int shift = execute ? b->var->getInt() : 0;
        CLEAN(b);
        
        if (execute) {
            if (op==LEXER_LSHIFT) a->var->setInt(a->var->getInt() << shift);
            if (op==LEXER_RSHIFT) a->var->setInt(a->var->getInt() >> shift);
            if (op==LEXER_RSHIFTUNSIGNED) a->var->setInt(((unsigned int)a->var->getInt()) >> shift);
        }
      
    }
  
    return a;
  
}

JLink *JInterpreter::condition(bool &execute) {  

    JLink *a = shift(execute);
    JLink *b;
    
    while (l->tk==LEXER_EQUAL || l->tk==LEXER_NEQUAL ||
           l->tk==LEXER_TYPEEQUAL || l->tk==LEXER_NTYPEEQUAL ||
           l->tk==LEXER_LEQUAL || l->tk==LEXER_GEQUAL ||
           l->tk=='<' || l->tk=='>') {
           
        int op = l->tk;
        l->match(l->tk);
        b = shift(execute);
        
        if (execute) {
            JObject *res = a->var->mathsOp(b->var, op);
            CREATE_LINK(a,res);
        }
        
        CLEAN(b);
        
    }
    
    return a;
    
}

JLink *JInterpreter::logic(bool &execute) {
    
    JLink *a = condition(execute);
    JLink *b;
    
    while(l->tk==LEXER_ANDAND || l->tk==LEXER_OROR) {
    
        bool noexecute = false;
        int op = l->tk;
        l->match(l->tk);
        bool shortCircuit = false;
        bool boolean = false;
        
        // if we have short-circuit ops, then if we know the outcome
        // we don't bother to execute the other op. Even if not
        // we need to tell mathsOp it's an & or |
        if (op==LEXER_ANDAND) {
            op = '&';
            shortCircuit = !a->var->getBool();
            boolean = true;
        } 
        
        else if (op==LEXER_OROR) {
            op = '|';
            shortCircuit = a->var->getBool();
            boolean = true;
        }
        
        b = condition(shortCircuit ? noexecute : execute);
        
        if (execute && !shortCircuit) {
        
            if (boolean) {            
                JObject *newa = new JObject(a->var->getBool());
                JObject *newb = new JObject(b->var->getBool());
                CREATE_LINK(a, newa);
                CREATE_LINK(b, newb);
            }
            
            JObject *res = a->var->mathsOp(b->var, op);
            CREATE_LINK(a, res);
            
        }
        
        CLEAN(b);
        
    }
    
    return a;
    
}

JLink *JInterpreter::ternary(bool &execute) {
    
    JLink *lhs = logic(execute);
    
    // to allow expression like
    // var res = a < b ? true : false;
    if (l->tk=='?') {
        bool noexecute = false;
        l->match('?');
        if (!execute) {
            CLEAN(lhs);
            CLEAN(base(noexecute));
            l->match(':');
            CLEAN(base(noexecute));
        } 
        else {
            bool first = lhs->var->getBool();
            CLEAN(lhs);
            if (first) {
                lhs = base(execute);
                l->match(':');
                CLEAN(base(noexecute));
            }       
            else {
                CLEAN(base(noexecute));
                l->match(':');
                lhs = base(execute);
            }
          
        }
      
    }
  
    return lhs;
  
}

JLink *JInterpreter::base(bool &execute) {
    
    JLink *lhs = ternary(execute);
    
    if (l->tk=='=' || l->tk==LEXER_PLUSEQUAL || l->tk==LEXER_MINUSEQUAL || l->tk==LEXER_MULEQUAL || l->tk==LEXER_DIVEQUAL || l->tk == LEXER_AND || l->tk == LEXER_OR || l->tk == LEXER_XOR) {
        
        /* If we're assigning to this and we don't have a parent,
         * add it to the symbol table root as per JavaScript. */
        if (execute && !lhs->owned) {
        
            if (lhs->name.length() > 0) {            
                JLink *realLhs = root->addChildNoDup(lhs->name, lhs->var);
                CLEAN(lhs);
                lhs = realLhs;               
            } 
            else 
                TRACE("Trying to assign to an un-named type");
            
        }

        int op = l->tk;
        l->match(l->tk);
        JLink *rhs = base(execute);
        
        if (execute) {
        
            if (op=='=') {            
                lhs->replaceWith(rhs);               
            }                        
            else if (op==LEXER_PLUSEQUAL) {                            
                JObject *res = lhs->var->mathsOp(rhs->var, '+');
                lhs->replaceWith(res);               
            }            
            else if (op==LEXER_MINUSEQUAL) {
                JObject *res = lhs->var->mathsOp(rhs->var, '-');
                lhs->replaceWith(res);
            }             
            else if (op==LEXER_MULEQUAL) {
                JObject *res = lhs->var->mathsOp(rhs->var, '*');
                lhs->replaceWith(res);
            }             
            else if (op==LEXER_DIVEQUAL) {
                JObject *res = lhs->var->mathsOp(rhs->var, '/');
                lhs->replaceWith(res);
            }            
            else if (op==LEXER_AND) {
                JObject *res = lhs->var->mathsOp(rhs->var, '&');
                lhs->replaceWith(res);
            }             
            else if (op==LEXER_OR) {
                JObject *res = lhs->var->mathsOp(rhs->var, '|');
                lhs->replaceWith(res);
            }             
            else if (op==LEXER_XOR) {
                JObject *res = lhs->var->mathsOp(rhs->var, '^');
                lhs->replaceWith(res);
            }             
            else ASSERT(0);
            
        }
        
        CLEAN(rhs);
        
    }
     
    return lhs;
    
}

void JInterpreter::block(bool &execute) {

    l->match('{');
    
    if (execute) {
    
        while(l->tk && l->tk != '}')
            statement(execute);
            
        l->match('}');
        
    } 
    
    else {
      
        // fast skip of blocks
        int brackets = 1;
        while (l->tk && brackets) {
            if (l->tk == '{') 
                brackets++;
            if (l->tk == '}') 
                brackets--;
            l->match(l->tk);
        }
      
    }

}

/// <summary>
///  Called first after execute
/// </summary>
/// <param name="execute"></param>
void JInterpreter::statement(bool &execute) {
    
    if(stepping)
        printf("%d", l->tk);
        
    if (l->tk==LEXER_IDENTIFIER || l->tk==LEXER_INTEGER || l->tk==LEXER_DOUBLE || l->tk==LEXER_STRING || l->tk==LEXER_CHARACTER || l->tk=='-') {
        
        /* Execute a simple statement, basic arithmetic */
        CLEAN(base(execute));        
        l->match(';');
    
    } 
    
    // object
    else if (l->tk=='{') {
    
        /* A block of code */
        block(execute);
        
    } 
    
    else if (l->tk==';') {
        /* Empty statement, to allow things like ;;; */
        l->match(';');
    } 
    
    else if (l->tk==LEXER_RESERVED_VAR) {

        l->match(LEXER_RESERVED_VAR);

        while (l->tk != ';') {
        
          JLink *a = 0;
          
          if (execute)
            a = scopes.back()->findChildOrCreate(l->tkStr);
            
          l->match(LEXER_IDENTIFIER);
          
          // now do stuff defined with dots
          while (l->tk == '.') {
              l->match('.');
              if (execute) {
                  JLink *lastA = a;
                  a = lastA->var->findChildOrCreate(l->tkStr);
              }
              l->match(LEXER_IDENTIFIER);
          }
          
          // sort out initialiser
          if (l->tk == '=') {
              l->match('=');
              JLink *var = base(execute);
              if (execute)
                  a->replaceWith(var);
              CLEAN(var);
          }
          
          if (l->tk != ';')
            l->match(',');
            
        } 
              
        l->match(';');
        
    } 
    
    else if (l->tk==LEXER_RESERVED_CLASS) {
        
        l->match(LEXER_RESERVED_CLASS);

        // at he end of a class definition (key : value) is always a ';'
        // between members is just a comma ','
        while (l->tk != ';') {

            // a is the key
            JLink *a = 0;
  
            if (execute) {
            
                a = scopes.back()->findChildOrCreate(l->tkStr);
                
            }

            l->match(LEXER_IDENTIFIER);

            // now do stuff defined with dots to get the key
            // todo
            /*
            while (l->tk == '.') {                
                l->match('.');                
                if (execute) {
                    JLink *lastA = a;
                    a = lastA->var->findChildOrCreate(l->tkStr);
                }                
                l->match(LEXER_IDENTIFIER);                
            }
            */

            // sort out initialiser
            JLink *var = base(execute);

            if (execute)
                a->replaceWith(var);

            CLEAN(var);
          
            // ',' after each statement, ';' means class definition is over
            if (l->tk == ',')
                l->match(',');
             
        }

    } 
    
    else if (l->tk==LEXER_RESERVED_IF) {
    
        l->match(LEXER_RESERVED_IF);
        l->match('(');
        JLink *var = base(execute);
        l->match(')');
        bool cond = execute && var->var->getBool();
        CLEAN(var);
        bool noexecute = false; // because we need to be abl;e to write to it
        statement(cond ? execute : noexecute);
        if (l->tk==LEXER_RESERVED_ELSE) {
            l->match(LEXER_RESERVED_ELSE);
            statement(cond ? noexecute : execute);
        }
        
    } 
    
    else if (l->tk==LEXER_RESERVED_WHILE) {
        
        // We do repetition by pulling out the string representing our statement
        // there's definitely some opportunity for optimisation here
        l->match(LEXER_RESERVED_WHILE);
        
        
        l->match('(');
        
        int whileCondStart = l->tokenStart;        
        bool noexecute = false;
        
        JLink *cond = base(execute);
        
        bool loopCond = execute && cond->var->getBool();
        
        CLEAN(cond);
        
        JLexer *whileCond = l->getSubLex(whileCondStart);
        
        l->match(')');
        
        
        int whileBodyStart = l->tokenStart;
        
        statement(loopCond ? execute : noexecute);
        
        JLexer *whileBody = l->getSubLex(whileBodyStart);
        
        JLexer *oldLex = l;
        
        int loopCount = JAIL_LOOP_MAX_ITERATIONS; 
        while (loopCond /*&& loopCount-- > 0*/) {
            whileCond->reset();
            l = whileCond;
            cond = base(execute);
            loopCond = execute && cond->var->getBool();
            CLEAN(cond);
            if (loopCond) { 
                whileBody->reset();
                l = whileBody;
                statement(execute);  
            }   
        }    
        /**
         * break could have set 'execute' to false..
         */
        if(!execute) 
            execute = true;
        l = oldLex;
        delete whileCond;
        delete whileBody;

        if (loopCount<=0) {
        
            root->trace();
            TRACE("WHILE Loop exceeded %d iterations at %s\n", JAIL_LOOP_MAX_ITERATIONS, l->getPosition().c_str());
            throw new Exception("LOOP_ERROR");
            
        }
        
    } 
    
    else if (l->tk==LEXER_RESERVED_FOR) {
    
        l->match(LEXER_RESERVED_FOR);
        l->match('(');
        statement(execute); // initialisation
        //l->match(';');
        int forCondStart = l->tokenStart;
        bool noexecute = false;
        JLink *cond = base(execute); // condition
        bool loopCond = execute && cond->var->getBool();
        CLEAN(cond);
        JLexer *forCond = l->getSubLex(forCondStart);
        l->match(';');
        int forIterStart = l->tokenStart;
        CLEAN(base(noexecute)); // iterator
        JLexer *forIter = l->getSubLex(forIterStart);
        l->match(')');
        int forBodyStart = l->tokenStart;
        statement(loopCond ? execute : noexecute);
        JLexer *forBody = l->getSubLex(forBodyStart);
        JLexer *oldLex = l;
        if (loopCond) {
            forIter->reset();
            l = forIter;
            CLEAN(base(execute));
        }
        int loopCount = JAIL_LOOP_MAX_ITERATIONS;
        while (execute && loopCond /*&& loopCount-->0*/ ) {
            forCond->reset();
            l = forCond;
            cond = base(execute);
            loopCond = cond->var->getBool();
            CLEAN(cond);
            if (execute && loopCond) {
                forBody->reset();
                l = forBody;
                statement(execute);
            }
            if (execute && loopCond) {
                forIter->reset();
                l = forIter;
                CLEAN(base(execute));
            }
        }
        l = oldLex;
        delete forCond;
        delete forIter;
        delete forBody;
        if (loopCount <= 0 && 1==2) {
            root->trace();
            TRACE("FOR Loop exceeded %d iterations at %s\n", JAIL_LOOP_MAX_ITERATIONS, l->getPosition().c_str());
            throw new Exception("LOOP_ERROR");
        }
    } 
    
    else if (l->tk==LEXER_RESERVED_BREAK) {
        //JLink *result = 0;
        l->match(LEXER_RESERVED_BREAK);
        l->match(';');
        //CLEAN(result);
        //bool noexecute = true;
        execute = false;
        
    }
    
    else if (l->tk==LEXER_RESERVED_RETURN) {
        l->match(LEXER_RESERVED_RETURN);
        JLink *result = 0;
        if (l->tk != ';')
          result = base(execute);
        if (execute) {
          JLink *resultVar = scopes.back()->findChild(JAIL_RETURN_VAR);
          if (resultVar)
            resultVar->replaceWith(result);
          else
            TRACE("RETURN statement, but not in a function.\n");
          execute = false;
        }
        CLEAN(result);
        l->match(';');
    } 
    
    else if (l->tk==LEXER_RESERVED_FUNCTION) {
        JLink *funcVar = parseFunctionDefinition();
        if (execute) {
          if (funcVar->name == JAIL_TEMP_NAME)
            TRACE("Functions defined at statement-level are meant to have a name\n");
          else
            scopes.back()->addChildNoDup(funcVar->name, funcVar->var);
        }
        CLEAN(funcVar);
    } 
    
    else l->match(LEXER_EOF);

}

/// Get the given JObject specified by a path (var1.var2.etc), or return 0
JObject *JInterpreter::getScriptVariable(const std::string &path) const {
    // traverse path
    size_t prevIdx = 0;
    size_t thisIdx = path.find('.');
    if (thisIdx == std::string::npos) thisIdx = path.length();
    JObject *var = root;
    while (var && prevIdx<path.length()) {
        std::string el = path.substr(prevIdx, thisIdx-prevIdx);
        JLink *varl = var->findChild(el);
        var = varl?varl->var:0;
        prevIdx = thisIdx+1;
        thisIdx = path.find('.', prevIdx);
        if (thisIdx == std::string::npos) thisIdx = path.length();
    }
    return var;
}

/// get the value of the given JObject, return true if it exists and fetched
bool JInterpreter::getVariable(const std::string &path, std::string &varData) const {
    JObject *var = getScriptVariable(path);
    // return result
    if (var) {
        varData = var->getString();
        return true;
    } else
        return false;
}

/// set the value of the given JObject, return true if it exists and gets set
bool JInterpreter::setVariable(const std::string &path, const std::string &varData) {
    JObject *var = getScriptVariable(path);
    // return result
    if (var) {
        if (var->isInt())
            var->setInt((int)strtol(varData.c_str(),0,0));
        else if (var->isDouble())
            var->setDouble(strtod(varData.c_str(),0));
        else
            var->setString(varData.c_str());
        return true;
    }    
    else
        return false;
}

/// Finds a child, looking recursively up the scopes
JLink *JInterpreter::findInScopes(const std::string &childName) const {
    for (int s=scopes.size()-1;s>=0;s--) {
      JLink *v = scopes[s]->findChild(childName);
      if (v) return v;
    }
    return NULL;

}

/// Look up in any parent classes of the given object
JLink *JInterpreter::findInParentClasses(JObject *object, const std::string &name) const {

    int d = 0;
    
    // Look for links to actual parent classes
    JLink *parentClass = object->findChild(JAIL_PROTOTYPE_CLASS);
    
    while (parentClass) {
        if(d) printf("NO FAKE! ");
        JLink *implementation = parentClass->var->findChild(name);
        if (implementation) 
            return implementation;
        parentClass = parentClass->var->findChild(JAIL_PROTOTYPE_CLASS);
    }
    
    // else fake it for strings
    if (object->isInt()) {
        if(d) printf("FAKE INTEGER! ");
        JLink *implementation = intClass->findChild(name);
        if (implementation) 
            return implementation;
    }
    
    // else fake it for strings
    if (object->isString()) {
        if(d) printf("FAKE STRING! ");
        JLink *implementation = stringClass->findChild(name);
        if (implementation) 
            return implementation;
    }
    
    if (object->isChar()) {
        if(d) printf("FAKE CHAR! ");
        JLink *implementation = charClass->findChild(name);
        if (implementation) 
            return implementation;
    }
    
    // arrays
    if (object->isArray()) {
        if(d )printf("FAKE ARRAY! ");
        JLink *implementation = arrayClass->findChild(name);
        if (implementation) 
            return implementation;
    }
    
    // todo forgot double class..
    
    // and finally objects
    if(d) printf("FAKE OBJECT! ");
    JLink *implementation = objectClass->findChild(name);
    if (implementation) 
        return implementation;

    return 0;
    
}

}; // namespace JAIL
