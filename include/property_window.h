#ifndef PROPERTY_WINDOW_H
#define PROPERTY_WINDOW_H

#include "ui_property_window.h"
#include "label.h"

class BaseLayer;

class PropertyWindow : public QDockWidget, private Ui::PropertyWindow
{
	Q_OBJECT

public:

	// Конструктор
	PropertyWindow(QWidget *parent);

	// Сброс фокуса в окне свойств
	void clearChildWidgetFocus();

signals:

	// Сигнал об изменении сцены
	void sceneChanged(const QString &commandName);

	// Сигнал об изменении слоя
	void layerChanged(BaseLayer *layer);

	// Сигнал об изменении позиции, размера, угла поворота, центра вращения
	void objectsChanged(const QPointF &rotationCenter);

	// Сигнал об изменении разрешенных операций редактирования
	void allowedEditorActionsChanged();

public slots:

	// Обработчик изменения выделения объектов
	void onEditorWindowSelectionChanged(const QList<GameObject *> &objects, const QPointF &rotationCenter);

	// Обработчик изменения общих свойств объектов
	void onEditorWindowObjectsChanged(const QList<GameObject *> &objects, const QPointF &rotationCenter);

protected:

	// Фильтр событий для QFrame с цветами
	virtual bool eventFilter(QObject *object, QEvent *event);

private slots:

	void on_mNameLineEdit_editingFinished();

	void on_mCopyIDPushButton_clicked();

	void on_mPositionXLineEdit_editingFinished();

	void on_mPositionYLineEdit_editingFinished();

	void on_mSizeWLineEdit_editingFinished();

	void on_mSizeHLineEdit_editingFinished();

	void on_mLockSizePushButton_clicked();

	void on_mFlipXCheckBox_clicked(bool checked);

	void on_mFlipYCheckBox_clicked(bool checked);

	void onRotationAngleEditingFinished();

	void on_mRotationAngleComboBox_activated(const QString &text);

	void on_mRotationCenterXLineEdit_editingFinished();

	void on_mRotationCenterYLineEdit_editingFinished();

	void on_mResetRotationCenterPushButton_clicked();

	void on_mSpriteFileNameBrowsePushButton_clicked();

	void on_mSpriteOpacitySlider_sliderMoved(int value);

	void on_mSpriteOpacitySlider_valueChanged(int value);

	void on_mTextLineEdit_editingFinished();

	void on_mTextEditPushButton_clicked();

	void onPlainTextEditTextChanged();

	void on_mLabelFileNameComboBox_activated(const QString &text);

	void onFontSizeEditingFinished();

	void on_mFontSizeComboBox_activated(const QString &text);

	void onHorzAlignmentClicked(QAbstractButton *button);

	void onVertAlignmentClicked(QAbstractButton *button);

	void onLineSpacingEditingFinished();

	void on_mLineSpacingComboBox_activated(const QString &text);

	void on_mLabelOpacitySlider_sliderMoved(int value);

	void on_mLabelOpacitySlider_valueChanged(int value);

	void on_mLocalizationPushButton_clicked();

private:

	// Тип указателя на метод для отката неверно введенного значения
	typedef QString (PropertyWindow::*FixupFunc)() const;

	// Валидатор для целочисленных свойств
	class PropertyIntValidator : public QIntValidator
	{
	public:

		// Конструктор
		PropertyIntValidator(int bottom, int top, PropertyWindow *parent, FixupFunc fixupFunc);

		// Вызывается для отката неверного значения
		virtual void fixup(QString &input) const;

	private:

		PropertyWindow  *mParent;       // Указатель на родительский класс
		FixupFunc       mFixupFunc;     // Указатель на метод для отката неверно введенного значения
	};

	// Валидатор для вещественных свойств
	class PropertyDoubleValidator : public QDoubleValidator
	{
	public:

		// Конструктор
		PropertyDoubleValidator(double bottom, double top, int decimals, PropertyWindow *parent, FixupFunc fixupFunc);

		// Вызывается для отката неверного значения
		virtual void fixup(QString &input) const;

	private:

		PropertyWindow  *mParent;       // Указатель на родительский класс
		FixupFunc       mFixupFunc;     // Указатель на метод для отката неверно введенного значения
	};

	// Точность представления вещественных свойств
	static const int PRECISION = 8;

	// логика показа свойств выделенных объектов
	void updateWidgetsVisibleAndEnabled();

	// Устанавливает видимость для заданного диапазона строк в грид лейауте
	void setGridLayoutRowsVisible(QGridLayout *layout, int firstRow, int numRows, bool visible);

	// Устанавливает видимость для элемента лейаута
	void setLayoutItemVisible(QLayoutItem *item, bool visible);

	// Активирует/деактивирует элемент для заданного диапазона строк в грид лейауте
	void setGridLayoutRowsEnabled(QGridLayout *layout, int firstRow, int numRows, bool enabled);

	// Активирует/деактивирует элемент лейаута
	void setLayoutItemEnabled(QLayoutItem *item, bool enabled);

	// ф-ции возврата значений выделенных объектов
	QString getCurrentPositionX() const;
	QString getCurrentPositionY() const;
	QString getCurrentSizeW() const;
	QString getCurrentSizeH() const;
	QString getCurrentRotationAngle() const;
	QString getCurrentRotationCenterX() const;
	QString getCurrentRotationCenterY() const;
	int getCurrentSpriteOpacity(bool *equal = NULL) const;
	QString getCurrentText() const;
	QString getCurrentLabelFileName() const;
	QString getCurrentFontSize() const;
	Label::HorzAlignment getCurrentHorzAlignment(bool *equal = NULL) const;
	Label::VertAlignment getCurrentVertAlignment(bool *equal = NULL) const;
	QString getCurrentLineSpacing() const;
	int getCurrentLabelOpacity(bool *equal = NULL) const;

	// Устанавливает новую позицию для выделенных объектов
	void setNewPosition();

	// Устанавливает новый размер для выделенных объектов
	void setNewSize(bool widthChanged);

	// Устанавливает новый центр вращения для выделенных объектов
	void setNewRotationCenter();

	// Выдает сигналы об изменении сцены и слоев, в которых находятся выделенные объекты
	void emitSceneAndLayerChangedSignals(const QString &commandName);

	// отображение значений параметров объектов в виджетах ГУИ
	void updateCommonWidgets();
	void updateSpriteWidgets();
	void updateLabelWidgets();

	// просчет текущего общего ограничивающего прямоугольника
	QRectF calculateCurrentBoundingRect() const;

	// просчет процентного расположения центра вращения
	QPointF calculatePercentPosition(const QRectF &boundingRect, const QPointF &rotationCenter);

	// просчет расположения центра вращения по процентному
	QPointF calculatePosition(const QRectF &boundingRect, const QPointF &percentCenter);

	// возврат из опций текущей коренной директории
	QString getRootPath() const;

	QString getSpritesPath() const;

	QString getFontsPath() const;

	// загрузка и отображение списка доступных шрифтов в комбобоксе
	void scanFonts();

	QList<GameObject *> mSelectedObjects;           // текущие выделенные объекты
	QPointF             mRotationCenter;            // текущий центр вращения выделенных объектов

	QPlainTextEdit      *mPlainTextEdit;            // Многострочное поле ввода в диалоге ввода текста
	QDialogButtonBox    *mDialogButtonBox;          // Группа кнопок в диалоге ввода текста

	QButtonGroup        *mHorzAlignmentButtonGroup; // группировка кнопок горизонтального выравнивания
	QButtonGroup        *mVertAlignmentButtonGroup; // группировка кнопок вертикального выравнивания

	QIcon               mLockTextureSizeIcon;       // иконка для кнопки блокировки изменения размера
	QIcon               mUnlockTextureSizeIcon;     // иконка для кнопки разблокировки изменения размера

	bool                mOpacitySliderMoved;        // Флаг перемещения ползунка прозрачности
};

#endif // PROPERTY_WINDOW_H
