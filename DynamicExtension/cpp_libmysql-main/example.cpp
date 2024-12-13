#include "mysql_connector.h"
#include "Jail.h"
#include <iostream>
#include <cstdio>
#include <sstream>
#include <vector>

using namespace std;

std::string addSlashes(const std::string &input) {
    std::ostringstream result;
    for (char ch : input) {
        if (ch == '\\') {
            result << "\\\\";
        } else {
            result << ch;
        }
    }
    return result.str();
}

std::vector<std::shared_ptr<mysql_connector>> conns;

extern "C" {
	
	__declspec(dllexport) void createConnection(JAIL::JObject *c, void *data) {
		
		std::string host = c->getParameter("host")->getString();
		std::string username = c->getParameter("username")->getString();
		std::string password = c->getParameter("password")->getString();
		std::string database = c->getParameter("database")->getString();

		auto mc = std::make_shared<mysql_connector>();
		if (mc->connect_to(host, username, password, database)) {
			
			std::string err = "Connection error: " + mc->error();
			c->getReturnVar()->setString(err);
			return;
			
		}

		conns.push_back(mc);

		size_t index = conns.size() - 1;
		c->getReturnVar()->setInt(index);

	}
	
	__declspec(dllexport) void my_use_mysql_connector(JAIL::JObject *c, void *data) {
		
		int idx = c->getParameter("conn")->getInt();
		std::string query = c->getParameter("query")->getString();
		std::string result = "";
		
		query = addSlashes(query);
		
		JAIL::JObject *ret = new JAIL::JObject();
		ret->setArray();
		
		auto& mc = *conns[idx];

		auto res = mc.query(query);

		if (res.second) {
			cerr << "query error : " << mc.error() << endl;
			std::string err = "query error : " + mc.error();
			c->getReturnVar()->setString(err);
			exit(1);
		}
		
		int arrIdx = 0;
		
		for (auto& row : res.first) {
			
			JAIL::JObject *resultRow = new JAIL::JObject();
			resultRow->setArray();
			int i = 0;
			for (auto& col : row) {
				//result += col + " ";
				std::string t = col;
				resultRow->setArrayIndex(i++, new JAIL::JObject(t));
			}
			ret->setArrayIndex(arrIdx++, resultRow);
			
		}
		
		c->setReturnVar(ret);
		
	}
	
	__declspec(dllexport) void addSlashesToString(JAIL::JObject *c, void *data) {
		
		std::string src = c->getParameter("str")->getString();
		src = addSlashes(src);
		c->getReturnVar()->setString(src);
		
	}
	
	__declspec(dllexport) void closeConnection(JAIL::JObject *c, void *data) {
		
		int idx = c->getParameter("conn")->getInt();
		
		if (idx < 0 || idx >= conns.size()) {
			c->getReturnVar()->setString("Invalid connection index.");
			return;
		}
		
		auto& mc = *conns[idx];
		mc.close(); 
		conns.erase(conns.begin() + idx);
		
		c->getReturnVar()->setString("Connection closed.");
		
	}

	__declspec(dllexport) void registerLib(JAIL::JInterpreter *interpreter) {
        
        interpreter->addNative("function MySQL.connect(host, username, password, database)", createConnection, 0);
		interpreter->addNative("function MySQL.query(conn, query)", my_use_mysql_connector, 0);
		interpreter->addNative("function MySQL.addSlashes(str)", addSlashesToString, 0);
		interpreter->addNative("function MySQL.close(conn)", closeConnection, 0);

    }

}