// SPDX-License-Identifier: GPL-3.0-or-later
// X64.51: "AI Behaviour" selector gadgets of the Skirmish setup screen. See AIBehaviorOptions.h.
#include "PreRTS.h"

#include "GameClient/AIBehaviorOptions.h"

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

UnicodeString entryText(AIBehaviorMode mode)
{
	return mode == AI_BEHAVIOR_ENHANCED ? TheGameText->FETCH_OR_SUBSTITUTE("GUI:AIBehaviorEnhanced", L"Enhanced (repaired)")
	                                    : TheGameText->FETCH_OR_SUBSTITUTE("GUI:AIBehaviorOriginal", L"Original (retail)");
}
}

namespace AIBehaviorOptions
{
GameWindow *create(GameWindow *parent, const Rect &label, const Rect &control, GameWindow *appearance, GameWindow *labelDonor, NameKeyType labelId, NameKeyType comboId)
{
	if (!parent || !appearance)
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
			GadgetStaticTextSetText(caption, TheGameText->FETCH_OR_SUBSTITUTE("GUI:AIBehavior", L"AI Behaviour"));
		}

		WinInstanceData instance;
		instance.m_font = appearance->winGetFont();
		instance.m_enabledText = appearance->winGetInstanceData()->m_enabledText;
		instance.m_disabledText = appearance->winGetInstanceData()->m_disabledText;
		instance.m_hiliteText = appearance->winGetInstanceData()->m_hiliteText;
		instance.m_id = comboId;
		instance.m_style = GWS_COMBO_BOX | GWS_MOUSE_TRACK;
		ComboBoxData data = {};
		data.maxDisplay = 2;
		data.maxChars = 24;
		data.entryData = NEW EntryData;
		memset(data.entryData, 0, sizeof(EntryData));
		data.entryData->maxTextLen = data.maxChars;
		data.listboxData = NEW ListboxData;
		memset(data.listboxData, 0, sizeof(ListboxData));
		data.listboxData->listLength = 2;
		data.listboxData->forceSelect = TRUE;
		data.listboxData->columns = 1;
		combo = TheWindowManager->gogoGadgetComboBox(parent, WIN_STATUS_ENABLED | WIN_STATUS_TAB_STOP, control.x, control.y, control.w, control.h, &instance, &data, instance.m_font, FALSE);
		if (!combo)
			return nullptr;
		copyAppearance(combo, appearance);
		copyAppearance(GadgetComboBoxGetEditBox(combo), GadgetComboBoxGetEditBox(appearance));
		copyAppearance(GadgetComboBoxGetDropDownButton(combo), GadgetComboBoxGetDropDownButton(appearance));
		copyAppearance(GadgetComboBoxGetListBox(combo), GadgetComboBoxGetListBox(appearance));
		combo->winBringToTop();   // the drop-down opens over the controls below it
	}
	GadgetComboBoxReset(combo);
	for (int mode = AI_BEHAVIOR_ORIGINAL; mode <= AI_BEHAVIOR_ENHANCED; ++mode)
	{
		const int index = GadgetComboBoxAddEntry(combo, entryText(static_cast<AIBehaviorMode>(mode)), combo->winGetEnabledTextColor());
		GadgetComboBoxSetItemData(combo, index, (void *)(intptr_t)mode);
	}
	return combo;
}

void select(GameWindow *combo, AIBehaviorMode mode)
{
	if (!combo)
		return;
	const int count = GadgetComboBoxGetLength(combo);
	for (int i = 0; i < count; ++i)
	{
		if ((int)(intptr_t)GadgetComboBoxGetItemData(combo, i) == static_cast<int>(mode))
		{
			GadgetComboBoxSetSelectedPos(combo, i, TRUE);
			return;
		}
	}
}

AIBehaviorMode selected(GameWindow *combo)
{
	if (!combo)
		return AI_BEHAVIOR_ORIGINAL;
	Int index = -1;
	GadgetComboBoxGetSelectedPos(combo, &index);
	if (index < 0)
		return AI_BEHAVIOR_ORIGINAL;
	return (int)(intptr_t)GadgetComboBoxGetItemData(combo, index) == AI_BEHAVIOR_ENHANCED ? AI_BEHAVIOR_ENHANCED : AI_BEHAVIOR_ORIGINAL;
}

const char *preferenceValue(AIBehaviorMode mode)
{
	return mode == AI_BEHAVIOR_ENHANCED ? "Enhanced" : "Original";
}

bool parsePreference(const char *text, AIBehaviorMode &mode)
{
	if (!text)
		return false;
	if (!_stricmp(text, "Enhanced")) { mode = AI_BEHAVIOR_ENHANCED; return true; }
	if (!_stricmp(text, "Original")) { mode = AI_BEHAVIOR_ORIGINAL; return true; }
	return false;
}
}
