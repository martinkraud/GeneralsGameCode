// SPDX-License-Identifier: GPL-3.0-or-later
// X64.52A: Skirmish Match Options dropdowns. See GameClient/MatchOptionControls.h.
#include "PreRTS.h"

#include "GameClient/MatchOptionControls.h"

#include "Common/AIBehavior.h"
#include "Common/SuperweaponLimit.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/WinInstanceData.h"

#include <cstring>

namespace
{
void copyAppearance(GameWindow *target, GameWindow *source)
{
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

const MatchOptionControls::Entry kAiBehavior[] = {
	{ "GUI:AIBehaviorOriginal", L"Original", AI_BEHAVIOR_ORIGINAL },
	{ "GUI:AIBehaviorEnhanced", L"Enhanced", AI_BEHAVIOR_ENHANCED },
};

// the order of the request: Disabled, Max 1, Max 3, Max 5, Unlimited
const MatchOptionControls::Entry kSuperweapons[] = {
	{ "GUI:SuperweaponsDisabled", L"Disabled", SuperweaponLimit::kDisabled },
	{ "GUI:SuperweaponsMax1", L"Max 1", 1 },
	{ "GUI:SuperweaponsMax3", L"Max 3", 3 },
	{ "GUI:SuperweaponsMax5", L"Max 5", 5 },
	{ "GUI:SuperweaponsUnlimited", L"Unlimited", SuperweaponLimit::kUnlimited },
};
}

namespace MatchOptionControls
{
const Entry *aiBehaviorEntries(int &count) { count = (int)(sizeof(kAiBehavior) / sizeof(kAiBehavior[0])); return kAiBehavior; }
const Entry *superweaponEntries(int &count) { count = (int)(sizeof(kSuperweapons) / sizeof(kSuperweapons[0])); return kSuperweapons; }

GameWindow *createCombo(GameWindow *parent, const MatchOptionsLayout::Rect &label, const MatchOptionsLayout::Rect &control, GameWindow *appearance, GameWindow *labelDonor,
                        NameKeyType labelId, NameKeyType comboId, const char *labelKey, const wchar_t *labelFallback, const Entry *entries, int count)
{
	if (!parent || !appearance || !entries || count <= 0)
		return nullptr;
	GameWindow *combo = TheWindowManager->winGetWindowFromId(parent, comboId);
	if (!combo)
	{
		WinInstanceData captionInstance;
		captionInstance.m_font = labelDonor ? labelDonor->winGetFont() : appearance->winGetFont();
		captionInstance.m_style = GWS_STATIC_TEXT;
		captionInstance.m_id = labelId;
		TextData text = {};
		text.centeredVertically = TRUE;
		GameWindow *caption = TheWindowManager->gogoGadgetStaticText(parent, WIN_STATUS_ENABLED | WIN_STATUS_NO_INPUT, label.x, label.y, label.w, label.h,
			&captionInstance, &text, captionInstance.m_font, FALSE);
		if (caption)
		{
			if (labelDonor)
				copyAppearance(caption, labelDonor);
			caption->winSetFont(appearance->winGetFont());
			if (TextData *data = (TextData *)caption->winGetUserData())
			{
				data->centered = FALSE;
				data->centeredVertically = TRUE;
			}
			GadgetStaticTextSetText(caption, TheGameText->FETCH_OR_SUBSTITUTE(labelKey, labelFallback));
		}

		WinInstanceData instance;
		instance.m_font = appearance->winGetFont();
		instance.m_enabledText = appearance->winGetInstanceData()->m_enabledText;
		instance.m_disabledText = appearance->winGetInstanceData()->m_disabledText;
		instance.m_hiliteText = appearance->winGetInstanceData()->m_hiliteText;
		instance.m_id = comboId;
		instance.m_style = GWS_COMBO_BOX | GWS_MOUSE_TRACK;
		ComboBoxData data = {};
		data.maxDisplay = count;
		data.maxChars = 24;
		data.entryData = NEW EntryData;
		memset(data.entryData, 0, sizeof(EntryData));
		data.entryData->maxTextLen = data.maxChars;
		data.listboxData = NEW ListboxData;
		memset(data.listboxData, 0, sizeof(ListboxData));
		data.listboxData->listLength = count;
		data.listboxData->forceSelect = TRUE;
		data.listboxData->columns = 1;
		combo = TheWindowManager->gogoGadgetComboBox(parent, WIN_STATUS_ENABLED | WIN_STATUS_TAB_STOP, control.x, control.y, control.w, control.h, &instance, &data, instance.m_font, FALSE);
		if (!combo)
			return nullptr;
		copyAppearance(combo, appearance);
		copyAppearance(GadgetComboBoxGetEditBox(combo), GadgetComboBoxGetEditBox(appearance));
		copyAppearance(GadgetComboBoxGetDropDownButton(combo), GadgetComboBoxGetDropDownButton(appearance));
		copyAppearance(GadgetComboBoxGetListBox(combo), GadgetComboBoxGetListBox(appearance));
		combo->winBringToTop();   // the list opens over the controls below it
	}
	GadgetComboBoxReset(combo);
	for (int i = 0; i < count; ++i)
	{
		const int index = GadgetComboBoxAddEntry(combo, TheGameText->FETCH_OR_SUBSTITUTE(entries[i].textKey, entries[i].fallback), combo->winGetEnabledTextColor());
		GadgetComboBoxSetItemData(combo, index, (void *)(intptr_t)entries[i].value);
	}
	return combo;
}

void place(GameWindow *window, const MatchOptionsLayout::Rect &where)
{
	if (!window)
		return;
	window->winSetPosition(where.x, where.y);
	window->winSetSize(where.w, where.h);
}

void placeCombo(GameWindow *combo, const MatchOptionsLayout::Rect &where)
{
	if (!combo)
		return;
	GameWindow *entry = GadgetComboBoxGetEditBox(combo);
	GameWindow *button = GadgetComboBoxGetDropDownButton(combo);
	GameWindow *list = GadgetComboBoxGetListBox(combo);
	int listWidth = 0, listHeight = 0, comboWidth = 0, comboHeight = 0;
	if (list)
		list->winGetSize(&listWidth, &listHeight);
	combo->winGetSize(&comboWidth, &comboHeight);
	int rows = (list && comboHeight > 0) ? (listHeight + comboHeight / 2) / comboHeight : 1;
	if (rows < 1)
		rows = 1;
	const MatchOptionsLayout::ComboParts parts = MatchOptionsLayout::comboParts(where, rows);
	place(combo, where);
	if (entry) { entry->winSetPosition(parts.entry.x, parts.entry.y); entry->winSetSize(parts.entry.w, parts.entry.h); }
	if (button) { button->winSetPosition(parts.button.x, parts.button.y); button->winSetSize(parts.button.w, parts.button.h); }
	if (list) { list->winSetPosition(parts.list.x, parts.list.y); list->winSetSize(parts.list.w, parts.list.h); }
}

void select(GameWindow *combo, int value)
{
	if (!combo)
		return;
	const int count = GadgetComboBoxGetLength(combo);
	for (int i = 0; i < count; ++i)
	{
		if ((int)(intptr_t)GadgetComboBoxGetItemData(combo, i) == value)
		{
			GadgetComboBoxSetSelectedPos(combo, i, TRUE);
			return;
		}
	}
}

int selected(GameWindow *combo, int fallback)
{
	if (!combo)
		return fallback;
	Int index = -1;
	GadgetComboBoxGetSelectedPos(combo, &index);
	if (index < 0)
		return fallback;
	return (int)(intptr_t)GadgetComboBoxGetItemData(combo, index);
}
}
