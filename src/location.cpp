#include "pch.h"
#include "location.h"
#include "layer.h"
#include "layer_group.h"
#include "lua_script.h"

Location::Location(QObject *parent)
: QObject(parent), mLayerIndex(1), mLayerGroupIndex(1)
{
	// создаем корневой слой
	mRootLayer = new LayerGroup("");

	// добавляем в него новый слой по умолчанию и делаем его активным
	mActiveLayer = createLayer(mRootLayer);
}

Location::~Location()
{
	delete mRootLayer;
}

bool Location::load(const QString &fileName)
{
	// загружаем Lua скрипт
	LuaScript script;
	if (!script.load(fileName))
		return false;

	// пересоздаем корневой слой
	delete mRootLayer;
	mRootLayer = new LayerGroup();

	// читаем таблицу слоев
	QString type;
	if (!script.pushTable("layers") || script.getLength() == 0 || !script.getString("type", type) || type != "LayerGroup" || !mRootLayer->load(script))
		return false;
	script.popTable();

	// устанавливаем активный слой
	int length;
	if (!script.pushTable("activeLayer") || (length = script.getLength()) == 0)
		return false;

	mActiveLayer = mRootLayer;
	for (int i = 1; i <= length; ++i)
	{
		int index;
		if (!script.getInt(i, index) || index >= mActiveLayer->getNumChildLayers())
			return false;
		mActiveLayer = mActiveLayer->getChildLayer(index);
	}

	script.popTable();

	// загружаем счетчики слоев и групп для генерации имен
	if (!script.getInt("layerIndex", mLayerIndex) || !script.getInt("layerGroupIndex", mLayerGroupIndex))
		return false;

	return true;
}

bool Location::save(const QString &fileName)
{
	// открываем файл
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly))
		return false;

	// создаем текстовый поток
	QTextStream stream(&file);
	stream << qSetRealNumberPrecision(8) << uppercasedigits;

	// записываем шапку файла
	stream << "-- *****************************************************************************" << endl;
	stream << "-- This file was automatically generated by " << QApplication::applicationName() << " editor" << endl;
	stream << "-- All changes made in this file will be lost. DO NOT EDIT!" << endl;
	stream << "-- *****************************************************************************" << endl;

	// сохраняем слои
	stream << endl << "layers =" << endl;
	mRootLayer->save(stream, 0);
	stream << endl;

	// сохраняем последовательность индексов, ведущую к активному слою
	QStringList indices;
	for (BaseLayer *layer = mActiveLayer; layer != mRootLayer; layer = layer->getParentLayer())
		indices.push_front(QString::number(layer->getParentLayer()->indexOfChildLayer(layer)));
	stream << endl << "activeLayer = {" << indices.join(", ") << "}" << endl;

	// сохраняем счетчики слоев и групп для генерации имен
	stream << "layerIndex = " << QString::number(mLayerIndex) << endl;
	stream << "layerGroupIndex = " << QString::number(mLayerGroupIndex) << endl;

	return stream.status() == QTextStream::Ok;
}

BaseLayer *Location::getRootLayer() const
{
	return mRootLayer;
}

BaseLayer *Location::getActiveLayer() const
{
	return mActiveLayer;
}

void Location::setActiveLayer(BaseLayer *layer)
{
	mActiveLayer = layer;
}

Layer *Location::getAvailableLayer() const
{
	// возвращаем указатель на активный слой, если он не папка, видим и не заблокирован
	Layer *layer = dynamic_cast<Layer *>(mActiveLayer);
	return layer != NULL && layer->getVisibleState() == BaseLayer::LAYER_VISIBLE && layer->getLockState() == BaseLayer::LAYER_UNLOCKED ? layer : NULL;
}

BaseLayer *Location::createLayer(BaseLayer *parent, int index)
{
	return new Layer(QString("Слой %1").arg(mLayerIndex++), parent, index);
}

BaseLayer *Location::createLayerGroup(BaseLayer *parent, int index)
{
	return new LayerGroup(QString("Группа %1").arg(mLayerGroupIndex++), parent, index);
}