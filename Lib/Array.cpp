/*

Function array.each() has a bug, everything is a string..

*/

#include "Jail.h"
#include <sstream>

namespace JAIL {

void scArrayContains(JObject *c, void *data) {
  JObject *obj = c->getParameter("obj"); // the obj to search for
  JLink *v = c->getParameter("this")->firstChild; // the array
  bool contains = false;
  while (v) {
      if (v->var->equals(obj)) {
        contains = true;
        break;
      }
      v = v->nextSibling;
  }
  c->getReturnVar()->setInt(contains);
}

// returns a new array
void scArrayRemove(JObject *c, void *data) {

  JObject *self = c->getParameter("this");
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
    
}

void scArrayJoin(JObject *c, void *data) {
  std::string sep = c->getParameter("sep")->getString();
  JObject *arr = c->getParameter("this");
  std::ostringstream sstr;
  int l = arr->getArrayLength();
  for (int i=0;i<l;i++) {
    if (i>0) sstr << sep;
    sstr << arr->getArrayIndex(i)->getString();
  }
  c->getReturnVar()->setString(sstr.str());
}

void scArrayCount(JObject *c, void *data) {
    JObject *arr = c->getParameter("this");
    int l = arr->getArrayLength();
    c->getReturnVar()->setInt(l);
}

// execute a function for each element in the array
// by default the passed element is 'e' if you pass a parameter to the callback it is named like that
// @deprecated
void scArrayEach(JObject *c, void *data) {    
    
    JObject *arr = c->getParameter("this");
    std::string func = c->getParameter("func")->getString();
    
    std::string paramname;
    if(c->getParameter("func")->getChildren() > 0)
        paramname = c->getParameter("func")->firstChild->name.c_str();
    else
        paramname = "e";
                
    int l = arr->getArrayLength();    
    for (int i = 0; i < l; i++) { 
           
        JInterpreter *interpreter = reinterpret_cast<JInterpreter *>(data);
              
        std::ostringstream oss;
        arr->getArrayIndex(i)->getJSON(oss);
        std::string t = oss.str();

        std::stringstream ss;        
        ss << "var " << paramname << " = " << t << "; " << func ;        
        interpreter->execute(ss.str());

        arr->setArrayIndex(i, interpreter->getScriptVariable(paramname)); 
        
    }
    
    c->setReturnVar(arr);
        
}

// todo
// shift first out, returns new array
void scArrayShift(JObject *c, void *data) {        
    JObject *self = c->getParameter("this");
    JObject *obj = c->getParameter("obj");  
    JObject *result = new JObject();
    result->setArray();  
    JLink *v = self->firstChild;
    JObject *t = self->firstChild->var->deepCopy();
    int i = 0, j = 0;
    while (v) {  
        if (j > 0) {        
          result->setArrayIndex(i, v->var);        
          i++;
        }
        j++;
        v = v->nextSibling;
    }
    i = 0;
    v = result->firstChild;        
    while(v) {
        self->setArrayIndex(i++, v->var);
        v = v->nextSibling;
    }
    self->removeLink(self->lastChild);    
    c->setReturnVar(t);
         
}

// todo
// modifies the original and returns a new array
void scArrayReverse(JObject *c, void *data) {
    
    JObject *obj = c->getParameter("this");
    
    // create temporary array    
    JObject *result = new JObject();
    result->setArray();    
    int i = obj->getArrayLength()-1;
    JLink *link = obj->firstChild;        
    while(link) {
        result->setArrayIndex(i--, link->var);
        link = link->nextSibling;
    }
    
    /*
    // copy temporary array back to this  
    i = result->getArrayLength()-1;
    link = result->firstChild;        
    while(link) {
        obj->setArrayIndex(i--, link->var);
        link = link->nextSibling;
    }
    */
    
    c->setReturnVar(result);
        
}

// modifies the original and returns a new array
void scArrayConcat(JObject *c, void *data) {
    
    JObject *arr1 = c->getParameter("this");
    JObject *arr2 = c->getParameter("arr");
    
    int len1 = arr1->getArrayLength()-1;
    int len2 = arr2->getArrayLength()-1;
    
    JObject *newArr = new JObject();
    newArr->setArray();
    int newLen = len1 + len2;
    int newLenCounter = 0;
    
    JLink *v = arr1->firstChild;
    while(v) {
        newArr->setArrayIndex(newLenCounter++, v->var);
        v = v->nextSibling;
    }
    
    v = arr2->firstChild;
    while(v) {
        newArr->setArrayIndex(newLenCounter++, v->var);
        v = v->nextSibling;
    }
    
    c->setReturnVar(newArr);
    
    /*
    JObject *arr1 = c->getParameter("this");
    JObject *arr2 = c->getParameter("arr");

    int len = arr1->getArrayLength()-1;
    JLink *v = arr2->firstChild;
    while(v) {
        arr1->setArrayIndex(++len, v->var);
        v = v->nextSibling;
    }
    
    c->setReturnVar(arr1);
    */

}

// modifies the original and returns a modified copy
void scArraySort(JObject *c, void *data) {

	JObject *arr = c->getParameter("this");
	std::stringstream ss;
	ss << "var inputArr = [";
	int len = arr->getArrayLength();
	int i = 0;
	JLink *v = arr->firstChild;
	while (v) {
		if (v->var->isNumeric()) 
			ss << v->var->getString();
		else 
			ss << "\"" << v->var->getString() << "\"";
		if (i < len - 1)
			ss << ", ";
		i++;
		v = v->nextSibling;
	}
	ss << "]; var len = inputArr.length - 1; for (var i = 0; i < len; i++) { for (var j = 0; j < len; j++) { if (inputArr[j] > inputArr[j + 1]) { var tmp = inputArr[j]; inputArr[j] = inputArr[j + 1]; inputArr[j + 1] = tmp; } } }";
	JInterpreter *interpreter = reinterpret_cast<JInterpreter *>(data);
	interpreter->execute(ss.str());
	JObject *res = interpreter->getScriptVariable("inputArr");	
	
    /*
    v = res->firstChild;
	i = 0;
	while (v) {
		arr->setArrayIndex(i, v->var);
		i++;
		v = v->nextSibling;
	}
    */
	
    c->setReturnVar(res);

}

// todo
void scArrayUnique(JObject *c, void *data) {

    JObject *arr = c->getParameter("this");
    
    JObject *uniques = new JObject();
    uniques->setArray();
    int uniquesIndex = 0;
    
    JObject *newArr = new JObject();
    newArr->setArray();
    int newArrIndex = 0;
    
    bool isDuplicate = false;
    
    // cheack each array index
    JLink *v = arr->firstChild;
	while (v) {
    
        isDuplicate = false;
        
        // is it already in uniques?
        JLink *w = uniques->firstChild;
        if(w) {
        
            while(w) {
            
                // if not, add it
                if(w->var != v->var) {
                    
                    isDuplicate = true;
                    
                }
                
                w = w->nextSibling;
                
            }
         
            if(!isDuplicate) {
                uniques->setArrayIndex(uniquesIndex++, v->var);
                isDuplicate = false;
            }
            
        } 
        else {
            uniques->setArrayIndex(uniquesIndex++, v->var);
        }
        
        v = v->nextSibling;
    }
    
    c->setReturnVar(uniques);
    
    /*
	JObject *arr = c->getParameter("this");
	std::stringstream ss;
	ss << "var inputArr = [";
	int len = arr->getArrayLength();
	int i = 0;
	JLink *v = arr->firstChild;
	while (v) {
		if (v->var->isNumeric())
			ss << v->var->getString();
		else
			ss << "\"" << v->var->getString() << "\"";
		if (i < len - 1)
			ss << ", ";
		i++;
		v = v->nextSibling;
	}
	ss << "]; var b = []; var i = 0; var j = 0; while(i < inputArr.length) { if (!b.contains(inputArr[i])) { b[j++] = inputArr[i]; } i++; }";
	JInterpreter *interpreter = reinterpret_cast<JInterpreter *>(data);
	interpreter->execute(ss.str());
	JObject *res = interpreter->getScriptVariable("b");
	v = res->firstChild;
	i = 0;
	while (v) {
		arr->setArrayIndex(i, v->var);
		i++;
		v = v->nextSibling;
	}
	c->setReturnVar(res);
    */

}

void scArrayPush(JObject *c, void *data) {

    JObject *arr = c->getParameter("this");    
    JObject *newObj = c->getParameter("obj"); 
    
    JObject *newArr = new JObject();
    newArr->setArray();
    
    JLink *v = arr->firstChild;
    
    int i = 0;
    newArr->setArrayIndex(i++, newObj);
    
    while(v) {    
        newArr->setArrayIndex(i++, v->var);
        v = v->nextSibling;
    }    
    i = 0;
    v = newArr->firstChild;
    while(v) {
        arr->setArrayIndex(i++, v->var);
        v = v->nextSibling;
    }
   
    c->setReturnVar(arr);

}

void scArrayPop(JObject *c, void *data) {

    JObject *arr = c->getParameter("this");   
    
    // create a temporary array
    JObject *newArr = new JObject();
    newArr->setArray();
    int newArrIndex = 0;
    
    // populate the temporary array with the new array, except the first index
    JLink *v = arr->firstChild; 
    // keep value of popped property   
    JLink *tmp = v;
    v = v->nextSibling;
    while(v) { 
    
        newArr->setArrayIndex(newArrIndex++, v->var);     
        v = v->nextSibling;
        
    }
    
    // clear the original array
    arr->removeAllChildren();
    
    // add all items of newArr back to arr
    v = newArr->firstChild;
    int arrIndex = 0;
    while(v) {
        arr->setArrayIndex(arrIndex++, v->var);     
        v = v->nextSibling;
    }
    
    c->setReturnVar(arr);
    
}

void scArraySlice(JObject *c, void *data) {

    JObject *arr = c->getParameter("this");
    JObject *newArr = new JObject();
    newArr->setArray();
    
    int from = c->getParameter("from")->getInt();
    int to = c->getParameter("to")->getInt();    
    int i = 0, j = 0;
         
    JLink *v = arr->firstChild;
    while(v) {
        if(i >= from && i < to)
            newArr->setArrayIndex(j++, v->var);
        i++;
        v = v->nextSibling;
    }
       
    c->setReturnVar(newArr);

}
    
    void registerArray(JInterpreter *interpreter) {
    
        interpreter->addNative("function Array.count()", scArrayCount, 0); // deprecated
        interpreter->addNative("function Array.contains(obj)", scArrayContains, 0);
        
        interpreter->addNative("function Array.remove(obj)", scArrayRemove, 0);
        interpreter->addNative("function Array.join(sep)", scArrayJoin, 0);
        interpreter->addNative("function Array.concat(arr)", scArrayConcat, 0); // todo: it modifies original
        interpreter->addNative("function Array.each(func)", scArrayEach, interpreter);
        interpreter->addNative("function Array.reverse()", scArrayReverse, 0);
        interpreter->addNative("function Array.shift()", scArrayShift, 0);
        interpreter->addNative("function Array.sort()", scArraySort, interpreter);
		interpreter->addNative("function Array.unique()", scArrayUnique, interpreter);
        interpreter->addNative("function Array.push(obj)", scArrayPush, 0);
        interpreter->addNative("function Array.pop()", scArrayPop, 0);
        interpreter->addNative("function Array.slice(from, to)", scArraySlice, 0);

    }

}
