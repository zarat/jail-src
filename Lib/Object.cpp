#include "Jail.h"

namespace JAIL {

    // like reflection
    void scObjectDump(JObject *c, void *) {
        c->getParameter("this")->trace("");
    }
    
    void scObjectClone(JObject *c, void *) {
        JObject *obj = c->getParameter("this");
        c->getReturnVar()->copyValue(obj);
    }
    
    // count no of elements in an object
    void scObjectCount(JObject *c, void *) {
        JLink *v;
        v = c->getParameter("this")->firstChild;
        int i = 0;
        while(v) {
            v = v->nextSibling;
            i++;
        }
        c->getReturnVar()->setInt(i);
    }
    
    void scObjectMemberAt(JObject *c, void *userdata) {    
        JLink *v = c->getParameter("this")->firstChild;    
        int memberAt = c->getParameter("pos")->getInt();    
        int i = 0, j = 0;
        while ( v ) {                
            if(i == memberAt) {
                c->setReturnVar(v->var);
                // create a new object containing "name" and "value"
                //o->addChild("value", v->var);
                //o->addChild("name", new JObject( v->getName().c_str() ) );
                break;
            }        
            v = v->nextSibling;            
            i++;         
        } 
    }
    
    // remove member at position
    void scRemoveMemberAt(JObject *c, void *userdata) {    
        
        JLink *v = c->getParameter("this")->firstChild;            
        int memberAt = c->getParameter("pos")->getInt();            
        int i = 0;

        while ( v ) {                
            if(i == memberAt) {
                // print name of variable
                //printf("-> %s <-", v->getName().c_str());
                c->getParameter("this")->removeChild(v->var);
                break;  
            }                 
            v = v->nextSibling;            
            i++;         
        }
  
    }
    
    void registerObject(JInterpreter *interpreter) {
    
        // interpreter->addNative("function Object.dump()", scObjectDump, 0);
        interpreter->addNative("function Object.clone()", scObjectClone, 0); // deprecated
        interpreter->addNative("function Object.count()", scObjectCount, 0);
        interpreter->addNative("function Object.memberAt(pos)", scObjectMemberAt, 0);
        interpreter->addNative("function Object.removeAt(pos)", scRemoveMemberAt, 0);
    
    }

}
