/*
 * scriptloader.cpp
 *
 *  Created on: Apr 29, 2022
 *      Author: pp
 */


#include "luaEngine.hpp"

#include <filesystem>
#include <exception>
#include <QDirIterator>

extern "C"
{
#if __has_include(<lua5.3/lua.h>)
	#include <lua5.3/lua.h>
	#include <lua5.3/lualib.h>
	#include <lua5.3/lauxlib.h>
#elif  __has_include(<lua.h>)
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
#else
	#error("No lua libraries found")
#endif
}

#include <LuaBridge/LuaBridge.h>



using namespace std;
using namespace luabridge;
using std::filesystem::directory_iterator;


static lua_State* L;

/**
 * @return [false, ...] if invalid
 */
LuaRef loadScriptFromFile(lua_State* L, filesystem::path p) {
	LuaRef scriptloader = getGlobal(L, "scriptloader_file");
	try {
		LuaResult r = scriptloader(p.c_str());
		if(!r.wasOk()) {
			cerr << p << ": " << r.errorMessage() << endl;
			return LuaRef(L);
		}
		if(r.size() != 1) {
			//cerr << p << " failed." << endl;
			return LuaRef(L);
		}
		if(!r[0].isTable()) {
			cerr << p << ": " << r[0] << endl;
			return LuaRef(L);
		}
		return r[0];

	} catch(LuaException& e)	{
		cerr << "serious shit got down in file " << p << endl;
		cerr << e.what() << endl;
		return LuaRef(L);
	}
}

/**
 * @return [false, ...] if invalid
 */
LuaRef loadScriptFromString(lua_State* L, std::string p) {
	LuaRef scriptloader = getGlobal(L, "scriptloader_string");
	try {
		LuaResult r = scriptloader(p);
		if(!r.wasOk()) {
			cerr << p << ": " << r.errorMessage() << endl;
			return LuaRef(L);
		}
		if(r.size() != 1) {
			//cerr << p << " failed." << endl;
			return LuaRef(L);
		}
		if(!r[0].isTable()) {
			cerr << p << ": " << r[0] << endl;
			return LuaRef(L);
		}
		return r[0];

	} catch(LuaException& e)	{
		cerr << "serious shit got down in string \n" << p << endl;
		cerr << e.what() << endl;
		return LuaRef(L);
	}
}

bool isScriptValidDevice(LuaRef& chunk, std::string name = "") {
	if(chunk.isNil()) {
		cerr << "[lua]\tScript " << name << " could not be loaded" << endl;
		return false;
	}
	if(chunk["classname"].isNil() || !chunk["classname"].isString()) {
		cerr << "[lua]\tScript " << name << " does not contain a (valid) classname" << endl;
		return false;
	}
	return true;
}

LuaEngine::LuaEngine(){
	L = luaL_newstate();
	luaL_openlibs(L);

	QFile loader(scriptloader.c_str());
	if (!loader.open(QIODevice::ReadOnly)) {
		throw(runtime_error("Could not open scriptloader at " + scriptloader));
	}
	QByteArray loader_content = loader.readAll();

	// TODO: If offering c-functions, do this before initalizing scriptloader

	if( luaL_dostring( L, loader_content) )
	{
		cerr << "Error loading loadscript:\n" <<
				 lua_tostring( L, lua_gettop( L ) ) << endl;
		lua_pop( L, 1 );
		throw(runtime_error("Loadscript not valid"));
	}

	//cout << "Scanning built-in devices..." << endl;

	QDirIterator it(":/devices/lua");
	while (it.hasNext()) {
		it.next();
		//cout << "\t" << it.fileName().toStdString() << endl;
		QFile script_file(it.filePath());
		if (!script_file.open(QIODevice::ReadOnly)) {
			throw(runtime_error("Could not open file " + it.fileName().toStdString()));
		}
		QByteArray script = script_file.readAll();

		auto chunk = loadScriptFromString(L, script.toStdString());
		if(!isScriptValidDevice(chunk, it.filePath().toStdString()))
			continue;
		available_devices.emplace(chunk["classname"].cast<string>(), it.filePath().toStdString());
	}
}

// TODO: Reduce code duplication for file/string load
void LuaEngine::scanAdditionalDir(std::string dir) {
	cout << "[lua] Scanning additional devices at '" << dir << "'." << endl;

	QDirIterator it(dir.c_str(),
			QStringList() << "*.lua",
			QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		it.next();
		//cout << "\t" << it.fileName().toStdString() << endl;
		auto chunk = loadScriptFromFile(L,  it.filePath().toStdString());
		if(!isScriptValidDevice(chunk, it.filePath().toStdString()))
			continue;
		available_devices.emplace(chunk["classname"].cast<string>(), it.filePath().toStdString());
	}
}

void LuaEngine::printAvailableDevices(){
	cout << "Available devices: " << endl;
	for(const auto& [name, file] : available_devices) {
		cout << "\t" << name << " from " << file << endl;
	}
}
