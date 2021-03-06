#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "ui_main_window.h"

class EditorWindow;
class FontBrowser;
class GameObject;
class HistoryWindow;
class LayersWindow;
class PropertyWindow;
class SpriteBrowser;
class Texture;

// Класс главного окна приложения
class MainWindow : public QMainWindow, private Ui::MainWindow
{
	Q_OBJECT

public:

	// Конструктор
	MainWindow();

	// Деструктор
	virtual ~MainWindow();

	// Возвращает текущее окно редактирования
	EditorWindow *getCurrentEditorWindow() const;

	// Возвращает окно редактирования с заданным индексом
	EditorWindow *getEditorWindow(int index) const;

protected:

	// Вызывается при получении события
	virtual bool event(QEvent *event);

	// Вызывается по срабатыванию таймера
	virtual void timerEvent(QTimerEvent *event);

	// Вызывается при попытке закрытия окна
	virtual void closeEvent(QCloseEvent *event);

private slots:

	// Обработчик пункта меню Файл-Создать
	void on_mNewAction_triggered();

	// Обработчик пункта меню Файл-Открыть
	void on_mOpenAction_triggered();

	// Обработчик пункта меню Файл-Сохранить
	bool on_mSaveAction_triggered();

	// Обработчик пункта меню Файл-Сохранить как
	bool on_mSaveAsAction_triggered();

	// Обработчик пункта меню Файл-Закрыть
	bool on_mCloseAction_triggered();

	// Обработчик пункта меню Файл-Закрыть все
	bool on_mCloseAllAction_triggered();

	// Обработчик пункта меню Правка-Отменить
	void on_mUndoAction_triggered();

	// Обработчик пункта меню Правка-Повторить
	void on_mRedoAction_triggered();

	// Обработчик пункта меню Правка-Вырезать
	void on_mCutAction_triggered();

	// Обработчик пункта меню Правка-Копировать
	void on_mCopyAction_triggered();

	// Обработчик пункта меню Правка-Вставить
	void on_mPasteAction_triggered();

	// Обработчик пункта меню Правка-Удалить
	void on_mDeleteAction_triggered();

	// Обработчик пункта меню Правка-Настройки
	void on_mOptionsAction_triggered();

	// Обработчик пункта меню Правка-Выделить все
	void on_mSelectAllAction_triggered();

	// Обработчик пункта меню Вид-Увеличить
	void on_mZoomInAction_triggered();

	// Обработчик пункта меню Вид-Уменьшить
	void on_mZoomOutAction_triggered();

	// Обработчик пункта меню Вид-Показывать сетку
	void on_mShowGridAction_triggered(bool checked);

	// Обработчик пункта меню Вид-Привязывать к сетке
	void on_mSnapToGridAction_triggered(bool checked);

	// Обработчик пункта меню Вид-Показывать направляющие
	void on_mShowGuidesAction_triggered(bool checked);

	// Обработчик пункта меню Вид-Привязывать к направляющим
	void on_mSnapToGuidesAction_triggered(bool checked);

	// Обработчик пункта меню Вид-Умные направляющие
	void on_mEnableSmartGuidesAction_triggered(bool checked);

	// Обработчик пункта меню Объект-На передний план
	void on_mBringToFrontAction_triggered();

	// Обработчик пункта меню Объект-На задний план
	void on_mSendToBackAction_triggered();

	// Обработчик пункта меню Объект-Переместить вверх
	void on_mMoveUpAction_triggered();

	// Обработчик пункта меню Объект-Переместить вниз
	void on_mMoveDownAction_triggered();

	// Обработчик закрытия вкладки
	bool on_mTabWidget_tabCloseRequested(int index);

	// Обработчик смены вкладки
	void on_mTabWidget_currentChanged(int index);

	// Обработчик изменения масштаба
	void onZoomChanged(const QString &zoomStr);

	// Редактирование масштаба вручную закончено в ComboBox
	void onZoomEditingFinished();

	// Обработчик изменения текущего языка
	void onLanguageChanged(const QString &language);

	// Обработчик изменения файла переводов
	void onTranslationFileChanged(const QString &path);

	// Обработчик изменения сцены
	void onSceneChanged(const QString &commandName);

	// Обработчик изменения выделения в окне редактирования
	void onEditorWindowSelectionChanged(const QList<GameObject *> &objects, const QPointF &rotationCenter);

	// Обработчик изменения положения курсора мышки на OpenGL окне
	void onEditorWindowMouseMoved(const QPointF &pos);

	// Обработчик изменения текущей команды в стеке отмен
	void onEditorWindowUndoCommandChanged();

	// Обработчик изменения слоя в окне слоёв
	void onLayerWindowLayerChanged();

	// Обработчик изменения свойств объектов в окне свойств
	void onPropertyWindowObjectsChanged(const QPointF &rotationCenter);

	// Обработчик изменения разрешенных операций редактирования в окне свойств
	void onPropertyWindowAllowedEditorActionsChanged();

	// Обработчик сигнала изменения текстуры от текстурного менеджера
	void onTextureChanged(const QString &fileName, const QSharedPointer<Texture> &texture);

	// Обработчик изменения содержимого буфера обмена
	void onClipboardDataChanged();

private:

	// Структура с информацией о файле переводов
	struct TranslationFileInfo
	{
		// Конструктор
		TranslationFileInfo(EditorWindow *editorWindow)
		: mEditorWindow(editorWindow), mChanged(false)
		{
		}

		EditorWindow    *mEditorWindow;     // Указатель на соответствующее окно редактирования
		bool            mChanged;           // Флаг изменения файла переводов
		QElapsedTimer   mTimer;             // Таймер для отсчета времени с момента последнего изменения файла переводов
	};

	// Тип для списка файлов переводов
	typedef QMap<QString, TranslationFileInfo> TranslationFilesMap;

	// Валидатор формата int с процентом на конце
	class PercentIntValidator : public QIntValidator
	{
	public:

		// Конструктор
		PercentIntValidator(int bottom, int top, MainWindow *parent);

		// Вызывается для отката неверного значения
		virtual void fixup(QString &input) const;

		// Вызывается для проверки введенного значения
		virtual QValidator::State validate(QString &input, int &pos) const;

	private:

		// Указатель на родительский класс
		MainWindow *mParent;
	};

	// Создает новое окно редактирования
	EditorWindow *createEditorWindow(const QString &fileName);

	// Определяет имя файла переводов по имени файла сцены
	QString getTranslationFileName(const QString &fileName) const;

	// Обновляет состояния пунктов главного меню
	void updateMainMenuActions();

	// Обновляет пункты меню с последними файлами
	void updateRecentFilesActions(const QString &fileName);

	// Обновляет пункты меню Undo/Redo и звездочку в имени вкладки
	void updateUndoRedoActions();

	// Проверяет текущую сцену на наличие отсутствующих файлов
	void checkMissedFiles();

	SpriteBrowser       *mSpriteBrowser;            // Браузер спрайтов
	FontBrowser         *mFontBrowser;              // Браузер шрифтов
	PropertyWindow      *mPropertyWindow;           // Окно свойств объекта
	LayersWindow        *mLayersWindow;             // Окно слоев
	HistoryWindow       *mHistoryWindow;            // Окно истории

	bool                mRenderEnabled;             // Флаг разрешения отрисовки по таймеру
	int                 mUntitledIndex;             // Текущий номер для новых файлов
	int                 mTabWidgetCurrentIndex;     // Текущий индекс вкладки

	QGLWidget           *mPrimaryGLWidget;          // OpenGL виджет для загрузки текстур в главном потоке
	QGLWidget           *mSecondaryGLWidget;        // OpenGL виджет для загрузки текстур в фоновом потоке
	QLabel              *mMousePosLabel;            // Текстовое поле для координат мыши в строке статуса
	QComboBox           *mZoomComboBox;             // Выпадающий список масштабов
	QList<qreal>        mZoomList;                  // Список масштабов
	QList<QAction *>    mRecentFilesActions;        // Список пунктов меню с последними файлами
	QAction             *mRecentFilesSeparator;     // Разделитель для пунктов меню с последними файлами

	QFileSystemWatcher  *mTranslationFilesWatcher;  // Объект слежения за файлами переводов
	TranslationFilesMap mTranslationFilesMap;       // Список файлов переводов, поставленных на слежение
	int                 mTranslationCounter;        // Счетчик для перезагрузки файлов переводов по таймеру
};

#endif // MAIN_WINDOW_H
