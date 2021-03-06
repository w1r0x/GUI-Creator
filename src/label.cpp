#include "pch.h"
#include "label.h"
#include "font_manager.h"
#include "layer.h"
#include "lua_script.h"
#include "project.h"
#include "utils.h"

Label::Label()
{
}

Label::Label(const QString &name, int id, const QPointF &pos, const QString &fileName, int size, Layer *parent)
: GameObject(name, id, parent), mText(name), mFileName(fileName), mFontSize(size), mHorzAlignment(HORZ_ALIGN_LEFT),
  mVertAlignment(VERT_ALIGN_TOP), mLineSpacing(1.0), mColor(Qt::white)
{
	// загружаем шрифт
	mFont = FontManager::getSingleton().loadFont(mFileName, mFontSize);

	// задаем начальную позицию и размер надписи
	if (!mFont.isNull())
	{
		mSize = QSizeF(qCeil(mFont->getWidth(getText())) + 1.0, qCeil(mFont->getHeight()));
		mPosition = QPointF(qFloor(pos.x() - mSize.width() / 2.0), qFloor(pos.y() - mSize.height() / 2.0));
	}

	// инициализируем локализованные свойства
	QString language = Project::getSingleton().getDefaultLanguage();
	mPositionXMap[language] = mPosition.x();
	mPositionYMap[language] = mPosition.y();
	mWidthMap[language] = mSize.width();
	mHeightMap[language] = mSize.height();

	mFileNameMap[language] = mFileName;
	mFontSizeMap[language] = mFontSize;
	mFontMap[language] = mFont;

	// обновляем текущую трансформацию
	updateTransform();
}

Label::~Label()
{
	// устанавливаем текущий контекст OpenGL для корректного удаления текстуры шрифта
	FontManager::getSingleton().makeCurrent();
}

QString Label::getText() const
{
	StringMap::const_iterator it = mTranslationMap.find(Project::getSingleton().getCurrentLanguage());
	return it != mTranslationMap.end() ? *it : mText;
}

void Label::setText(const QString &text)
{
	mText = text;
}

QString Label::getFileName() const
{
	return mFileName;
}

void Label::setFileName(const QString &fileName)
{
	// устанавливаем новое имя файла и загружаем шрифт
	Q_ASSERT(isLocalized());
	mFileName = fileName;
	mFont = FontManager::getSingleton().loadFont(mFileName, mFontSize);

	// записываем локализованное имя файла и шрифт для текущего языка
	QString language = Project::getSingleton().getCurrentLanguage();
	mFileNameMap[language] = mFileName;
	mFontMap[language] = mFont;
}

int Label::getFontSize() const
{
	return mFontSize;
}

void Label::setFontSize(int size)
{
	// устанавливаем новый размер шрифта и загружаем шрифт
	Q_ASSERT(isLocalized());
	mFontSize = size;
	mFont = FontManager::getSingleton().loadFont(mFileName, mFontSize);

	// записываем локализованный размер шрифта и шрифт для текущего языка
	QString language = Project::getSingleton().getCurrentLanguage();
	mFontSizeMap[language] = mFontSize;
	mFontMap[language] = mFont;
}

Label::HorzAlignment Label::getHorzAlignment() const
{
	return mHorzAlignment;
}

void Label::setHorzAlignment(HorzAlignment alignment)
{
	mHorzAlignment = alignment;
}

Label::VertAlignment Label::getVertAlignment() const
{
	return mVertAlignment;
}

void Label::setVertAlignment(VertAlignment alignment)
{
	mVertAlignment = alignment;
}

qreal Label::getLineSpacing() const
{
	return mLineSpacing;
}

void Label::setLineSpacing(qreal lineSpacing)
{
	mLineSpacing = lineSpacing;
}

QColor Label::getColor() const
{
	return mColor;
}

void Label::setColor(const QColor &color)
{
	mColor = color;
}

bool Label::load(QDataStream &stream)
{
	// загружаем общие данные игрового объекта
	if (!GameObject::load(stream))
		return false;

	// загружаем данные надписи
	int horzAlignment, vertAlignment;
	stream >> mText >> mFileNameMap >> mFontSizeMap >> horzAlignment >> vertAlignment >> mLineSpacing >> mColor;
	if (stream.status() != QDataStream::Ok)
		return false;
	mHorzAlignment = static_cast<HorzAlignment>(horzAlignment);
	mVertAlignment = static_cast<VertAlignment>(vertAlignment);

	// загружаем локализованные шрифты
	loadFonts();
	return true;
}

bool Label::save(QDataStream &stream)
{
	// сохраняем тип объекта
	stream << QString("Label");
	if (stream.status() != QDataStream::Ok)
		return false;

	// сохраняем общие данные игрового объекта
	if (!GameObject::save(stream))
		return false;

	// сохраняем данные надписи
	stream << mText << mFileNameMap << mFontSizeMap << mHorzAlignment << mVertAlignment << mLineSpacing << mColor;
	return stream.status() == QDataStream::Ok;
}

bool Label::load(LuaScript &script)
{
	// загружаем общие данные игрового объекта
	if (!GameObject::load(script))
		return false;

	// загружаем данные надписи
	int horzAlignment, vertAlignment;
	unsigned int color;
	if (!script.getString("text", mText) || !readStringMap(script, "fileName", mFileNameMap) || !readRealMap(script, "size", mFontSizeMap)
		|| !script.getInt("horzAlignment", horzAlignment) || !script.getInt("vertAlignment", vertAlignment)
		|| !script.getReal("lineSpacing", mLineSpacing) || !script.getUnsignedInt("color", color))
		return false;
	mHorzAlignment = static_cast<HorzAlignment>(horzAlignment);
	mVertAlignment = static_cast<VertAlignment>(vertAlignment);
	mColor = QColor::fromRgba(color);

	// загружаем локализованные шрифты
	loadFonts();
	return true;
}

bool Label::save(QTextStream &stream, int indent)
{
	// делаем отступ и записываем тип объекта
	stream << QString(indent, '\t') << "{type = \"Label\", ";

	// сохраняем общие данные игрового объекта
	if (!GameObject::save(stream, indent))
		return false;

	// сохраняем свойства надписи
	stream << ", text = " << Utils::quotify(mText) << ", fileName = ";
	writeStringMap(stream, mFileNameMap);
	stream << ", size = ";
	writeRealMap(stream, mFontSizeMap);
	stream << ", horzAlignment = " << mHorzAlignment << ", vertAlignment = " << mVertAlignment
		<< ", lineSpacing = " << mLineSpacing << ", color = 0x" << hex << mColor.rgba() << dec << "}";
	return stream.status() == QTextStream::Ok;
}

void Label::setCurrentLanguage(const QString &language)
{
	// устанавливаем язык для игрового объекта
	GameObject::setCurrentLanguage(language);

	// устанавливаем новые значения для имени файла, размера шрифта и объекта шрифта
	QString currentLanguage = isLocalized() ? language : Project::getSingleton().getDefaultLanguage();
	mFileName = mFileNameMap[currentLanguage];
	mFontSize = static_cast<int>(mFontSizeMap[currentLanguage]);
	mFont = mFontMap[currentLanguage];
}

bool Label::isLocalized() const
{
	QString language = Project::getSingleton().getCurrentLanguage();
	return GameObject::isLocalized() && mFileNameMap.contains(language) && mFontSizeMap.contains(language) && mFontMap.contains(language);
}

void Label::setLocalized(bool localized)
{
	QString currentLanguage = Project::getSingleton().getCurrentLanguage();
	QString defaultLanguage = Project::getSingleton().getDefaultLanguage();
	Q_ASSERT(currentLanguage != defaultLanguage);

	// устанавливаем локализацию для игрового объекта
	GameObject::setLocalized(localized);

	if (localized)
	{
		// копируем значения локализованных свойств из дефолтного языка
		mFileNameMap[currentLanguage] = mFileNameMap[defaultLanguage];
		mFontSizeMap[currentLanguage] = mFontSizeMap[defaultLanguage];
		mFontMap[currentLanguage] = mFontMap[defaultLanguage];
	}
	else
	{
		// удаляем значения локализованных свойств
		mFileNameMap.remove(currentLanguage);
		mFontSizeMap.remove(currentLanguage);
		mFontMap.remove(currentLanguage);
	}

	// устанавливаем текущий язык
	setCurrentLanguage(Project::getSingleton().getCurrentLanguage());
}

void Label::loadTranslations(LuaScript *script)
{
	// очищаем таблицу переводов
	mTranslationMap.clear();

	// выходим, если скрипт не задан
	if (script == NULL)
		return;

	// ищем таблицу с переводами в скрипте по ID объекта
	if (!script->pushTable(mObjectID))
		return;

	// читаем таблицу переводов
	script->firstEntry();
	while (script->nextEntry())
	{
		QString key, value;
		if (script->getString(value) && script->getString(key, false))
			mTranslationMap[key] = value;
	}

	// извлекаем таблицу переводов из стека
	script->popTable();
}

GameObject *Label::duplicate(Layer *parent) const
{
	// создаем свою копию и добавляем ее к родительскому слою
	Label *label = new Label(*this);
	if (parent != NULL)
		parent->insertGameObject(0, label);
	return label;
}

QStringList Label::getMissedFiles() const
{
	// возвращаем имена незагруженных шрифтов
	QStringList missedFiles;
	for (StringMap::const_iterator it = mFileNameMap.begin(); it != mFileNameMap.end(); ++it)
		if (mFontMap[it.key()]->isDefault())
			missedFiles.push_back(*it);
	return missedFiles;
}

bool Label::changeTexture(const QString &fileName, const QSharedPointer<Texture> &texture)
{
	return false;
}

void Label::draw()
{
	// сохраняем матрицу трансформации
	glPushMatrix();

	// задаем новую трансформацию и цвет
	QPointF scale(mSize.width() >= 0.0 ? 1.0 : -1.0, mSize.height() >= 0.0 ? 1.0 : -1.0);
	glTranslated(mPosition.x(), mPosition.y(), 0.0);
	glRotated(mRotationAngle, 0.0, 0.0, 1.0);
	glColor4d(mColor.redF(), mColor.greenF(), mColor.blueF(), mColor.alphaF());

	// разбиваем текст на строки
	QStringList lines;
	qreal spaceWidth = mFont->getWidth(" ");
	foreach (const QString &line, getText().split("\n"))
	{
		// разбиваем строку на слова
		qreal width = 0.0, oldWidth = 0.0;
		QString str;
		foreach (const QString &word, line.split(" ", QString::SkipEmptyParts))
		{
			// определяем ширину текущего слова в пикселях
			qreal wordWidth = mFont->getWidth(word);
			width += wordWidth;

			// переносим строку, если ее суммарная ширина превышает ширину текстового прямоугольника
			if (width >= qAbs(mSize.width()) && oldWidth > 0.0)
			{
				lines.push_back(str.left(str.size() - 1));
				str.clear();
				width = wordWidth;
			}

			// склеиваем слова в строке, разделяя их пробелами
			str += word + " ";
			width += spaceWidth;
			oldWidth = width;
		}

		// добавляем последнюю строку в список строк
		lines.push_back(str.left(str.size() - 1));
	}

	// определяем начальную координату по оси Y с учетом вертикального выравнивания
	qreal y = 0.0;
	qreal height = ((lines.size() - 1) * mLineSpacing + 1.0) * mFont->getHeight();
	if (mVertAlignment == VERT_ALIGN_CENTER)
		y = (qAbs(mSize.height()) - height) / 2.0;
	else if (mVertAlignment == VERT_ALIGN_BOTTOM)
		y = qAbs(mSize.height()) - height;

	// выводим текст построчно
	foreach (const QString &line, lines)
	{
		// определяем координату текущей строки по оси X с учетом горизонтального выравнивания
		qreal x = 0.0;
		qreal width = mFont->getWidth(line);
		if (mHorzAlignment == HORZ_ALIGN_CENTER)
			x = (qAbs(mSize.width()) - width) / 2.0;
		else if (mHorzAlignment == HORZ_ALIGN_RIGHT)
			x = qAbs(mSize.width()) - width;

		// отрисовываем текущую строку
		glPushMatrix();
		glTranslated(qCeil(x) * scale.x(), (qCeil(y) + qRound(mFont->getHeight() / 1.25)) * scale.y(), 0.0);
		glScaled(scale.x(), -scale.y(), 1.0);
		mFont->draw(line);
		glPopMatrix();

		// переходим на следующую строку текста
		y += mFont->getHeight() * mLineSpacing;
	}

	// восстанавливаем матрицу трансформации
	glPopMatrix();
}

void Label::loadFonts()
{
	// загружаем локализованные шрифты
	mFontMap.clear();
	for (StringMap::const_iterator it = mFileNameMap.begin(); it != mFileNameMap.end(); ++it)
		mFontMap[it.key()] = FontManager::getSingleton().loadFont(*it, static_cast<int>(mFontSizeMap[it.key()]));

	// устанавливаем текущий язык
	setCurrentLanguage(Project::getSingleton().getCurrentLanguage());
}
