-- *****************************************************************************
-- This file was automatically generated by dmb editor.
-- All changes made in this file will be lost. DO NOT EDIT!
-- *****************************************************************************

-- Location root table. Do not declare global variables with the same name!
mainScreen =
{
	-- Layers table
	layers =
	{type = "LayerGroup", name = "", visibleState = 0, lockState = 0, expanded = false,
		{type = "LayerGroup", name = "главное меню", visibleState = 0, lockState = 0, expanded = true,
			{type = "Layer", name = "надписи", visibleState = 0, lockState = 0, expanded = false,
				{type = "Label", name = "Текст 5", id = 12, posX = 38, posY = 773, width = 347, height = 43, angle = 0, centerX = 0.5, centerY = 0.5, text = "Настройки", fileName = "fonts/liberation_sans_bold.ttf", size = 32, horzAlignment = 0, vertAlignment = 0, lineSpacing = 1, color = 0xFFFFFFFF},
				{type = "Label", name = "Текст 4", id = 11, posX = 38, posY = 623, width = 347, height = 43, angle = 0, centerX = 0.5, centerY = 0.5, text = "Диагностика", fileName = "fonts/liberation_sans_bold.ttf", size = 32, horzAlignment = 0, vertAlignment = 0, lineSpacing = 1, color = 0xFFFFFFFF},
				{type = "Label", name = "Текст 3", id = 10, posX = 38, posY = 463, width = 347, height = 43, angle = 0, centerX = 0.5, centerY = 0.5, text = "Калибровка", fileName = "fonts/liberation_sans_bold.ttf", size = 32, horzAlignment = 0, vertAlignment = 0, lineSpacing = 1, color = 0xFFFFFFFF},
				{type = "Label", name = "Текст 2", id = 9, posX = 38, posY = 311, width = 347, height = 43, angle = 0, centerX = 0.5, centerY = 0.5, text = "Оператор 2", fileName = "fonts/liberation_sans_bold.ttf", size = 32, horzAlignment = 0, vertAlignment = 0, lineSpacing = 1, color = 0xFFFFFFFF},
				{type = "Label", name = "Текст 1", id = 8, posX = 38, posY = 160, width = 347, height = 43, angle = 0, centerX = 0.5, centerY = 0.5, text = "Оператор 1", fileName = "fonts/liberation_sans_bold.ttf", size = 32, horzAlignment = 0, vertAlignment = 0, lineSpacing = 1, color = 0xFFFFFFFF}
			},
			{type = "Layer", name = "фон меню", visibleState = 0, lockState = 0, expanded = false,
				{type = "Sprite", name = "Спрайт 6", id = 7, posX = 23, posY = 31, width = 380, height = 873, angle = 0, centerX = 0.5, centerY = 0.5, fileName = "sprites/main_menu.png", textureWidth = 380, textureHeight = 873, color = 0xFFFFFFFF}
			}
		},
		{type = "LayerGroup", name = "кнопка СТАРТ", visibleState = 0, lockState = 0, expanded = true,
			{type = "LayerGroup", name = "отпущена", visibleState = 0, lockState = 0, expanded = true,
				{type = "Layer", name = "отпущена", visibleState = 0, lockState = 0, expanded = false,
					{type = "Sprite", name = "Спрайт 3", id = 3, posX = {["en"] = 712, ["ru"] = 712}, posY = {["en"] = 901, ["ru"] = 901}, width = {["en"] = 189, ["ru"] = 192}, height = {["en"] = 48, ["ru"] = 47}, angle = 0, centerX = 0.5, centerY = 0.5, fileName = {["en"] = "sprites/start_button_text_en_normal.png", ["ru"] = "sprites/start_button_text_ru_normal.png"}, textureWidth = {["en"] = 189, ["ru"] = 192}, textureHeight = {["en"] = 48, ["ru"] = 47}, color = 0xFFFFFFFF},
					{type = "Sprite", name = "Спрайт 2", id = 2, posX = 667, posY = 846, width = 270, height = 156, angle = 0, centerX = 0.52222222, centerY = 0.5, fileName = "sprites/start_button_normal.png", textureWidth = 270, textureHeight = 156, color = 0xFFFFFFFF}
				}
			},
			{type = "LayerGroup", name = "нажата", visibleState = 0, lockState = 0, expanded = true,
				{type = "Layer", name = "нажата", visibleState = 0, lockState = 0, expanded = false,
					{type = "Sprite", name = "Спрайт 5", id = 5, posX = {["en"] = 712, ["ru"] = 712}, posY = {["en"] = 901, ["ru"] = 901}, width = {["en"] = 188, ["ru"] = 191}, height = {["en"] = 47, ["ru"] = 46}, angle = 0, centerX = 0.5, centerY = 0.5, fileName = {["en"] = "sprites/start_button_text_en_pressed.png", ["ru"] = "sprites/start_button_text_ru_pressed.png"}, textureWidth = {["en"] = 188, ["ru"] = 191}, textureHeight = {["en"] = 47, ["ru"] = 46}, color = 0xFFFFFFFF},
					{type = "Sprite", name = "Спрайт 4", id = 4, posX = 667, posY = 846, width = 270, height = 156, angle = 0, centerX = 0.5, centerY = 0.5, fileName = "sprites/start_button_pressed.png", textureWidth = 270, textureHeight = 156, color = 0xFFFFFFFF}
				}
			}
		},
		{type = "Layer", name = "фон", visibleState = 0, lockState = 0, expanded = false,
			{type = "Sprite", name = "Спрайт 1", id = 1, posX = 0, posY = 0, width = 1280, height = 1024, angle = 0, centerX = 0.5, centerY = 0.5, fileName = "sprites/background.jpg", textureWidth = 1280, textureHeight = 1024, color = 0xFFFFFFFF}
		}
	},

	-- Path to the current active layer
	activeLayer = {1, 0, 0},

	-- Counters for generating various object IDs
	objectIndex = 13,
	layerIndex = 6,
	layerGroupIndex = 5,
	spriteIndex = 7,
	labelIndex = 6,

	-- Coordinates of horizontal and vertical guides
	horzGuides = {512},
	vertGuides = {38, 640}
}
