#include "pch.h"
#include "lua_script.h"
#include "utils.h"

LuaScript::LuaScript()
: mLuaState(NULL), mTableIndex(0)
{
}

LuaScript::~LuaScript()
{
	if (mLuaState != NULL)
		lua_close(mLuaState);
}

bool LuaScript::load(const QString &fileName, int numReturnValues)
{
	// создаем и инициализируем новое состояние Lua
	mLuaState = luaL_newstate();
	if (mLuaState == NULL)
		return false;
	luaL_openlibs(mLuaState);

	// загружаем и выполняем скрипт
	if (luaL_loadfile(mLuaState, Utils::toStdString(fileName).c_str()) != 0 || lua_pcall(mLuaState, 0, numReturnValues, 0) != 0)
	{
		qWarning() << lua_tostring(mLuaState, -1);
		lua_pop(mLuaState, 1);
		return false;
	}

	return true;
}

bool LuaScript::getString(QString &value, bool pop) const
{
	// проверяем значение на вершине стека
	if (lua_isstring(mLuaState, -1) != 0)
	{
		value = lua_tostring(mLuaState, -1);
		if (pop)
			lua_pop(mLuaState, 1);
		return true;
	}

	// удаляем неверный элемент из стека
	if (pop)
		lua_pop(mLuaState, 1);
	return false;
}

bool LuaScript::getString(const QString &name, QString &value) const
{
	lua_getfield(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -1, name.toStdString().c_str());
	return getString(value);
}

bool LuaScript::getString(int index, QString &value) const
{
	lua_pushinteger(mLuaState, index);
	lua_gettable(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -2);
	return getString(value);
}

bool LuaScript::getInt(int &value, bool pop) const
{
	// проверяем значение на вершине стека
	if (lua_isnumber(mLuaState, -1) != 0)
	{
		value = lua_tointeger(mLuaState, -1);
		if (pop)
			lua_pop(mLuaState, 1);
		return true;
	}

	// удаляем неверный элемент из стека
	if (pop)
		lua_pop(mLuaState, 1);
	return false;
}

bool LuaScript::getInt(const QString &name, int &value) const
{
	lua_getfield(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -1, name.toStdString().c_str());
	return getInt(value);
}

bool LuaScript::getInt(int index, int &value) const
{
	lua_pushinteger(mLuaState, index);
	lua_gettable(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -2);
	return getInt(value);
}

bool LuaScript::getUnsignedInt(unsigned int &value, bool pop) const
{
	// проверяем значение на вершине стека
	if (lua_isnumber(mLuaState, -1) != 0)
	{
		value = static_cast<unsigned int>(lua_tonumber(mLuaState, -1));
		if (pop)
			lua_pop(mLuaState, 1);
		return true;
	}

	// удаляем неверный элемент из стека
	if (pop)
		lua_pop(mLuaState, 1);
	return false;
}

bool LuaScript::getUnsignedInt(const QString &name, unsigned int &value) const
{
	lua_getfield(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -1, name.toStdString().c_str());
	return getUnsignedInt(value);
}

bool LuaScript::getUnsignedInt(int index, unsigned int &value) const
{
	lua_pushinteger(mLuaState, index);
	lua_gettable(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -2);
	return getUnsignedInt(value);
}

bool LuaScript::getReal(qreal &value, bool pop) const
{
	// проверяем значение на вершине стека
	if (lua_isnumber(mLuaState, -1) != 0)
	{
		value = lua_tonumber(mLuaState, -1);
		if (pop)
			lua_pop(mLuaState, 1);
		return true;
	}

	// удаляем неверный элемент из стека
	if (pop)
		lua_pop(mLuaState, 1);
	return false;
}

bool LuaScript::getReal(const QString &name, qreal &value) const
{
	lua_getfield(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -1, name.toStdString().c_str());
	return getReal(value);
}

bool LuaScript::getReal(int index, qreal &value) const
{
	lua_pushinteger(mLuaState, index);
	lua_gettable(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -2);
	return getReal(value);
}

bool LuaScript::getBool(bool &value, bool pop) const
{
	// проверяем значение на вершине стека
	if (lua_isboolean(mLuaState, -1) != 0)
	{
		value = lua_toboolean(mLuaState, -1);
		if (pop)
			lua_pop(mLuaState, 1);
		return true;
	}

	// удаляем неверный элемент из стека
	if (pop)
		lua_pop(mLuaState, 1);
	return false;
}

bool LuaScript::getBool(const QString &name, bool &value) const
{
	lua_getfield(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -1, name.toStdString().c_str());
	return getBool(value);
}

bool LuaScript::getBool(int index, bool &value) const
{
	lua_pushinteger(mLuaState, index);
	lua_gettable(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -2);
	return getBool(value);
}

int LuaScript::getLength() const
{
	return lua_objlen(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -1);
}

bool LuaScript::pushTable()
{
	// проверяем значение на вершине стека
	if (lua_istable(mLuaState, -1) != 0)
	{
		++mTableIndex;
		return true;
	}

	// удаляем неверный элемент из стека
	lua_pop(mLuaState, 1);
	return false;
}

bool LuaScript::pushTable(const QString &name)
{
	lua_getfield(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -1, name.toStdString().c_str());
	return pushTable();
}

bool LuaScript::pushTable(int index)
{
	lua_pushinteger(mLuaState, index);
	lua_gettable(mLuaState, mTableIndex == 0 ? LUA_GLOBALSINDEX : -2);
	return pushTable();
}

void LuaScript::firstEntry()
{
	lua_pushnil(mLuaState);
}

bool LuaScript::nextEntry()
{
	return lua_next(mLuaState, -2) != 0;
}

void LuaScript::popTable()
{
	// извлекаем таблицу из стека и уменьшаем индекс текущей таблицы
	lua_pop(mLuaState, 1);
	--mTableIndex;
}
