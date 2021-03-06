#include "pch.h"
#include "editor_window.h"
#include "game_object.h"
#include "layer.h"
#include "options.h"
#include "project.h"
#include "scene.h"
#include "sprite.h"
#include "utils.h"

EditorWindow::EditorWindow(QWidget *parent, QGLWidget *shareWidget, const QString &fileName, QWidget *spriteWidget, QWidget *fontWidget)
: QGLWidget(shareWidget->format(), parent, shareWidget), mFileName(fileName), mUntitled(true), mEditorState(STATE_IDLE),
  mCameraPos(0.0, 0.0), mZoom(1.0), mSpriteWidget(spriteWidget), mFontWidget(fontWidget)
{
	// разрешаем события клавиатуры и перемещения мыши
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);

	// запрещаем очистку фона перед рисованием
	setAutoFillBackground(false);

	// разрешаем бросание объектов
	setAcceptDrops(true);

	// создаем шрифт для подписей на линейках
	mRulerFont = QFont("Arial", 8);

	// создаем курсор поворота
	mRotateCursor = QCursor(QPixmap(":/images/rotate_cursor.png"));

	// корректируем положение камеры, если включены линейки
	if (Options::getSingleton().isShowGuides())
		mCameraPos = QPointF(-RULER_SIZE, -RULER_SIZE);

	// обновляем разрешенные операции редактирования
	updateAllowedEditorActions();

	// создаем новую сцену
	mScene = new Scene(this);
	connect(mScene, SIGNAL(undoCommandChanged()), this, SIGNAL(undoCommandChanged()));
}

bool EditorWindow::load(const QString &fileName)
{
	if (mScene->load(fileName))
	{
		// устанавливаем имя файла
		mFileName = fileName;
		mUntitled = false;
		return true;
	}

	return false;
}

bool EditorWindow::save(const QString &fileName)
{
	if (mScene->save(fileName))
	{
		// устанавливаем имя файла и флаг неизмененной сцены
		mFileName = fileName;
		mUntitled = false;
		mScene->setClean();
		return true;
	}

	return false;
}

bool EditorWindow::loadTranslationFile(const QString &fileName)
{
	return mScene->loadTranslationFile(fileName);
}

Scene *EditorWindow::getScene() const
{
	return mScene;
}

QString EditorWindow::getFileName() const
{
	return mFileName;
}

bool EditorWindow::isUntitled() const
{
	return mUntitled;
}

bool EditorWindow::isClean() const
{
	return mScene->isClean();
}

QUndoStack *EditorWindow::getUndoStack() const
{
	return mScene->getUndoStack();
}

void EditorWindow::pushCommand(const QString &commandName)
{
	mScene->pushCommand(commandName);
}

qreal EditorWindow::getZoom() const
{
	return mZoom;
}

void EditorWindow::setZoom(qreal zoom)
{
	// игнорируем разницу в полпроцента, вызванную округлением
	if (Utils::isEqual(zoom, mZoom, 0.005))
		return;

	// смещение текущего положения камеры, чтобы новый центр экрана совпадал с текущим центром экрана
	QPointF size(width(), height());
	mCameraPos = mCameraPos + size / 2.0 / mZoom - size / 2.0 / zoom;
	mZoom = zoom;

	// обновляем курсор мыши
	QPointF pos = windowToWorld(mapFromGlobal(QCursor::pos()));
	updateMouseCursor(pos);

	// отправляем сигнал об изменении координат мыши
	emit mouseMoved(pos);
}

QList<GameObject *> EditorWindow::getSelectedObjects() const
{
	return mSelectedObjects;
}

QPointF EditorWindow::getRotationCenter() const
{
	return mSnappedCenter;
}

bool EditorWindow::canUndo() const
{
	return mScene->canUndo();
}

bool EditorWindow::canRedo() const
{
	return mScene->canRedo();
}

void EditorWindow::undo()
{
	mScene->undo();
}

void EditorWindow::redo()
{
	mScene->redo();
}

void EditorWindow::cut()
{
	if (mEditorState == STATE_IDLE && !mSelectedObjects.empty() && canDeleteSelectedObjects())
	{
		copy();
		QSet<BaseLayer *> layers = getParentLayers();
		qDeleteAll(mSelectedObjects);
		deselectAll();
		emitSceneAndLayerChangedSignals(layers, "Вырезание объектов");
	}
}

void EditorWindow::copy()
{
	if (mEditorState == STATE_IDLE && !mSelectedObjects.empty())
	{
		// сортируем выделенные объекты по глубине
		sortSelectedGameObjects();

		// сохраняем объекты в байтовый массив
		QByteArray data;
		QDataStream stream(&data, QIODevice::WriteOnly);
		foreach (GameObject *object, mSelectedObjects)
			if (!object->save(stream))
				return;

		// копируем данные в буфер обмена
		QMimeData *mimeData = new QMimeData();
		mimeData->setData("application/x-gameobject", data);
		QApplication::clipboard()->setMimeData(mimeData);
	}
}

void EditorWindow::paste()
{
	Layer *layer = mScene->getAvailableLayer();
	if (mEditorState == STATE_IDLE && layer != NULL)
	{
		// получаем байтовый массив
		QByteArray data = QApplication::clipboard()->mimeData()->data("application/x-gameobject");
		if (data.isEmpty())
			return;

		// создаем и загружаем объекты из буфера обмена
		QDataStream stream(data);
		QList<GameObject *> objects;
		while (!stream.atEnd())
		{
			GameObject *object = mScene->loadGameObject(stream);
			if (object != NULL)
			{
				objects.push_back(object);
			}
			else
			{
				qDeleteAll(objects);
				return;
			}
		}

		// генерируем новые имена и ID для созданных объектов и добавляем их в активный слой
		for (int i = objects.size() - 1; i >= 0; --i)
		{
			objects[i]->setName(mScene->generateDuplicateName(objects[i]->getName()));
			objects[i]->setObjectID(mScene->generateDuplicateObjectID());
			layer->insertGameObject(0, objects[i]);
		}

		// выделяем вставленные объекты
		selectGameObjects(objects);

		// обновляем курсор мыши и посылаем сигналы об изменении сцены и слоев
		updateMouseCursor(windowToWorld(mapFromGlobal(QCursor::pos())));
		emitSceneAndLayerChangedSignals(getParentLayers(), "Вставка объектов");
	}
}

void EditorWindow::clear()
{
	if (mEditorState == STATE_IDLE && !mSelectedObjects.empty() && canDeleteSelectedObjects())
	{
		QSet<BaseLayer *> layers = getParentLayers();
		qDeleteAll(mSelectedObjects);
		deselectAll();
		emitSceneAndLayerChangedSignals(layers, "Удаление объектов");
	}
}

void EditorWindow::selectAll()
{
	if (mEditorState == STATE_IDLE)
	{
		selectGameObjects(mScene->getRootLayer()->findActiveGameObjects());
		updateMouseCursor(windowToWorld(mapFromGlobal(QCursor::pos())));
	}
}

void EditorWindow::deselectAll()
{
	if (mEditorState == STATE_IDLE && !mSelectedObjects.empty())
	{
		selectGameObjects(QList<GameObject *>());
		updateMouseCursor(windowToWorld(mapFromGlobal(QCursor::pos())));
	}
}

void EditorWindow::bringToFront()
{
	if (mEditorState == STATE_IDLE && !mSelectedObjects.empty())
	{
		// сортируем выделенные объекты по глубине
		sortSelectedGameObjects();

		// перемещаем выделенные объекты в начало списка у родительского слоя
		bool moved = false;
		Layer *parent = NULL;
		int first = 0;
		for (int i = 0; i < mSelectedObjects.size(); ++i)
		{
			// переходим на новый слой, если требуется
			if (mSelectedObjects[i]->getParentLayer() != parent)
			{
				parent = mSelectedObjects[i]->getParentLayer();
				first = i;
			}

			// перемещаем текущий объект в соответствующую позицию в начале списка
			int index = parent->indexOfGameObject(mSelectedObjects[i]);
			int insertIndex = i - first;
			if (index > insertIndex)
			{
				moved = true;
				parent->removeGameObject(index);
				parent->insertGameObject(insertIndex, mSelectedObjects[i]);
			}
		}

		// обновляем курсор мыши и посылаем сигналы об изменении сцены и слоев
		if (moved)
		{
			updateMouseCursor(windowToWorld(mapFromGlobal(QCursor::pos())));
			emitSceneAndLayerChangedSignals(getParentLayers(), "Перемещение объектов на передний план");
		}
	}
}

void EditorWindow::sendToBack()
{
	if (mEditorState == STATE_IDLE && !mSelectedObjects.empty())
	{
		// сортируем выделенные объекты по глубине
		sortSelectedGameObjects();

		// перемещаем выделенные объекты в конец списка у родительского слоя
		bool moved = false;
		Layer *parent = NULL;
		int last = 0;
		for (int i = mSelectedObjects.size() - 1; i >= 0; --i)
		{
			// переходим на новый слой, если требуется
			if (mSelectedObjects[i]->getParentLayer() != parent)
			{
				parent = mSelectedObjects[i]->getParentLayer();
				last = i;
			}

			// перемещаем текущий объект в соответствующую позицию в конце списка
			int index = parent->indexOfGameObject(mSelectedObjects[i]);
			int insertIndex = parent->getNumGameObjects() - 1 - (last - i);
			if (index < insertIndex)
			{
				moved = true;
				parent->removeGameObject(index);
				parent->insertGameObject(insertIndex, mSelectedObjects[i]);
			}
		}

		// обновляем курсор мыши и посылаем сигналы об изменении сцены и слоев
		if (moved)
		{
			updateMouseCursor(windowToWorld(mapFromGlobal(QCursor::pos())));
			emitSceneAndLayerChangedSignals(getParentLayers(), "Перемещение объектов на задний план");
		}
	}
}

void EditorWindow::moveUp()
{
	if (mEditorState == STATE_IDLE && !mSelectedObjects.empty())
	{
		// сортируем выделенные объекты по глубине
		sortSelectedGameObjects();

		// перемещаем выделенные объекты на одну позицию вверх
		bool moved = false;
		Layer *parent = NULL;
		int first = 0;
		for (int i = 0; i < mSelectedObjects.size(); ++i)
		{
			// переходим на новый слой, если требуется
			if (mSelectedObjects[i]->getParentLayer() != parent)
			{
				parent = mSelectedObjects[i]->getParentLayer();
				first = i;
			}

			// перемещаем текущий объект на одну позицию вверх
			int index = parent->indexOfGameObject(mSelectedObjects[i]);
			if (index > i - first)
			{
				moved = true;
				parent->removeGameObject(index);
				parent->insertGameObject(index - 1, mSelectedObjects[i]);
			}
		}

		// обновляем курсор мыши и посылаем сигналы об изменении сцены и слоев
		if (moved)
		{
			updateMouseCursor(windowToWorld(mapFromGlobal(QCursor::pos())));
			emitSceneAndLayerChangedSignals(getParentLayers(), "Перемещение объектов вверх");
		}
	}
}

void EditorWindow::moveDown()
{
	if (mEditorState == STATE_IDLE && !mSelectedObjects.empty())
	{
		// сортируем выделенные объекты по глубине
		sortSelectedGameObjects();

		// перемещаем выделенные объекты на одну позицию вниз
		bool moved = false;
		Layer *parent = NULL;
		int last = 0;
		for (int i = mSelectedObjects.size() - 1; i >= 0; --i)
		{
			// переходим на новый слой, если требуется
			if (mSelectedObjects[i]->getParentLayer() != parent)
			{
				parent = mSelectedObjects[i]->getParentLayer();
				last = i;
			}

			// перемещаем текущий объект на одну позицию вниз
			int index = parent->indexOfGameObject(mSelectedObjects[i]);
			if (index < parent->getNumGameObjects() - 1 - (last - i))
			{
				moved = true;
				parent->removeGameObject(index);
				parent->insertGameObject(index + 1, mSelectedObjects[i]);
			}
		}

		// обновляем курсор мыши и посылаем сигналы об изменении сцены и слоев
		if (moved)
		{
			updateMouseCursor(windowToWorld(mapFromGlobal(QCursor::pos())));
			emitSceneAndLayerChangedSignals(getParentLayers(), "Перемещение объектов вниз");
		}
	}
}

void EditorWindow::setCurrentLanguage(const QString &language)
{
	// устанавливаем новый язык
	mScene->getRootLayer()->setCurrentLanguage(language);

	// обновляем разрешенные операции редактирования
	updateAllowedEditorActions();

	// пересчитываем прямоугольник выделения и центр вращения
	if (mEditorState == STATE_IDLE && !mSelectedObjects.empty())
	{
		QPointF scale((mOriginalCenter.x() - mOriginalRect.left()) / mOriginalRect.width(),
			(mOriginalCenter.y() - mOriginalRect.top()) / mOriginalRect.height());
		selectGameObjects(mSelectedObjects);
		QPointF rotationCenter(mOriginalRect.width() * scale.x() + mOriginalRect.left(),
			mOriginalRect.height() * scale.y() + mOriginalRect.top());
		mOriginalCenter = mSnappedCenter = mSelectedObjects.size() == 1 ? mSelectedObjects.front()->getRotationCenter() : rotationCenter;
		updateMouseCursor(windowToWorld(mapFromGlobal(QCursor::pos())));
	}
}

QStringList EditorWindow::getMissedFiles() const
{
	return mScene->getRootLayer()->getMissedFiles();
}

void EditorWindow::changeTexture(const QString &fileName, const QSharedPointer<Texture> &texture)
{
	// заменяем текстуры во всех объектах
	QList<GameObject *> objects = mScene->getRootLayer()->changeTexture(fileName, texture);

	// пересчитываем прямоугольник выделения и центр вращения
	if (mEditorState == STATE_IDLE && !mSelectedObjects.empty())
	{
		QPointF scale((mOriginalCenter.x() - mOriginalRect.left()) / mOriginalRect.width(),
			(mOriginalCenter.y() - mOriginalRect.top()) / mOriginalRect.height());
		selectGameObjects(mSelectedObjects);
		QPointF rotationCenter(mOriginalRect.width() * scale.x() + mOriginalRect.left(),
			mOriginalRect.height() * scale.y() + mOriginalRect.top());
		mOriginalCenter = mSnappedCenter = mSelectedObjects.size() == 1 ? mSelectedObjects.front()->getRotationCenter() : rotationCenter;
		updateMouseCursor(windowToWorld(mapFromGlobal(QCursor::pos())));
	}

	// формируем список измененных слоев
	QSet<BaseLayer *> layers;
	foreach (GameObject *object, objects)
		layers |= object->getParentLayer();

	// выдаем сигналы об изменении слоев
	foreach (BaseLayer *layer, layers)
		emit layerChanged(mScene, layer);
}

void EditorWindow::updateSelection(const QPointF &rotationCenter)
{
	// обновляем прямоугольник выделения и текущий центр вращения
	if (mEditorState == STATE_IDLE)
	{
		selectGameObjects(mSelectedObjects);
		mOriginalCenter = mSnappedCenter = mSelectedObjects.size() == 1 ? mSelectedObjects.front()->getRotationCenter() : rotationCenter;
		updateMouseCursor(windowToWorld(mapFromGlobal(QCursor::pos())));
	}
}

void EditorWindow::updateAllowedEditorActions()
{
	// определяем, не заблокировано ли изменение размеров выделенных объектов
	bool sizeLocked = false;
	foreach (GameObject *object, mSelectedObjects)
	{
		Sprite *sprite = dynamic_cast<Sprite *>(object);
		if (sprite != NULL && sprite->isSizeLocked())
			sizeLocked = true;
	}

	// проверяем текущую локаль
	if (Project::getSingleton().getCurrentLanguage() == Project::getSingleton().getDefaultLanguage())
	{
		// в дефолтной локали разрешаем все операции редактирования
		mMoveEnabled = true;
		mResizeEnabled = !sizeLocked;
		mRotationEnabled = true;
		mMoveCenterEnabled = true;
	}
	else
	{
		// в недефолтной локали запрещаем поворот и перемещение центра вращения
		mRotationEnabled = false;
		mMoveCenterEnabled = false;

		// разрешаем перемещение и изменение размеров, только если все выделенные объекты локализованы
		bool allLocalized = true;
		foreach (GameObject *object, mSelectedObjects)
			if (!object->isLocalized())
				allLocalized = false;
		mMoveEnabled = allLocalized;
		mResizeEnabled = allLocalized && !sizeLocked;
	}
}

void EditorWindow::paintEvent(QPaintEvent *event)
{
	// очищаем окно
	qglClearColor(QColor(33, 40, 48));
	glClear(GL_COLOR_BUFFER_BIT);

	// устанавливаем стандартную систему координат (0, 0, width, height) с началом координат в левом верхнем углу
	glViewport(0, 0, width(), height());
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0.0, width(), height(), 0.0);

	// устанавливаем камеру
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScaled(mZoom, mZoom, 1.0);
	glTranslated(-mCameraPos.x(), -mCameraPos.y(), 0.0);

	// определение видимой области
	QRectF visibleRect(mCameraPos.x(), mCameraPos.y(), width() / mZoom, height() / mZoom);

	// рисуем сетку
	Options &options = Options::getSingleton();
	if (options.isShowGrid())
	{
		// подбираем подходящий шаг сетки
		int gridSpacing = options.getGridSpacing();
		while (gridSpacing * mZoom < MIN_GRID_SPACING)
			gridSpacing *= GRID_SPACING_COEFF;

		// определение номеров линий для отрисовки
		int left = static_cast<int>(visibleRect.left() / gridSpacing);
		int top = static_cast<int>(visibleRect.top() / gridSpacing);
		int right = static_cast<int>(visibleRect.right() / gridSpacing);
		int bottom = static_cast<int>(visibleRect.bottom() / gridSpacing);

		// устанавливаем сдвиг в 0.5 пикселя
		glPushMatrix();
		glTranslated(0.5 / mZoom, 0.5 / mZoom, 0.0);

		// рисуем сетку из линий или точек
		int interval = options.getMajorLinesInterval();
		if (!options.isShowDots())
		{
			glBegin(GL_LINES);

			// рисуем вспомогательные линии
			qglColor(QColor(39, 45, 56));
			for (int i = left; i <= right; ++i)
				if (i % interval != 0)
				{
					glVertex2d(i * gridSpacing, visibleRect.top());
					glVertex2d(i * gridSpacing, visibleRect.bottom());
				}

			for (int i = top; i <= bottom; ++i)
				if (i % interval != 0)
				{
					glVertex2d(visibleRect.left(), i * gridSpacing);
					glVertex2d(visibleRect.right(), i * gridSpacing);
				}

			// рисуем основные линии
			qglColor(QColor(51, 57, 73));
			for (int i = left; i <= right; ++i)
				if (i % interval == 0)
				{
					glVertex2d(i * gridSpacing, visibleRect.top());
					glVertex2d(i * gridSpacing, visibleRect.bottom());
				}

			for (int i = top; i <= bottom; ++i)
				if (i % interval == 0)
				{
					glVertex2d(visibleRect.left(), i * gridSpacing);
					glVertex2d(visibleRect.right(), i * gridSpacing);
				}

			glEnd();
		}
		else
		{
			glBegin(GL_POINTS);

			// рисуем вспомогательные точки
			qglColor(QColor(51, 57, 73));
			for (int i = left; i <= right; ++i)
				for (int j = top; j <= bottom; ++j)
					if (i % interval != 0 && j % interval != 0)
						glVertex2d(i * gridSpacing, j * gridSpacing);

			// рисуем основные точки
			qglColor(QColor(77, 86, 110));
			for (int i = left; i <= right; ++i)
				for (int j = top; j <= bottom; ++j)
					if (i % interval == 0 || j % interval == 0)
						glVertex2d(i * gridSpacing, j * gridSpacing);

			glEnd();
		}

		// рисуем оси координат
		glBegin(GL_LINES);
		qglColor(QColor(109, 36, 38));
		glVertex2d(visibleRect.left(), 0.0);
		glVertex2d(visibleRect.right(), 0.0);
		qglColor(QColor(35, 110, 38));
		glVertex2d(0.0, visibleRect.top());
		glVertex2d(0.0, visibleRect.bottom());
		glEnd();

		// восстанавливаем матрицу трансформации
		glPopMatrix();
	}

	// рисуем сцену
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	mScene->getRootLayer()->draw();
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);

	// создаем painter для дополнительного рисования
	QPainter painter(this);

	// рисуем прямоугольник выделения
	if (!mSelectedObjects.empty())
	{
		// рисуем прямоугольники выделения объектов
		painter.setPen(QColor(0, 127, 255));
		foreach (GameObject *object, mSelectedObjects)
		{
			qreal angle = object->getRotationAngle();
			if (angle == 0.0 || angle == 90.0 || angle == 180.0 || angle == 270.0)
			{
				QRectF rect = worldRectToWindow(object->getBoundingRect());
				painter.drawRect(rect.translated(0.5, 0.5));
			}
			else
			{
				painter.save();
				painter.translate(worldToWindow(object->getPosition()));
				painter.rotate(angle);
				painter.scale(mZoom, mZoom);
				painter.drawRect(0.0, 0.0, object->getSize().width(), object->getSize().height());
				painter.restore();
			}
		}

		// рисуем общий прямоугольник выделения
		QRectF rect = worldRectToWindow(mSnappedRect);
		painter.drawRect(rect.translated(0.5, 0.5));

		// рисуем маркеры выделения
		QPointF center(qFloor(rect.center().x()) + 0.5, qFloor(rect.center().y()) + 0.5);
		painter.setPen(QColor(100, 100, 100));
		painter.setBrush(QBrush(QColor(0, 127, 255)));
		drawSelectionMarker(rect.left() + 0.5, rect.top() + 0.5, painter);
		drawSelectionMarker(rect.left() + 0.5, center.y(), painter);
		drawSelectionMarker(rect.left() + 0.5, rect.bottom() + 0.5, painter);
		drawSelectionMarker(center.x(), rect.bottom() + 0.5, painter);
		drawSelectionMarker(rect.right() + 0.5, rect.bottom() + 0.5, painter);
		drawSelectionMarker(rect.right() + 0.5, center.y(), painter);
		drawSelectionMarker(rect.right() + 0.5, rect.top() + 0.5, painter);
		drawSelectionMarker(center.x(), rect.top() + 0.5, painter);

		// рисуем центр вращения
		center = worldToWindow(mSnappedCenter);
		qreal x = qFloor(center.x()) + 0.5, y = qFloor(center.y()) + 0.5;
		qreal offset = qFloor(CENTER_SIZE / 2.0);
		painter.setPen(QColor(0, 127, 255));
		painter.drawLine(QPointF(x - offset, y), QPointF(x + offset, y));
		painter.drawLine(QPointF(x, y - offset), QPointF(x, y + offset));
	}

	// рисуем рамку выделения
	if (mEditorState == STATE_SELECT && !mSelectionRect.isNull())
	{
		QRectF rect = worldRectToWindow(mSelectionRect.normalized());
		painter.setPen(QColor(0, 127, 255));
		painter.setBrush(QBrush(QColor(0, 100, 255, 60)));
		painter.drawRect(rect.translated(0.5, 0.5));
	}

	// рисуем направляющие
	if (options.isShowGuides())
	{
		// рисуем горизонтальные направляющие
		painter.setPen(QColor(0, 192, 192));
		int numHorzGuides = mScene->getNumGuides(true);
		for (int i = 0; i < numHorzGuides; ++i)
		{
			qreal y = qFloor((mScene->getGuide(true, i) - mCameraPos.y()) * mZoom) + 0.5;
			painter.drawLine(QPointF(0.5, y), QPointF(width() - 0.5, y));
		}

		// рисуем вертикальные направляющие
		int numVertGuides = mScene->getNumGuides(false);
		for (int i = 0; i < numVertGuides; ++i)
		{
			qreal x = qFloor((mScene->getGuide(false, i) - mCameraPos.x()) * mZoom) + 0.5;
			painter.drawLine(QPointF(x, 0.5), QPointF(x, height() - 0.5));
		}
	}

	// рисуем линии привязки
	if (isShowSnapLines())
	{
		painter.setPen(Qt::green);
		if (!Utils::isNull(mHorzSnapLine))
			drawSnapLine(mHorzSnapLine, painter);
		if (!Utils::isNull(mVertSnapLine))
			drawSnapLine(mVertSnapLine, painter);
	}

	// рисуем линейки
	if (options.isShowGuides())
	{
		// рисуем фон для линеек
		painter.fillRect(0, 0, width(), RULER_SIZE, Qt::white);
		painter.fillRect(0, 0, RULER_SIZE, height(), Qt::white);

		// подбираем цену деления
		qreal rulerSpacing = 10.0;
		while (rulerSpacing * mZoom < 10.0)
			rulerSpacing *= 2.0;
		while (rulerSpacing * mZoom > 20.0)
			rulerSpacing /= 2.0;

		// определяем номера делений для отрисовки
		const int RULER_INTERVAL = 10;
		int left = static_cast<int>(visibleRect.left() / rulerSpacing) - RULER_INTERVAL;
		int top = static_cast<int>(visibleRect.top() / rulerSpacing) - RULER_INTERVAL;
		int right = static_cast<int>(visibleRect.right() / rulerSpacing);
		int bottom = static_cast<int>(visibleRect.bottom() / rulerSpacing);

		// рисуем деления на горизонтальной линейке
		painter.setFont(mRulerFont);
		painter.setPen(Qt::black);
		QVector<QLineF> lines;
		lines.reserve(right - left + 1);
		for (int i = left; i <= right; ++i)
		{
			qreal x = qFloor((i * rulerSpacing - mCameraPos.x()) * mZoom) + 0.5;
			if (i % RULER_INTERVAL != 0)
				lines.push_back(QLineF(x, RULER_SIZE - DIVISION_SIZE + 0.5, x, RULER_SIZE - 0.5));
			else
				lines.push_back(QLineF(x, 0.5, x, RULER_SIZE - 0.5));
		}
		painter.drawLines(lines);

		// рисуем деления на вертикальной линейке
		lines.clear();
		lines.reserve(bottom - top + 1);
		for (int i = top; i <= bottom; ++i)
		{
			qreal y = qFloor((i * rulerSpacing - mCameraPos.y()) * mZoom) + 0.5;
			if (i % RULER_INTERVAL != 0)
				lines.push_back(QLineF(RULER_SIZE - DIVISION_SIZE + 0.5, y, RULER_SIZE - 0.5, y));
			else
				lines.push_back(QLineF(0.5, y, RULER_SIZE - 0.5, y));
		}
		painter.drawLines(lines);

		// рисуем подписи на горизонтальной линейке
		const QFontMetrics &metrics = painter.fontMetrics();
		int ascent = metrics.ascent();
		for (int i = left; i <= right; ++i)
			if (i % RULER_INTERVAL == 0)
			{
				int x = static_cast<int>((i * rulerSpacing - mCameraPos.x()) * mZoom);
				painter.drawText(x + 4, (RULER_SIZE - DIVISION_SIZE - ascent) / 2 + ascent, QString::number(i * rulerSpacing, 'g', 8));
			}

		// рисуем подписи на вертикальной линейке
		for (int i = top; i <= bottom; ++i)
			if (i % RULER_INTERVAL == 0)
			{
				int y = static_cast<int>((i * rulerSpacing - mCameraPos.y()) * mZoom);
				foreach (QChar ch, QString::number(i * rulerSpacing, 'g', 8))
				{
					painter.drawText((RULER_SIZE - DIVISION_SIZE - metrics.width(ch)) / 2, y + ascent + 2, QString(ch));
					y += ascent;
				}
			}

		// затираем фоном линеек левый верхний угол окна
		painter.fillRect(0, 0, RULER_SIZE, RULER_SIZE, Qt::white);
	}
}

void EditorWindow::mousePressEvent(QMouseEvent *event)
{
	// обрабатываем нажатие кнопок
	mLastPos = event->pos();
	QPointF pos = windowToWorld(event->pos());
	if (event->button() == Qt::LeftButton)
	{
		// сохраняем начальные оконные координаты мыши и сбрасываем флаг разрешения редактирования
		mFirstPos = event->pos();
		mEditEnabled = false;
		mHorzSnapLine = mVertSnapLine = QLineF();

		bool showGuides = Options::getSingleton().isShowGuides();
		qreal distance = GUIDE_DISTANCE / mZoom;
		qreal size = CENTER_SIZE / mZoom;
		qreal offset = size / 2.0;
		GameObject *object;

		if (showGuides && event->pos().y() < RULER_SIZE)
		{
			// создаем горизонтальную направляющую и переходим в режим перетаскивания
			mEditorState = STATE_HORZ_GUIDE;
			mGuideIndex = mScene->addGuide(true, qRound(pos.y()));
		}
		else if (showGuides && event->pos().x() < RULER_SIZE)
		{
			// создаем вертикальную направляющую и переходим в режим перетаскивания
			mEditorState = STATE_VERT_GUIDE;
			mGuideIndex = mScene->addGuide(false, qRound(pos.x()));
		}
		else if (showGuides && (mGuideIndex = mScene->findGuide(true, pos.y(), distance)) != -1)
		{
			// переходим в режим перетаскивания горизонтальной направляющей
			mEditorState = STATE_HORZ_GUIDE;
		}
		else if (showGuides && (mGuideIndex = mScene->findGuide(false, pos.x(), distance)) != -1)
		{
			// переходим в режим перетаскивания вертикальной направляющей
			mEditorState = STATE_VERT_GUIDE;
		}
		else if (mMoveCenterEnabled && !mSelectedObjects.empty() && QRectF(mSnappedCenter.x() - offset, mSnappedCenter.y() - offset, size, size).contains(pos))
		{
			// переходим в состояние перетаскивания центра вращения
			mEditorState = STATE_MOVE_CENTER;
		}
		else if (mResizeEnabled && (mSelectionMarker = findSelectionMarker(pos, MARKER_SIZE / mZoom)) != MARKER_NONE)
		{
			// переходим в состояние изменения размера объектов
			mEditorState = STATE_RESIZE;
		}
		else if (mRotationEnabled && findSelectionMarker(pos, ROTATE_SIZE / mZoom) != MARKER_NONE && findSelectionMarker(pos, MARKER_SIZE / mZoom) == MARKER_NONE)
		{
			// переходим в состояние поворота объектов
			mEditorState = STATE_ROTATE;
			mRotationVector = QVector2D(pos - mOriginalCenter).normalized();
		}
		else if ((object = mScene->getRootLayer()->findGameObjectByPoint(pos)) != NULL)
		{
			if ((event->modifiers() & Qt::ControlModifier) != 0)
			{
				// устанавливаем/снимаем выделение по щелчку с Ctrl
				mEditorState = STATE_IDLE;
				QList<GameObject *> objects = mSelectedObjects;
				if (objects.contains(object))
					objects.removeOne(object);
				else
					objects.push_back(object);
				selectGameObjects(objects);
				updateMouseCursor(pos);
			}
			else
			{
				// выделяем найденный объект
				if (!mSelectedObjects.contains(object))
					selectGameObject(object);

				// переходим в состояние перетаскивания
				if (mMoveEnabled)
				{
					mEditorState = STATE_MOVE;
					setCursor(Qt::ClosedHandCursor);
				}
			}
		}
		else
		{
			// ничего не нашли - сбрасываем текущее выделение и запускаем рамку выделения
			deselectAll();
			mEditorState = STATE_SELECT;
			mSelectionRect = QRectF(pos, pos);
		}
	}
}

void EditorWindow::mouseReleaseEvent(QMouseEvent *event)
{
	// обрабатываем отпускание кнопок
	QPointF pos = windowToWorld(event->pos());
	if (event->button() == Qt::LeftButton)
	{
		// обрабатываем текущее состояние редактирования
		if (mEditEnabled)
		{
			if (mEditorState == STATE_SELECT)
			{
				// выделяем игровые объекты, попавшие в рамку
				selectGameObjects(mScene->getRootLayer()->findGameObjectsByRect(mSelectionRect.normalized()));
			}
			else if (mEditorState == STATE_MOVE || mEditorState == STATE_RESIZE || mEditorState == STATE_ROTATE)
			{
				// обновляем текущее выделение
				selectGameObjects(mSelectedObjects);

				// обновляем координаты центра вращения
				mOriginalCenter = mSnappedCenter = mSelectedObjects.size() == 1 ? mSelectedObjects.front()->getRotationCenter() : mSnappedCenter;

				// посылаем сигналы об изменении сцены и слоев
				if (mEditorState == STATE_MOVE)
					emitSceneAndLayerChangedSignals(getParentLayers(), "Перемещение объектов");
				else if (mEditorState == STATE_RESIZE)
					emitSceneAndLayerChangedSignals(getParentLayers(), "Изменение размеров объектов");
				else
					emitSceneAndLayerChangedSignals(getParentLayers(), "Поворот объектов");
			}
			else if (mEditorState == STATE_MOVE_CENTER)
			{
				// обновляем координаты центра вращения
				mOriginalCenter = mSnappedCenter = mSelectedObjects.size() == 1 ? mSelectedObjects.front()->getRotationCenter() : mSnappedCenter;

				// посылаем сигнал об изменении сцены
				emit sceneChanged("Перемещение центра вращения");
			}
			else if (mEditorState == STATE_HORZ_GUIDE)
			{
				if (event->pos().y() >= RULER_SIZE && event->pos().y() < height())
				{
					// посылаем сигнал о создании или перемещении направляющей
					emit sceneChanged(mFirstPos.y() < RULER_SIZE ? "Создание направляющей" : "Перемещение направляющей");
				}
				else
				{
					// удаляем направляющую и посылаем сигнал, если ее вытащили за пределы окна со свободного места
					mScene->removeGuide(true, mGuideIndex);
					if (mFirstPos.y() >= RULER_SIZE)
						emit sceneChanged("Удаление направляющей");
				}
			}
			else if (mEditorState == STATE_VERT_GUIDE)
			{
				if (event->pos().x() >= RULER_SIZE && event->pos().x() < width())
				{
					// посылаем сигнал о создании или перемещении направляющей
					emit sceneChanged(mFirstPos.x() < RULER_SIZE ? "Создание направляющей" : "Перемещение направляющей");
				}
				else
				{
					// удаляем направляющую и посылаем сигнал, если ее вытащили за пределы окна со свободного места
					mScene->removeGuide(false, mGuideIndex);
					if (mFirstPos.x() >= RULER_SIZE)
						emit sceneChanged("Удаление направляющей");
				}
			}
		}
		else if (mEditorState == STATE_HORZ_GUIDE || mEditorState == STATE_VERT_GUIDE)
		{
			// удаляем созданную направляющую при щелчке по линейке без перетаскивания
			if (mFirstPos.x() < RULER_SIZE || mFirstPos.y() < RULER_SIZE)
				mScene->removeGuide(mEditorState == STATE_HORZ_GUIDE, mGuideIndex);
		}

		// возвращаемся в состояние простоя
		mEditorState = STATE_IDLE;
		updateMouseCursor(pos);
	}
}

void EditorWindow::mouseMoveEvent(QMouseEvent *event)
{
	// определяем смещение мыши с прошлого раза
	QPointF delta = QPointF(event->pos() - mLastPos) / mZoom;
	mLastPos = event->pos();

	// разрешаем редактирование, если мышь передвинулась достаточно далеко от начального положения при зажатой левой кнопке
	if ((event->buttons() & Qt::LeftButton) != 0 && (qAbs(mLastPos.x() - mFirstPos.x()) > 2 || qAbs(mLastPos.y() - mFirstPos.y()) > 2))
		mEditEnabled = true;

	// перемещаем камеру при зажатой правой кнопке мыши
	if ((event->buttons() & Qt::RightButton) != 0)
		mCameraPos -= delta;

	// обрабатываем текущее состояние редактирования
	QPointF pos = windowToWorld(event->pos());
	if (mEditEnabled)
	{
		if (mEditorState == STATE_SELECT)
		{
			mSelectionRect.setBottomRight(pos);
		}
		else if (mEditorState == STATE_MOVE)
		{
			// определяем вектор перемещения
			bool shift = (event->modifiers() & Qt::ShiftModifier) != 0;
			QVector2D axis;
			QPointF offset = calcTranslation(QPointF(mLastPos - mFirstPos) / mZoom, shift, axis);
			offset = Utils::round(mOriginalRect.topLeft() + offset) - mOriginalRect.topLeft();

			// определяем координаты привязываемого прямоугольника
			QRectF rect = mOriginalRect.translated(offset);
			mHorzSnapLine = mVertSnapLine = QLineF();

			// привязываем прямоугольник по оси X
			bool vertSnapOnCenter = false;
			if (!shift || Utils::isEqual(axis.y(), 0.0))
			{
				// вычисляем расстояния привязок для левого края, центра и правого края прямоугольника выделения
				QLineF leftLine, centerLine, rightLine;
				qreal leftDistance, centerDistance, rightDistance;
				qreal left = snapXCoord(rect.left(), rect.top(), rect.bottom(), true, &leftLine, &leftDistance);
				qreal center = snapXCoord(rect.center().x(), rect.center().y(), rect.center().y(), true, &centerLine, &centerDistance);
				qreal right = snapXCoord(rect.right(), rect.top(), rect.bottom(), true, &rightLine, &rightDistance);

				// привязываем край с наименьшим расстоянием привязки
				if (centerDistance < SNAP_DISTANCE / mZoom && centerDistance <= leftDistance && centerDistance <= rightDistance)
				{
					vertSnapOnCenter = true;
					offset.rx() += center - rect.center().x();
					mVertSnapLine = centerLine;
				}
				else if (leftDistance < SNAP_DISTANCE / mZoom && leftDistance <= centerDistance && leftDistance <= rightDistance)
				{
					offset.rx() += left - rect.left();
					mVertSnapLine = leftLine;
				}
				else if (rightDistance < SNAP_DISTANCE / mZoom && rightDistance <= leftDistance && rightDistance <= centerDistance)
				{
					offset.rx() += right - rect.right();
					mVertSnapLine = rightLine;
				}
				else
				{
					offset.rx() += left - rect.left();
				}
			}

			// привязываем прямоугольник по оси Y
			bool horzSnapOnCenter = false;
			if (!shift || Utils::isEqual(axis.x(), 0.0))
			{
				// вычисляем расстояния привязок для верхнего края, центра и нижнего края прямоугольника выделения
				QLineF topLine, centerLine, bottomLine;
				qreal topDistance, centerDistance, bottomDistance;
				qreal top = snapYCoord(rect.top(), rect.left(), rect.right(), true, &topLine, &topDistance);
				qreal center = snapYCoord(rect.center().y(), rect.center().x(), rect.center().x(), true, &centerLine, &centerDistance);
				qreal bottom = snapYCoord(rect.bottom(), rect.left(), rect.right(), true, &bottomLine, &bottomDistance);

				// привязываем край с наименьшим расстоянием привязки
				if (centerDistance < SNAP_DISTANCE / mZoom && centerDistance <= topDistance && centerDistance <= bottomDistance)
				{
					horzSnapOnCenter = true;
					offset.ry() += center - rect.center().y();
					mHorzSnapLine = centerLine;
				}
				else if (topDistance < SNAP_DISTANCE / mZoom && topDistance <= centerDistance && topDistance <= bottomDistance)
				{
					offset.ry() += top - rect.top();
					mHorzSnapLine = topLine;
				}
				else if (bottomDistance < SNAP_DISTANCE / mZoom && bottomDistance <= topDistance && bottomDistance <= centerDistance)
				{
					offset.ry() += bottom - rect.bottom();
					mHorzSnapLine = bottomLine;
				}
				else
				{
					offset.ry() += top - rect.top();
				}
			}

			// перемещаем все выделенные объекты
			for (int i = 0; i < mSelectedObjects.size(); ++i)
				mSelectedObjects[i]->setPosition(mOriginalPositions[i] + offset);

			// пересчитываем общий прямоугольник выделения
			mSnappedRect = mSelectedObjects.front()->getBoundingRect();
			foreach (GameObject *object, mSelectedObjects)
				mSnappedRect |= object->getBoundingRect();

			// перемещаем центр вращения
			mSnappedCenter = mSelectedObjects.size() == 1 ? mSelectedObjects.front()->getRotationCenter() : mOriginalCenter + offset;

			// корректируем координаты вертикальной линии привязки
			if (!Utils::isNull(mVertSnapLine))
			{
				if (vertSnapOnCenter)
					snapXCoord(mVertSnapLine.x1(), mSnappedRect.center().y(), mSnappedRect.center().y(), true, &mVertSnapLine);
				else
					snapXCoord(mVertSnapLine.x1(), mSnappedRect.top(), mSnappedRect.bottom(), true, &mVertSnapLine);
			}

			// корректируем координаты горизонтальной линии привязки
			if (!Utils::isNull(mHorzSnapLine))
			{
				if (horzSnapOnCenter)
					snapYCoord(mHorzSnapLine.y1(), mSnappedRect.center().x(), mSnappedRect.center().x(), true, &mHorzSnapLine);
				else
					snapYCoord(mHorzSnapLine.y1(), mSnappedRect.left(), mSnappedRect.right(), true, &mHorzSnapLine);
			}
		}
		else if (mEditorState == STATE_RESIZE)
		{
			// определяем значение масштаба и координаты точки, относительно которой он производится
			bool keepProportions = (event->modifiers() & Qt::ShiftModifier) != 0 || mKeepProportions;
			QPointF roundPos, pivot, scale;
			mHorzSnapLine = mVertSnapLine = QLineF();
			switch (mSelectionMarker)
			{
			case MARKER_TOP_LEFT:
				pivot = mOriginalRect.bottomRight();
				roundPos = Utils::round(pos - pivot) + pivot;
				scale = calcScale(roundPos, pivot, -1.0, -1.0, keepProportions);
				break;

			case MARKER_CENTER_LEFT:
				pivot = QPointF(mOriginalRect.right(), mOriginalRect.center().y());
				roundPos = Utils::round(pos - pivot) + pivot;
				scale.rx() = (pivot.x() - snapXCoord(roundPos.x(), mOriginalRect.top(), mOriginalRect.bottom(), true, &mVertSnapLine)) / mOriginalRect.width();
				scale.ry() = keepProportions ? qAbs(scale.x()) : 1.0;
				snapXCoord(roundPos.x(), pivot.y() - mOriginalRect.height() / 2.0 * scale.y(), pivot.y() + mOriginalRect.height() / 2.0 * scale.y(), true, &mVertSnapLine);
				break;

			case MARKER_BOTTOM_LEFT:
				pivot = mOriginalRect.topRight();
				roundPos = Utils::round(pos - pivot) + pivot;
				scale = calcScale(roundPos, pivot, -1.0, 1.0, keepProportions);
				break;

			case MARKER_BOTTOM_CENTER:
				pivot = QPointF(mOriginalRect.center().x(), mOriginalRect.top());
				roundPos = Utils::round(pos - pivot) + pivot;
				scale.ry() = (snapYCoord(roundPos.y(), mOriginalRect.left(), mOriginalRect.right(), true, &mHorzSnapLine) - pivot.y()) / mOriginalRect.height();
				scale.rx() = keepProportions ? qAbs(scale.y()) : 1.0;
				snapYCoord(roundPos.y(), pivot.x() - mOriginalRect.width() / 2.0 * scale.x(), pivot.x() + mOriginalRect.width() / 2.0 * scale.x(), true, &mHorzSnapLine);
				break;

			case MARKER_BOTTOM_RIGHT:
				pivot = mOriginalRect.topLeft();
				roundPos = Utils::round(pos - pivot) + pivot;
				scale = calcScale(roundPos, pivot, 1.0, 1.0, keepProportions);
				break;

			case MARKER_CENTER_RIGHT:
				pivot = QPointF(mOriginalRect.left(), mOriginalRect.center().y());
				roundPos = Utils::round(pos - pivot) + pivot;
				scale.rx() = (snapXCoord(roundPos.x(), mOriginalRect.top(), mOriginalRect.bottom(), true, &mVertSnapLine) - pivot.x()) / mOriginalRect.width();
				scale.ry() = keepProportions ? qAbs(scale.x()) : 1.0;
				snapXCoord(roundPos.x(), pivot.y() - mOriginalRect.height() / 2.0 * scale.y(), pivot.y() + mOriginalRect.height() / 2.0 * scale.y(), true, &mVertSnapLine);
				break;

			case MARKER_TOP_RIGHT:
				pivot = mOriginalRect.bottomLeft();
				roundPos = Utils::round(pos - pivot) + pivot;
				scale = calcScale(roundPos, pivot, 1.0, -1.0, keepProportions);
				break;

			case MARKER_TOP_CENTER:
				pivot = QPointF(mOriginalRect.center().x(), mOriginalRect.bottom());
				roundPos = Utils::round(pos - pivot) + pivot;
				scale.ry() = (pivot.y() - snapYCoord(roundPos.y(), mOriginalRect.left(), mOriginalRect.right(), true, &mHorzSnapLine)) / mOriginalRect.height();
				scale.rx() = keepProportions ? qAbs(scale.y()) : 1.0;
				snapYCoord(roundPos.y(), pivot.x() - mOriginalRect.width() / 2.0 * scale.x(), pivot.x() + mOriginalRect.width() / 2.0 * scale.x(), true, &mHorzSnapLine);
				break;

			default:
				break;
			}

			// ограничиваем минимальный размер рамки выделения одним пикселем
			if (qAbs(mOriginalRect.width() * scale.x()) < 1.0)
				scale.rx() = (scale.x() >= 0.0 ? 1.0 : -1.0) / mOriginalRect.width();
			if (qAbs(mOriginalRect.height() * scale.y()) < 1.0)
				scale.ry() = (scale.y() >= 0.0 ? 1.0 : -1.0) / mOriginalRect.height();

			// пересчитываем положение и размеры выделенных объектов
			for (int i = 0; i < mSelectedObjects.size(); ++i)
			{
				// устанавливаем новую позицию
				mSelectedObjects[i]->setPosition(QPointF((mOriginalPositions[i].x() - pivot.x()) * scale.x() + pivot.x(),
					(mOriginalPositions[i].y() - pivot.y()) * scale.y() + pivot.y()));

				// устанавливаем новый размер
				if (keepProportions || mOriginalAngles[i] == 0.0 || mOriginalAngles[i] == 180.0)
					mSelectedObjects[i]->setSize(QSizeF(mOriginalSizes[i].width() * scale.x(), mOriginalSizes[i].height() * scale.y()));
				else
					mSelectedObjects[i]->setSize(QSizeF(mOriginalSizes[i].width() * scale.y(), mOriginalSizes[i].height() * scale.x()));
			}

			// пересчитываем общий прямоугольник выделения
			mSnappedRect = mSelectedObjects.front()->getBoundingRect();
			foreach (GameObject *object, mSelectedObjects)
				mSnappedRect |= object->getBoundingRect();

			// пересчитываем координаты центра вращения
			mSnappedCenter = mSelectedObjects.size() == 1 ? mSelectedObjects.front()->getRotationCenter()
				: QPointF((mOriginalCenter.x() - pivot.x()) * scale.x() + pivot.x(), (mOriginalCenter.y() - pivot.y()) * scale.y() + pivot.y());
		}
		else if (mEditorState == STATE_ROTATE)
		{
			// определяем текущий угол поворота в радианах относительно начального вектора
			QVector2D currVector = QVector2D(pos - mOriginalCenter).normalized();
			qreal angle = qAcos(QVector2D::dotProduct(currVector, mRotationVector));
			if (QString::number(angle) == "nan" || Utils::isEqual(angle, 0.0))
				angle = 0.0;

			// приводим угол к диапазону [-PI; PI]
			QVector2D normalVector(-mRotationVector.y(), mRotationVector.x());
			if (QVector2D::dotProduct(currVector, normalVector) < 0.0)
				angle = -angle;

			// если зажат шифт, округляем угол до величины, кратной 45 градусам
			if ((event->modifiers() & Qt::ShiftModifier) != 0)
				angle = qFloor((angle + Utils::PI / 8.0) / (Utils::PI / 4.0)) * (Utils::PI / 4.0);

			// поворачиваем все выделенные объекты вокруг центра вращения
			for (int i = 0; i < mSelectedObjects.size(); ++i)
			{
				// определяем абсолютный угол поворота в градусах и приводим его к диапазону [0; 360)
				qreal absAngle = fmod(mOriginalAngles[i] + Utils::radToDeg(angle), 360.0);
				if (absAngle < 0.0)
					absAngle += 360.0;

				// привязываем абсолютный угол поворота к 0/90/180/270 градусам
				const qreal ANGLE_EPS = 0.5;
				if (Utils::isEqual(absAngle, 0.0, ANGLE_EPS) || Utils::isEqual(absAngle, 360.0, ANGLE_EPS))
					absAngle = 0.0;
				else if (Utils::isEqual(absAngle, 90.0, ANGLE_EPS))
					absAngle = 90.0;
				else if (Utils::isEqual(absAngle, 180.0, ANGLE_EPS))
					absAngle = 180.0;
				else if (Utils::isEqual(absAngle, 270.0, ANGLE_EPS))
					absAngle = 270.0;

				// поворачиваем объект на привязанный угол
				qreal newAngle = Utils::degToRad(absAngle - mOriginalAngles[i]);
				QPointF vec = mOriginalPositions[i] - mOriginalCenter;
				QPointF position(vec.x() * qCos(newAngle) - vec.y() * qSin(newAngle) + mOriginalCenter.x(),
					vec.x() * qSin(newAngle) + vec.y() * qCos(newAngle) + mOriginalCenter.y());
				if (absAngle == 0.0 || absAngle == 90.0 || absAngle == 180.0 || absAngle == 270.0)
					position = Utils::round(position);

				// устанавливаем новую позицию и угол поворота
				mSelectedObjects[i]->setPosition(position);
				mSelectedObjects[i]->setRotationAngle(absAngle);
			}

			// пересчитываем общий прямоугольник выделения
			mSnappedRect = mSelectedObjects.front()->getBoundingRect();
			foreach (GameObject *object, mSelectedObjects)
				mSnappedRect |= object->getBoundingRect();
		}
		else if (mEditorState == STATE_MOVE_CENTER)
		{
			// определяем вектор перемещения
			bool shift = (event->modifiers() & Qt::ShiftModifier) != 0;
			QVector2D axis;
			QPointF offset = calcTranslation(pos - mOriginalCenter, shift, axis);
			offset = Utils::round(mOriginalCenter + offset) - mOriginalCenter;

			// определяем координаты привязываемой точки
			QPointF pt = mOriginalCenter + offset;
			mHorzSnapLine = mVertSnapLine = QLineF();

			// привязываем точку по оси X
			if (!shift || Utils::isEqual(axis.y(), 0.0))
				offset.rx() += snapXCoord(pt.x(), pt.y(), pt.y(), false, &mVertSnapLine) - pt.x();

			// привязываем точку по оси Y
			if (!shift || Utils::isEqual(axis.x(), 0.0))
				offset.ry() += snapYCoord(pt.y(), pt.x(), pt.x(), false, &mHorzSnapLine) - pt.y();

			// перемещаем центр вращения
			mSnappedCenter = mOriginalCenter + offset;
			if (mSelectedObjects.size() == 1)
				mSelectedObjects.front()->setRotationCenter(mSnappedCenter);

			// корректируем координаты вертикальной линии привязки
			if (!Utils::isNull(mVertSnapLine))
				snapXCoord(mSnappedCenter.x(), mSnappedCenter.y(), mSnappedCenter.y(), false, &mVertSnapLine);

			// корректируем координаты горизонтальной линии привязки
			if (!Utils::isNull(mHorzSnapLine))
				snapYCoord(mSnappedCenter.y(), mSnappedCenter.x(), mSnappedCenter.x(), false, &mHorzSnapLine);
		}
		else if (mEditorState == STATE_HORZ_GUIDE)
		{
			// перемещаем горизонтальную направляющую
			qreal snappedY = pos.y();
			if (Options::getSingleton().isEnableSmartGuides())
			{
				const qreal MAX_COORD = 1.0E+8;
				qreal distance = SNAP_DISTANCE / mZoom;
				mHorzSnapLine = mVertSnapLine = QLineF();
				mScene->getRootLayer()->snapYCoord(pos.y(), MAX_COORD, -MAX_COORD, QList<GameObject *>(), snappedY, distance, mHorzSnapLine);
			}
			mScene->setGuide(true, mGuideIndex, qRound(snappedY));
		}
		else if (mEditorState == STATE_VERT_GUIDE)
		{
			// перемещаем вертикальную направляющую
			qreal snappedX = pos.x();
			if (Options::getSingleton().isEnableSmartGuides())
			{
				const qreal MAX_COORD = 1.0E+8;
				qreal distance = SNAP_DISTANCE / mZoom;
				mHorzSnapLine = mVertSnapLine = QLineF();
				mScene->getRootLayer()->snapXCoord(pos.x(), MAX_COORD, -MAX_COORD, QList<GameObject *>(), snappedX, distance, mVertSnapLine);
			}
			mScene->setGuide(false, mGuideIndex, qRound(snappedX));
		}

		// отправляем сигнал об изменении объектов
		if (mEditorState == STATE_MOVE || mEditorState == STATE_RESIZE || mEditorState == STATE_ROTATE || mEditorState == STATE_MOVE_CENTER)
			emit objectsChanged(mSelectedObjects, mSnappedCenter);
	}

	// обновляем курсор мыши
	updateMouseCursor(pos);

	// отправляем сигнал об изменении координат мыши
	emit mouseMoved(pos);
}

void EditorWindow::wheelEvent(QWheelEvent *event)
{
	const qreal SCROLL_SPEED = 16.0;
	if ((event->modifiers() & Qt::AltModifier) != 0)
	{
		// рассчитываем новый зум
		qreal zoom = qBound(0.1, mZoom * qPow(1.1, event->delta() / 120.0), 32.0);

		// применяем новый зум, чтобы объектное положение мышки не изменилось
		QPointF mousePos = event->pos();
		mCameraPos = mCameraPos + mousePos / mZoom - mousePos / zoom;
		mZoom = zoom;

		// отправляем сигнал об изменении масштаба
		QString zoomStr = QString::number(qRound(mZoom * 100.0)) + '%';
		emit zoomChanged(zoomStr);
	}
	else
	{
		// перемещаем камеру по горизонтали или вертикали
		Qt::Orientation orientation = event->orientation();
		if ((event->modifiers() & Qt::ControlModifier) != 0)
			orientation = orientation == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal;
		if (orientation == Qt::Horizontal)
			mCameraPos.rx() -= (event->delta() / 120.0) * SCROLL_SPEED / mZoom;
		else
			mCameraPos.ry() -= (event->delta() / 120.0) * SCROLL_SPEED / mZoom;

		// обновляем курсор мыши и посылаем сигнал об изменении координат
		QPointF pos = windowToWorld(event->pos());
		updateMouseCursor(pos);
		emit mouseMoved(pos);
	}
}

void EditorWindow::keyPressEvent(QKeyEvent *event)
{
	// стрелки клавиатуры
	if ((event->key() == Qt::Key_Left || event->key() == Qt::Key_Right || event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
		&& mMoveEnabled && !mSelectedObjects.empty() && mEditorState == STATE_IDLE)
	{
		// выбираем направление перемещения
		QPointF offset;
		if (event->key() == Qt::Key_Left)
			offset = QPointF(-1.0, 0.0);
		else if (event->key() == Qt::Key_Right)
			offset = QPointF(1.0, 0.0);
		else if (event->key() == Qt::Key_Up)
			offset = QPointF(0.0, -1.0);
		else
			offset = QPointF(0.0, 1.0);

		// перемещаем все выделенные объекты
		for (int i = 0; i < mSelectedObjects.size(); ++i)
			mSelectedObjects[i]->setPosition(mOriginalPositions[i] + offset);

		// перемещаем центр вращения
		mOriginalCenter = mSnappedCenter = mOriginalCenter + offset;

		// обновляем выделение и курсор мыши
		selectGameObjects(mSelectedObjects);
		updateMouseCursor(windowToWorld(mapFromGlobal(QCursor::pos())));

		// отправляем сигнал об изменении объектов
		emit objectsChanged(mSelectedObjects, mSnappedCenter);
	}
	else if (event->key() == Qt::Key_Shift && !event->isAutoRepeat() && mEditorState != STATE_IDLE)
	{
		// генерируем событие перемещения мыши при нажатии Shift
		QMouseEvent event(QEvent::MouseMove, mapFromGlobal(QCursor::pos()), Qt::NoButton, Qt::LeftButton, Qt::ShiftModifier);
		mouseMoveEvent(&event);
	}
	else
	{
		QGLWidget::keyPressEvent(event);
	}
}

void EditorWindow::keyReleaseEvent(QKeyEvent *event)
{
	if ((event->key() == Qt::Key_Left || event->key() == Qt::Key_Right || event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
		&& !event->isAutoRepeat() && mMoveEnabled && !mSelectedObjects.empty() && mEditorState == STATE_IDLE)
	{
		// посылаем сигналы об изменении сцены и слоев
		emitSceneAndLayerChangedSignals(getParentLayers(), "Перемещение объектов");
	}
	else if (event->key() == Qt::Key_Shift && !event->isAutoRepeat() && mEditorState != STATE_IDLE)
	{
		// генерируем событие перемещения мыши при отпускании Shift
		QMouseEvent event(QEvent::MouseMove, mapFromGlobal(QCursor::pos()), Qt::NoButton, Qt::LeftButton, 0);
		mouseMoveEvent(&event);
	}
	else
	{
		QGLWidget::keyReleaseEvent(event);
	}
}

void EditorWindow::dragEnterEvent(QDragEnterEvent *event)
{
	// выходим, если текущий активный слой недоступен
	if (mScene->getAvailableLayer() == NULL)
		return;

	// проверяем, что перетаскивается элемент из браузера спрайтов или шрифтов
	if (event->source() == mSpriteWidget || event->source() == mFontWidget)
		event->acceptProposedAction();
}

void EditorWindow::dropEvent(QDropEvent *event)
{
	// выходим, если текущий активный слой недоступен
	if (mScene->getAvailableLayer() == NULL)
		return;

	// получаем полный путь к ресурсному файлу
	QString path;
	if (event->source() == mSpriteWidget)
	{
		// получаем MIME-закодированные данные для перетаскиваемого элемента
		QByteArray data = event->mimeData()->data("application/x-qabstractitemmodeldatalist");
		if (data.isEmpty())
			return;

		// распаковываем полученные данные
		int row, col;
		QMap<int, QVariant> roles;
		QDataStream stream(data);
		stream >> row >> col >> roles;

		// извлекаем путь к ресурсному файлу из UserRole
		path = roles.value(Qt::UserRole).toString();
	}
	else if (event->source() == mFontWidget)
	{
		// получаем список URL для перетаскиваемого элемента
		QList<QUrl> urls = event->mimeData()->urls();
		if (urls.empty())
			return;

		// извлекаем путь к ресурсному файлу из первого URL в списке
		path = urls.front().toLocalFile();
	}
	else
	{
		return;
	}

	// проверяем путь к файлу на валидность и определяем имя файла относительно корневого каталога
	QString rootDirectory = Project::getSingleton().getRootDirectory();
	if (!Utils::isFileNameValid(path, rootDirectory, this))
		return;
	QString fileName = path.mid(rootDirectory.size());

	// создаем объект нужного типа в окне редактора
	GameObject *object;
	QPointF pos = windowToWorld(event->pos());
	if (event->source() == mSpriteWidget)
		object = mScene->createSprite(pos, fileName);
	else
		object = mScene->createLabel(pos, fileName, 32);

	// привязываем объект к сетке, если необходимо
	if (Options::getSingleton().isSnapToGrid())
	{
		QPointF position = object->getPosition();
		int gridSpacing = getGridSpacing();
		position.setX(qFloor((position.x() + gridSpacing / 2.0) / gridSpacing) * gridSpacing);
		position.setY(qFloor((position.y() + gridSpacing / 2.0) / gridSpacing) * gridSpacing);
		object->setPosition(position);
	}

	// выделяем созданный объект
	selectGameObject(object);

	// обновляем курсор мыши и посылаем сигналы об изменении сцены и слоев
	updateMouseCursor(pos);
	emitSceneAndLayerChangedSignals(getParentLayers(), "Создание объекта");

	// устанавливаем фокус на окне редактирования
	setFocus();

	// принимаем перетаскивание
	event->acceptProposedAction();
}

QPointF EditorWindow::worldToWindow(const QPointF &pt) const
{
	return (pt - mCameraPos) * mZoom;
}

QPointF EditorWindow::windowToWorld(const QPointF &pt) const
{
	return mCameraPos + pt / mZoom;
}

QRectF EditorWindow::worldRectToWindow(const QRectF &rect) const
{
	QPointF topLeft = worldToWindow(rect.topLeft());
	QPointF bottomRight = worldToWindow(rect.bottomRight());

	// округляем координаты в меньшую или большую сторону в соответствии с правилами растеризации полигонов
	topLeft.rx() = topLeft.x() <= qFloor(topLeft.x()) + 0.5 ? qFloor(topLeft.x()) : qFloor(topLeft.x()) + 1.0;
	topLeft.ry() = topLeft.y() <= qFloor(topLeft.y()) + 0.5 ? qFloor(topLeft.y()) : qFloor(topLeft.y()) + 1.0;
	bottomRight.rx() = bottomRight.x() < qFloor(bottomRight.x()) + 0.5 ? qFloor(bottomRight.x()) : qFloor(bottomRight.x()) + 1.0;
	bottomRight.ry() = bottomRight.y() < qFloor(bottomRight.y()) + 0.5 ? qFloor(bottomRight.y()) : qFloor(bottomRight.y()) + 1.0;

	return QRectF(topLeft, bottomRight);
}

int EditorWindow::getGridSpacing() const
{
	// подбираем подходящий шаг сетки
	int gridSpacing = Options::getSingleton().getGridSpacing();
	if (Options::getSingleton().isSnapToVisibleLines())
	{
		while (gridSpacing * mZoom < MIN_GRID_SPACING)
			gridSpacing *= GRID_SPACING_COEFF;
	}
	return gridSpacing;
}

qreal EditorWindow::snapXCoord(qreal x, qreal y1, qreal y2, bool excludeSelection, QLineF *linePtr, qreal *distancePtr)
{
	Options &options = Options::getSingleton();

	qreal snappedX = x;
	qreal distance = SNAP_DISTANCE / mZoom;
	QLineF line;

	// привязываем координату к направляющим
	if (options.isSnapToGuides())
	{
		// привязываем координату к обычным направляющим
		int index = mScene->findGuide(false, x, distance);
		if (index != -1)
		{
			snappedX = mScene->getGuide(false, index);
			line = QLineF(snappedX, y1, snappedX, y2);
		}

		// привязываем координату к оси X
		if (qAbs(x) < distance)
		{
			distance = qAbs(x);
			snappedX = 0.0;
			line = QLineF(0.0, y1, 0.0, y2);
		}
	}

	// привязываем координату к умным направляющим
	if (options.isEnableSmartGuides())
	{
		const QList<GameObject *> &excludedObjects = excludeSelection ? mSelectedObjects : QList<GameObject *>();
		mScene->getRootLayer()->snapXCoord(x, y1, y2, excludedObjects, snappedX, distance, line);
	}

	// привязываем координату к сетке
	if (options.isSnapToGrid() && distance == SNAP_DISTANCE / mZoom)
	{
		// округляем координату до выбранного шага сетки
		int gridSpacing = getGridSpacing();
		snappedX = qFloor((x + gridSpacing / 2.0) / gridSpacing) * gridSpacing;
	}

	// возвращаем полученные значения
	if (linePtr != NULL)
		*linePtr = line;
	if (distancePtr != NULL)
		*distancePtr = distance;

	return snappedX;
}

qreal EditorWindow::snapYCoord(qreal y, qreal x1, qreal x2, bool excludeSelection, QLineF *linePtr, qreal *distancePtr)
{
	Options &options = Options::getSingleton();

	qreal snappedY = y;
	qreal distance = SNAP_DISTANCE / mZoom;
	QLineF line;

	// привязываем координату к направляющим
	if (options.isSnapToGuides())
	{
		// привязываем координату к обычным направляющим
		int index = mScene->findGuide(true, y, distance);
		if (index != -1)
		{
			snappedY = mScene->getGuide(true, index);
			line = QLineF(x1, snappedY, x2, snappedY);
		}

		// привязываем координату к оси Y
		if (qAbs(y) < distance)
		{
			distance = qAbs(y);
			snappedY = 0.0;
			line = QLineF(x1, 0.0, x2, 0.0);
		}
	}

	// привязываем координату к умным направляющим
	if (options.isEnableSmartGuides())
	{
		const QList<GameObject *> &excludedObjects = excludeSelection ? mSelectedObjects : QList<GameObject *>();
		mScene->getRootLayer()->snapYCoord(y, x1, x2, excludedObjects, snappedY, distance, line);
	}

	// привязываем координату к сетке
	if (options.isSnapToGrid() && distance == SNAP_DISTANCE / mZoom)
	{
		// округляем координату до выбранного шага сетки
		int gridSpacing = getGridSpacing();
		snappedY = qFloor((y + gridSpacing / 2.0) / gridSpacing) * gridSpacing;
	}

	// возвращаем полученные значения
	if (linePtr != NULL)
		*linePtr = line;
	if (distancePtr != NULL)
		*distancePtr = distance;

	return snappedY;
}

QPointF EditorWindow::calcTranslation(const QPointF &direction, bool shift, QVector2D &axis)
{
	if (shift)
	{
		// определяем угол между исходным вектором и осью OX в радианах
		QVector2D vector(direction);
		qreal angle = qAcos(vector.normalized().x());
		if (QString::number(angle) == "nan" || Utils::isEqual(angle, 0.0))
			angle = 0.0;

		// приводим угол к диапазону [-PI; PI]
		if (vector.y() < 0.0)
			angle = -angle;

		// округляем угол до величины, кратной 45 градусам
		angle = qFloor((angle + Utils::PI / 8.0) / (Utils::PI / 4.0)) * (Utils::PI / 4.0);

		// получаем ось перемещения и проецируем на нее исходный вектор
		axis = QVector2D(qCos(angle), qSin(angle));
		return (QVector2D::dotProduct(vector, axis) * axis).toPointF();
	}
	else
	{
		// возвращаем исходный вектор перемещения
		axis = QVector2D(direction).normalized();
		return direction;
	}
}

QPointF EditorWindow::calcScale(const QPointF &pos, const QPointF &pivot, qreal sx, qreal sy, bool keepProportions)
{
	// привязываем точку к осям X и Y и вычисляем масштаб
	qreal snappedX = snapXCoord(pos.x(), qMin(pos.y(), pivot.y()), qMax(pos.y(), pivot.y()), true, &mVertSnapLine);
	qreal snappedY = snapYCoord(pos.y(), qMin(pos.x(), pivot.x()), qMax(pos.x(), pivot.x()), true, &mHorzSnapLine);
	QPointF scale((snappedX - pivot.x()) * sx / mOriginalRect.width(), (snappedY - pivot.y()) * sy / mOriginalRect.height());

	// проверяем флаг сохранения пропорций
	if (keepProportions)
	{
		// при пропорциональном масштабировании привязываем точку только по оси с наименьшим масштабом
		if (qAbs(scale.x()) < qAbs(scale.y()))
		{
			scale.ry() = Utils::sign(scale.y()) * qAbs(scale.x());
			qreal y = mOriginalRect.height() * scale.y() * sy + pivot.y();
			snapXCoord(snappedX, qMin(y, pivot.y()), qMax(y, pivot.y()), true, &mVertSnapLine);
			mHorzSnapLine = QLineF();
		}
		else
		{
			scale.rx() = Utils::sign(scale.x()) * qAbs(scale.y());
			qreal x = mOriginalRect.width() * scale.x() * sx + pivot.x();
			snapYCoord(snappedY, qMin(x, pivot.x()), qMax(x, pivot.x()), true, &mHorzSnapLine);
			mVertSnapLine = QLineF();
		}
	}
	else
	{
		// корректируем координаты вертикальной линии привязки
		if (!Utils::isNull(mVertSnapLine))
			snapXCoord(snappedX, qMin(snappedY, pivot.y()), qMax(snappedY, pivot.y()), true, &mVertSnapLine);

		// корректируем координаты горизонтальной линии привязки
		if (!Utils::isNull(mHorzSnapLine))
			snapYCoord(snappedY, qMin(snappedX, pivot.x()), qMax(snappedX, pivot.x()), true, &mHorzSnapLine);
	}

	return scale;
}

void EditorWindow::selectGameObject(GameObject *object)
{
	QList<GameObject *> objects;
	objects.push_back(object);
	selectGameObjects(objects);
}

void EditorWindow::selectGameObjects(const QList<GameObject *> &objects)
{
	// находим общий ограничивающий прямоугольник для всех выделенных объектов
	if (!objects.empty())
	{
		mOriginalPositions.clear();
		mOriginalSizes.clear();
		mOriginalAngles.clear();
		mOriginalRect = objects.front()->getBoundingRect();
		mKeepProportions = false;
		foreach (GameObject *object, objects)
		{
			qreal angle = object->getRotationAngle();
			if (angle != 0.0 && angle != 90.0 && angle != 180.0 && angle != 270.0)
				mKeepProportions = true;
			mOriginalPositions.push_back(object->getPosition());
			mOriginalSizes.push_back(object->getSize());
			mOriginalAngles.push_back(angle);
			mOriginalRect |= object->getBoundingRect();
		}
		mSnappedRect = mOriginalRect;
	}

	// проверяем на изменение выделения
	if (objects != mSelectedObjects)
	{
		// сохраняем список выделенных объектов
		mSelectedObjects = objects;

		// обновляем координаты центра вращения
		if (!mSelectedObjects.empty())
			mOriginalCenter = mSnappedCenter = mSelectedObjects.size() == 1 ? mSelectedObjects.front()->getRotationCenter() : mOriginalRect.center();

		// обновляем разрешенные операции редактирования
		updateAllowedEditorActions();

		// посылаем сигнал об изменении выделения
		emit selectionChanged(mSelectedObjects, mSnappedCenter);
	}
}

void EditorWindow::sortSelectedGameObjects()
{
	// сортируем выделенные объекты по глубине и пересоздаем списки исходных позиций, размеров и углов
	if (!mSelectedObjects.empty())
	{
		mSelectedObjects = mScene->getRootLayer()->sortGameObjects(mSelectedObjects);
		mOriginalPositions.clear();
		mOriginalSizes.clear();
		mOriginalAngles.clear();
		foreach (GameObject *object, mSelectedObjects)
		{
			mOriginalPositions.push_back(object->getPosition());
			mOriginalSizes.push_back(object->getSize());
			mOriginalAngles.push_back(object->getRotationAngle());
		}
	}
}

bool EditorWindow::canDeleteSelectedObjects()
{
	if (!mSelectedObjects.empty() && !mUntitled)
	{
		// определяем имя файла имен
		Project &project = Project::getSingleton();
		QString scenesDir = project.getRootDirectory() + project.getScenesDirectory();
		QString namesDir = project.getRootDirectory() + project.getNamesDirectory();
		QString fileName = namesDir + mFileName.mid(scenesDir.size());

		// выдаем предупреждение, если хотя бы один из выделенных объектов используется в файле имен
		QList<GameObject *> usedObjects = mScene->findUsedGameObjects(fileName, mSelectedObjects);
		if (!usedObjects.empty())
		{
			QMessageBox::warning(this, "", "Невозможно удалить выделенные объекты, поскольку объект с именем \""
				+ usedObjects.front()->getName() + "\" используется в программе");
			return false;
		}
	}

	return true;
}

void EditorWindow::updateMouseCursor(const QPointF &pos)
{
	if (mEditorState == STATE_IDLE)
	{
		bool showGuides = Options::getSingleton().isShowGuides();
		qreal distance = GUIDE_DISTANCE / mZoom;
		qreal size = CENTER_SIZE / mZoom;
		qreal offset = size / 2.0;
		SelectionMarker marker;
		GameObject *object;

		if (showGuides && ((pos.x() < mCameraPos.x() + RULER_SIZE / mZoom) || (pos.y() < mCameraPos.y() + RULER_SIZE / mZoom)))
		{
			// курсор над линейками
			setCursor(Qt::ArrowCursor);
		}
		else if (showGuides && mScene->findGuide(true, pos.y(), distance) != -1)
		{
			// курсор над горизонтальной направляющей
			setCursor(Qt::SplitVCursor);
		}
		else if (showGuides && mScene->findGuide(false, pos.x(), distance) != -1)
		{
			// курсор над вертикальной направляющей
			setCursor(Qt::SplitHCursor);
		}
		else if (mMoveCenterEnabled && !mSelectedObjects.empty() && QRectF(mSnappedCenter.x() - offset, mSnappedCenter.y() - offset, size, size).contains(pos))
		{
			// курсор над центром вращения
			setCursor(Qt::CrossCursor);
		}
		else if (mResizeEnabled && (marker = findSelectionMarker(pos, MARKER_SIZE / mZoom)) != MARKER_NONE)
		{
			// курсор над маркером выделения
			static const Qt::CursorShape MARKER_CURSORS[] = {Qt::SizeFDiagCursor, Qt::SizeHorCursor, Qt::SizeBDiagCursor, Qt::SizeVerCursor,
				Qt::SizeFDiagCursor, Qt::SizeHorCursor, Qt::SizeBDiagCursor, Qt::SizeVerCursor};
			setCursor(MARKER_CURSORS[marker - 1]);
		}
		else if (mRotationEnabled && findSelectionMarker(pos, ROTATE_SIZE / mZoom) != MARKER_NONE && findSelectionMarker(pos, MARKER_SIZE / mZoom) == MARKER_NONE)
		{
			// курсор рядом с маркером выделения
			setCursor(mRotateCursor);
		}
		else if ((object = mScene->getRootLayer()->findGameObjectByPoint(pos)) != NULL
			&& (mSelectedObjects.contains(object) ? mMoveEnabled : object->isLocalized()))
		{
			// курсор над игровым объектом
			setCursor(Qt::OpenHandCursor);
		}
		else
		{
			// курсор над свободным местом
			setCursor(Qt::ArrowCursor);
		}
	}
}

QSet<BaseLayer *> EditorWindow::getParentLayers() const
{
	// формируем список родительских слоев
	QSet<BaseLayer *> layers;
	foreach (GameObject *object, mSelectedObjects)
		layers |= object->getParentLayer();
	return layers;
}

void EditorWindow::emitSceneAndLayerChangedSignals(const QSet<BaseLayer *> &layers, const QString &commandName)
{
	// посылаем сигналы об изменении сцены и слоев
	emit sceneChanged(commandName);
	foreach (BaseLayer *layer, layers)
		emit layerChanged(mScene, layer);
}

EditorWindow::SelectionMarker EditorWindow::findSelectionMarker(const QPointF &pos, qreal size) const
{
	if (!mSelectedObjects.empty())
	{
		qreal offset = size / 2.0;
		QPointF center = mSnappedRect.center();

		if (QRectF(mSnappedRect.left() - offset, mSnappedRect.top() - offset, size, size).contains(pos))
			return MARKER_TOP_LEFT;
		if (QRectF(mSnappedRect.left() - offset, center.y() - offset, size, size).contains(pos))
			return MARKER_CENTER_LEFT;
		if (QRectF(mSnappedRect.left() - offset, mSnappedRect.bottom() - offset, size, size).contains(pos))
			return MARKER_BOTTOM_LEFT;
		if (QRectF(center.x() - offset, mSnappedRect.bottom() - offset, size, size).contains(pos))
			return MARKER_BOTTOM_CENTER;
		if (QRectF(mSnappedRect.right() - offset, mSnappedRect.bottom() - offset, size, size).contains(pos))
			return MARKER_BOTTOM_RIGHT;
		if (QRectF(mSnappedRect.right() - offset, center.y() - offset, size, size).contains(pos))
			return MARKER_CENTER_RIGHT;
		if (QRectF(mSnappedRect.right() - offset, mSnappedRect.top() - offset, size, size).contains(pos))
			return MARKER_TOP_RIGHT;
		if (QRectF(center.x() - offset, mSnappedRect.top() - offset, size, size).contains(pos))
			return MARKER_TOP_CENTER;
	}

	return MARKER_NONE;
}

void EditorWindow::drawSelectionMarker(qreal x, qreal y, QPainter &painter)
{
	// рисуем круглый или прямоугольный маркер выделения
	qreal offset = qFloor(MARKER_SIZE / 2.0);
	painter.drawRect(QRectF(x - offset, y - offset, MARKER_SIZE - 1.0, MARKER_SIZE - 1.0));
}

bool EditorWindow::isShowSnapLines() const
{
	return mEditorState == STATE_MOVE || mEditorState == STATE_RESIZE || mEditorState == STATE_MOVE_CENTER
		|| mEditorState == STATE_HORZ_GUIDE || mEditorState == STATE_VERT_GUIDE;
}

void EditorWindow::drawSnapLine(const QLineF &line, QPainter &painter)
{
	// рисуем линию
	QPointF p1 = worldToWindow(line.p1()), p2 = worldToWindow(line.p2());
	p1 = QPointF(qFloor(p1.x()) + 0.5, qFloor(p1.y()) + 0.5);
	p2 = QPointF(qFloor(p2.x()) + 0.5, qFloor(p2.y()) + 0.5);
	painter.drawLine(p1, p2);

	// рисуем крестик в начале линии
	qreal offset = qFloor(CENTER_SIZE / 2.0);
	painter.drawLine(QPointF(p1.x() - offset, p1.y() - offset), QPointF(p1.x() + offset, p1.y() + offset));
	painter.drawLine(QPointF(p1.x() - offset, p1.y() + offset), QPointF(p1.x() + offset, p1.y() - offset));

	// рисуем крестик в конце линии
	painter.drawLine(QPointF(p2.x() - offset, p2.y() - offset), QPointF(p2.x() + offset, p2.y() + offset));
	painter.drawLine(QPointF(p2.x() - offset, p2.y() + offset), QPointF(p2.x() + offset, p2.y() - offset));
}
