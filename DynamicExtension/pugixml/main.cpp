#include "pugixml.hpp"
#include <iostream>
#include "Jail.h"
#include <Windows.h>
#include <vector>

std::vector<pugi::xml_document*> openDocs;

extern "C" {
	
	__declspec(dllexport) void openXML(JAIL::JObject *c, void *data) {
		
		std::string src = c->getParameter("src")->getString();
		pugi::xml_document* doc = new pugi::xml_document();
		pugi::xml_parse_result result = doc->load_file(src.c_str());
		if (!result) {
			delete doc; 
			return;
		}
		openDocs.push_back(doc);
		int newIndex = openDocs.size() - 1;
        c->getReturnVar()->setInt(newIndex); 
	
	}
	
	__declspec(dllexport) void readXML(JAIL::JObject *c, void *data) {
		
		int index = c->getParameter("doc")->getInt();
		std::string elem = c->getParameter("elem")->getString();  	
		if (index < 0 || index >= openDocs.size()) {
            return;
        }
        pugi::xml_document& doc = *openDocs[index];  
        
		//pugi::xml_node nodes = doc.child("articles").children("article");
		pugi::xpath_node_set nodes = doc.select_nodes(elem.c_str());
		
		if(NULL == nodes)
			return;
		
		JAIL::JObject *arr = new JAIL::JObject();
		arr->setArray();
        int arrayIndex = 0;
		
        for (pugi::xpath_node node : nodes) {
            
			pugi::xml_node element = node.node();  
			JAIL::JObject *obj = new JAIL::JObject();
			
			obj->addChild("content", new JAIL::JObject(element.child_value()));
			for (pugi::xml_attribute attr : element.attributes()) {
				obj->addChild(attr.name(), new JAIL::JObject(attr.value()));
			}
			arr->setArrayIndex(arrayIndex++, obj);
			
        }
		
		c->setReturnVar(arr);

	}
	
	__declspec(dllexport) void writeXML(JAIL::JObject *c, void *data) {
		
		int index = c->getParameter("doc")->getInt();
		std::string dst = c->getParameter("dst")->getString();
		
		pugi::xml_document& doc = *openDocs[index];
		
		if (!doc.save_file( dst.c_str() )) {
			std::cerr << "Fehler beim Speichern der Datei!" << std::endl;
			return;
		}
		
		/*
		pugi::xml_document doc;
		
		// Root-Element erstellen
		pugi::xml_node articles = doc.append_child("articles");

		// Unterknoten hinzufügen
		pugi::xml_node article1 = articles.append_child("article");
		article1.append_attribute("id") = 1;           
		article1.append_child(pugi::node_pcdata).set_value("Hello"); 

		pugi::xml_node article2 = articles.append_child("article");
		article2.append_attribute("id") = 2;
		article2.append_child(pugi::node_pcdata).set_value("World");

		// XML in eine Datei schreiben
		if (!doc.save_file( dst.c_str() )) {
			std::cerr << "Fehler beim Speichern der Datei!" << std::endl;
			return;
		}

		std::cout << "XML erfolgreich gespeichert!" << std::endl;
		*/
		
	}
	
	__declspec(dllexport) void closeXML(JAIL::JObject *c, void *data) {
		
		int index = c->getParameter("doc")->getInt();	
		pugi::xml_document* doc = openDocs[index];
		delete doc;  // remove doc
        openDocs.erase(openDocs.begin() + index); // remove ptr
		
	}
	
	__declspec(dllexport) void registerLib(JAIL::JInterpreter *interpreter) {
		
		interpreter->addNative("function XML.open(src)", openXML, 0);
		interpreter->addNative("function XML.read(doc, elem)", readXML, 0);
		interpreter->addNative("function XML.write(doc, dst)", writeXML, 0);
		interpreter->addNative("function XML.close(doc)", closeXML, 0);
		
	}

}