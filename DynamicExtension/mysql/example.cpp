/*

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
*/


#include "mysql_connector.h"
#include "Jail.h"
#include <iostream>
#include <cstdio>
#include <sstream>
#include <vector>
#include <memory>
#include <unordered_map>

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
std::unordered_map<int, std::vector<std::vector<std::string>>> queryResults;
std::unordered_map<int, MYSQL_RES*> activeResults;


extern "C" {

    __declspec(dllexport) void createConnection(JAIL::JObject *c, void *data) {
        std::string host = c->getParameter("host")->getString();
        std::string username = c->getParameter("username")->getString();
        std::string password = c->getParameter("password")->getString();
        std::string database = c->getParameter("database")->getString();
		
		JAIL::JObject *arr = new JAIL::JObject();
		arr->setArray();
        
		auto mc = std::make_shared<mysql_connector>();
        if (mc->connect_to(host, username, password, database)) {
            std::string err = "Connection error: " + mc->error();
			arr->setArrayIndex(0, new JAIL::JObject(-1));
			arr->setArrayIndex(1, new JAIL::JObject(err));
            c->setReturnVar(arr);
            return;
        }

        conns.push_back(mc);

        size_t index = conns.size() - 1;
        c->getReturnVar()->setInt(index);
		
		//arr->setArrayIndex(0, new JAIL::JObject((int)index));
		//c->setReturnVar(arr);
			
    }

    __declspec(dllexport) void my_use_mysql_connector(JAIL::JObject *c, void *data) {
        int idx = c->getParameter("conn")->getInt();
        std::string query = c->getParameter("query")->getString();

        query = addSlashes(query);

        if (idx < 0 || idx >= conns.size()) {
            c->getReturnVar()->setString("Invalid connection index.");
            return;
        }

        auto& mc = *conns[idx];
        MYSQL_RES* result = mc.query_store(query);

        if (!result) {
            //cerr << "Query error: " << mc.error() << endl;
            c->getReturnVar()->setInt(0);
            return;
        }

        // Store the active result
        activeResults[idx] = result;

        c->getReturnVar()->setInt(static_cast<int>(mysql_num_rows(result)));
    }

    __declspec(dllexport) void fetchArray(JAIL::JObject *c, void *data) {
        int idx = c->getParameter("conn")->getInt();

        if (idx < 0 || idx >= conns.size()) {
            c->getReturnVar()->setString("Invalid connection index.");
            return;
        }

        if (activeResults.find(idx) == activeResults.end() || !activeResults[idx]) {
            c->getReturnVar()->setString("No more rows.");
            return;
        }

        MYSQL_RES* result = activeResults[idx];
        MYSQL_ROW row = mysql_fetch_row(result);

        if (!row) {
            mysql_free_result(result);
            activeResults.erase(idx);
            c->getReturnVar()->setString("No more rows.");
            return;
        }

        JAIL::JObject *resultRow = new JAIL::JObject();
        resultRow->setArray();

        unsigned int num_fields = mysql_num_fields(result);
        for (unsigned int i = 0; i < num_fields; ++i) {
            std::string col = row[i] ? row[i] : "NULL";
            resultRow->setArrayIndex(i, new JAIL::JObject(col));
        }

        c->setReturnVar(resultRow);
    }
	
	__declspec(dllexport) void fetchArrayAssoc(JAIL::JObject *c, void *data) {
		int idx = c->getParameter("conn")->getInt();

		if (idx < 0 || idx >= conns.size()) {
			c->getReturnVar()->setString("Invalid connection index.");
			return;
		}

		if (activeResults.find(idx) == activeResults.end() || !activeResults[idx]) {
			c->getReturnVar()->setString("No more rows.");
			return;
		}

		MYSQL_RES* result = activeResults[idx];
		MYSQL_ROW row = mysql_fetch_row(result);

		if (!row) {
			mysql_free_result(result);
			activeResults.erase(idx);
			c->getReturnVar()->setString("No more rows.");
			return;
		}

		MYSQL_FIELD* fields = mysql_fetch_fields(result);
		unsigned int num_fields = mysql_num_fields(result);

		JAIL::JObject* resultRow = new JAIL::JObject();

		for (unsigned int i = 0; i < num_fields; ++i) {
			std::string colName = fields[i].name ? fields[i].name : "UNKNOWN";
			std::string colValue = row[i] ? row[i] : "NULL";
			resultRow->addChild(colName, new JAIL::JObject(colValue));
		}

		c->setReturnVar(resultRow);
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
        queryResults.erase(idx);

        c->getReturnVar()->setString("Connection closed.");
    }
	
	__declspec(dllexport) void addSlashesToString(JAIL::JObject *c, void *data) {
		
		std::string src = c->getParameter("str")->getString();
		src = addSlashes(src);
		c->getReturnVar()->setString(src);
		
	}

    __declspec(dllexport) void registerLib(JAIL::JInterpreter *interpreter) {
        interpreter->addNative("function MySQL.connect(host, username, password, database)", createConnection, 0);
        interpreter->addNative("function MySQL.query(conn, query)", my_use_mysql_connector, 0);
        interpreter->addNative("function MySQL.fetchArray(conn)", fetchArray, 0);
		interpreter->addNative("function MySQL.fetchAssoc(conn)", fetchArrayAssoc, 0);
        interpreter->addNative("function MySQL.close(conn)", closeConnection, 0);
		interpreter->addNative("function MySQL.addSlashes(str)", addSlashesToString, 0);
    }
}
