/*
** Copyright 2026 TheSuperHackers
** This program is free software: you can redistribute it and/or modify it
** under the terms of the GNU General Public License, version 3 or later.
** This program is distributed WITHOUT ANY WARRANTY; see the GNU GPL for details.
*/
#include "PreRTS.h"

#include "Common/FramePacer.h"
#include "Common/GlobalData.h"
#include "Common/OptionPreferences.h"
#include "GameClient/FrameRateOptions.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetStaticText.h"

static void copyAppearance(GameWindow *target, GameWindow *source)
{
	// Copy only borrowed image/color/font data, never owning text or gadget state.
	WinInstanceData *to = target->winGetInstanceData();
	const WinInstanceData *from = source->winGetInstanceData();
	memcpy(to->m_enabledDrawData, from->m_enabledDrawData, sizeof(to->m_enabledDrawData));
	memcpy(to->m_disabledDrawData, from->m_disabledDrawData, sizeof(to->m_disabledDrawData));
	memcpy(to->m_hiliteDrawData, from->m_hiliteDrawData, sizeof(to->m_hiliteDrawData));
	to->m_enabledText = from->m_enabledText;
	to->m_disabledText = from->m_disabledText;
	to->m_hiliteText = from->m_hiliteText;
	target->winSetStatus(source->winGetStatus() & WIN_STATUS_IMAGE);
	target->winSetDrawFunc(source->winGetDrawFunc());
	target->winSetFont(source->winGetFont());
}

static void selectFrameRate(GameWindow *combo, Int fps)
{
	if (!combo)
		return;
	for (Int i = 0; i < RenderFpsPreset::OptionCount; ++i)
		if (fps == RenderFpsPreset::getOptionFpsValue(i))
			GadgetComboBoxSetSelectedPos(combo, i);
}

GameWindow *createFrameRateOptions(GameWindow *detail, const OptionPreferences &preferences)
{
	// TheSuperHackers @feature Extend the packaged Options layout using native gadgets, without asset overrides.
	GameWindow *combo = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("OptionsMenu.wnd:ComboBoxFrameRateLimit"));
	if (!combo)
	{
		GameWindow *label = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("OptionsMenu.wnd:DetailLabel"));
		if (!detail || !label || label->winGetParent() != detail->winGetParent())
			return nullptr; // A custom layout can provide the named combo instead.
		GameWindow *parent = detail->winGetParent();
		Int x, y, width, height, parentWidth, parentHeight, labelX, labelY, labelWidth, labelHeight;
		detail->winGetPosition(&x, &y);
		detail->winGetSize(&width, &height);
		parent->winGetSize(&parentWidth, &parentHeight);
		label->winGetPosition(&labelX, &labelY);
		label->winGetSize(&labelWidth, &labelHeight);
		const Int gap = max(2, height / 4);
		const Int columnWidth = (parentWidth - 2 * x - gap) / 2;
		if (columnWidth < height * 3)
			return nullptr; // Do not overlap controls in an incompatible mod layout.

		WinInstanceData instance;
		instance.m_font = detail->winGetFont();
		instance.m_enabledText = detail->winGetInstanceData()->m_enabledText;
		instance.m_disabledText = detail->winGetInstanceData()->m_disabledText;
		instance.m_hiliteText = detail->winGetInstanceData()->m_hiliteText;
		instance.m_style = GWS_STATIC_TEXT;
		TextData text = {};
		text.centeredVertically = TRUE;
		GameWindow *caption = TheWindowManager->gogoGadgetStaticText(parent, WIN_STATUS_ENABLED,
			x + columnWidth + gap, labelY, columnWidth, labelHeight, &instance, &text, instance.m_font, FALSE);
		copyAppearance(caption, label);
		caption->winSetFont(detail->winGetFont()); // Keep the longer caption within the split column in both titles.
		GadgetStaticTextSetText(caption, TheGameText->FETCH_OR_SUBSTITUTE("GUI:FrameRateLimit", L"Frame Rate Limit"));

		instance.m_id = NAMEKEY("OptionsMenu.wnd:ComboBoxFrameRateLimit");
		instance.m_style = GWS_COMBO_BOX | GWS_MOUSE_TRACK;
		ComboBoxData data = {};
		data.maxDisplay = RenderFpsPreset::OptionCount;
		data.maxChars = 16;
		data.entryData = NEW EntryData;
		memset(data.entryData, 0, sizeof(EntryData));
		data.entryData->maxTextLen = data.maxChars;
		data.listboxData = NEW ListboxData;
		memset(data.listboxData, 0, sizeof(ListboxData));
		data.listboxData->listLength = RenderFpsPreset::OptionCount;
		data.listboxData->forceSelect = TRUE;
		data.listboxData->columns = 1;
		combo = TheWindowManager->gogoGadgetComboBox(parent, WIN_STATUS_ENABLED | WIN_STATUS_TAB_STOP,
			x + columnWidth + gap, y, columnWidth, height, &instance, &data, instance.m_font, FALSE);
		copyAppearance(combo, detail);
		copyAppearance(GadgetComboBoxGetEditBox(combo), GadgetComboBoxGetEditBox(detail));
		copyAppearance(GadgetComboBoxGetDropDownButton(combo), GadgetComboBoxGetDropDownButton(detail));
		copyAppearance(GadgetComboBoxGetListBox(combo), GadgetComboBoxGetListBox(detail));
		detail->winSetSize(columnWidth, height);
		label->winSetSize(columnWidth, labelHeight);
	}
	GadgetComboBoxReset(combo);
	for (Int i = 0; i < RenderFpsPreset::OptionCount; ++i)
	{
		UnicodeString text;
		text.format(L"%d FPS", RenderFpsPreset::getOptionFpsValue(i));
		GadgetComboBoxAddEntry(combo, text, combo->winGetEnabledTextColor());
	}
	selectFrameRate(combo, preferences.getFrameRateLimit());
	return combo;
}

void resetFrameRateOptions(GameWindow *combo)
{
	selectFrameRate(combo, RenderFpsPreset::DefaultFpsValue);
}

void acceptFrameRateOptions(GameWindow *combo, OptionPreferences &preferences)
{
	if (!combo)
		return;
	Int index = -1;
	GadgetComboBoxGetSelectedPos(combo, &index);
	if (index < 0 || index >= RenderFpsPreset::OptionCount)
		return;
	const Int fps = RenderFpsPreset::getOptionFpsValue(index);
	preferences.setFrameRateLimit(fps);
	preferences.setAsciiString("FPSLimit", "yes");
	TheWritableGlobalData->m_framesPerSecondLimit = fps;
	TheWritableGlobalData->m_useFpsLimit = TRUE;
	TheFramePacer->setFramesPerSecondLimit(fps);
}
