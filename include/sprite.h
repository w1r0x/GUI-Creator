#ifndef SPRITE_H
#define SPRITE_H

#include "game_object.h"
#include "texture.h"

// Класс спрайта
class Sprite : public GameObject
{
public:

	// Конструктор
	Sprite();

	// Конструктор
	Sprite(const QPointF &position, const QString &fileName, const QSharedPointer<Texture> &texture, Layer *parent = NULL);

	// Конструктор копирования
	Sprite(const Sprite &sprite);

	// Возвращает цвет спрайта
	QColor getColor() const;

	// Устанавливает цвет спрайта
	void setColor(const QColor &color);

	// Загружает объект из бинарного потока
	virtual bool load(QDataStream &stream);

	// Сохраняет объект в бинарный поток
	virtual bool save(QDataStream &stream);

	// Загружает объект из Lua скрипта
	virtual bool load(LuaScript &script);

	// Сохраняет объект в текстовый поток
	virtual bool save(QTextStream &stream, int indent);

	// Дублирует объект
	virtual GameObject *duplicate(Layer *parent = NULL) const;

	// Возвращает список отсутствующих текстур в объекте
	virtual QStringList getMissedTextures() const;

	// Заменяет текстуру в объекте
	virtual bool changeTexture(const QString &fileName, const QSharedPointer<Texture> &texture);

	// Отрисовывает объект
	virtual void draw();

private:

	QString                 mFileName;  // Имя файла с текстурой
	QSharedPointer<Texture> mTexture;   // Текстура спрайта
	QColor                  mColor;     // Цвет спрайта
};

#endif // SPRITE_H