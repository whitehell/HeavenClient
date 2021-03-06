//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "UIItemInventory.h"

#include "../UI.h"
#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"

#include "../../Data/ItemData.h"
#include "../../Net/Packets/InventoryPackets.h"
#include "../../Util/Misc.h"

#include "nlnx/nx.hpp"

namespace jrc
{
	UIItemInventory::UIItemInventory(const Inventory& invent) : UIDragElement<PosINV>(Point<int16_t>(172, 20)), inventory(invent)
	{
		nl::node close = nl::nx::ui["Basic.img"]["BtClose"];
		nl::node src = nl::nx::ui["UIWindow2.img"]["Item"];

		sprites.emplace_back(src["backgrnd"]); // TODO: productionBackgrnd not working
		sprites.emplace_back(src["productionBackgrnd2"]);
		sprites.emplace_back(src["backgrnd3"]); // TODO: productionBackgrnd3 not working

		newitemslot = src["New"]["inventory"];
		newitemtab = src["New"]["Tab0"];
		projectile = src["activeIcon"];

		nl::node taben = src["Tab"]["enabled"];
		nl::node tabdis = src["Tab"]["disabled"];

		buttons[BT_CLOSE] = std::make_unique<MapleButton>(close, Point<int16_t>(156, 12));

		buttons[BT_TAB_EQUIP] = std::make_unique<TwoSpriteButton>(tabdis["0"], taben["0"]);
		buttons[BT_TAB_USE] = std::make_unique<TwoSpriteButton>(tabdis["1"], taben["1"]);
		buttons[BT_TAB_ETC] = std::make_unique<TwoSpriteButton>(tabdis["2"], taben["2"]);
		buttons[BT_TAB_SETUP] = std::make_unique<TwoSpriteButton>(tabdis["3"], taben["3"]);
		buttons[BT_TAB_CASH] = std::make_unique<TwoSpriteButton>(tabdis["4"], taben["4"]);

		buttons[BT_DROPMESO] = std::make_unique<MapleButton>(src["BtCoin3"]);
		buttons[BT_POINTS] = std::make_unique<MapleButton>(src["BtPoint0"]);
		buttons[BT_GATHER] = std::make_unique<MapleButton>(src["BtGather3"]);
		buttons[BT_SORT] = std::make_unique<MapleButton>(src["BtSort3"]);
		buttons[BT_EXPAND] = std::make_unique<MapleButton>(src["BtFull3"]);
		buttons[BT_ITEMPOT] = std::make_unique<MapleButton>(src["BtPot3"]);
		buttons[BT_UPGRADE] = std::make_unique<MapleButton>(src["BtUpgrade3"]);
		buttons[BT_MAGNIFY] = std::make_unique<MapleButton>(src["BtAppraise3"]);
		buttons[BT_EXTRACT] = std::make_unique<MapleButton>(src["BtExtract3"]);
		buttons[BT_DISASSEMBLE] = std::make_unique<MapleButton>(src["BtDisassemble3"]);
		buttons[BT_TOAD] = std::make_unique<MapleButton>(src["BtToad3"]);

		tab = InventoryType::EQUIP;
		slotrange.first = 1;
		slotrange.second = 24;
		newtab = InventoryType::NONE;
		newslot = 0;

		buttons[BT_SORT]->set_active(false);
		buttons[BT_ITEMPOT]->set_state(Button::State::DISABLED);
		buttons[BT_EXTRACT]->set_state(Button::State::DISABLED);
		buttons[BT_DISASSEMBLE]->set_state(Button::State::DISABLED);
		buttons[button_by_tab(tab)]->set_state(Button::PRESSED);

		mesolabel = { Text::A11M, Text::RIGHT, Text::BLACK };
		maplepointslabel = { Text::A11M, Text::RIGHT, Text::BLACK };

		slider = {
			Slider::Type::DEFAULT, { 50, 245 }, 152, 6, 1 + inventory.get_slotmax(tab) / 4, [&](bool upwards) {
				int16_t shift = upwards ? -4 : 4;
				bool above = slotrange.first + shift > 0;
				bool below = slotrange.second + shift < inventory.get_slotmax(tab) + 1 + 4;

				if (above && below) {
					slotrange.first += shift;
					slotrange.second += shift;
				}
			}
		};

		dimension = { 172, 335 };
		active = true;

		load_icons();
	}

	void UIItemInventory::draw(float alpha) const
	{
		UIElement::draw(alpha);

		slider.draw(position + Point<int16_t>(0, 1));

		for (auto& icon : icons)
		{
			int16_t slot = icon.first;

			if (icon.second && slot >= slotrange.first && slot <= slotrange.second)
			{
				Point<int16_t> slotpos = get_slotpos(slot);
				icon.second->draw(position + slotpos);
			}
		}

		int16_t bulletslot = inventory.get_bulletslot();

		if (tab == InventoryType::USE && is_visible(bulletslot))
		{
			Point<int16_t> bulletslotpos = position + get_slotpos(bulletslot);
			projectile.draw({ bulletslotpos });
		}
		else if (newtab == tab && is_visible(newslot))
		{
			Point<int16_t> newslotpos = position + get_slotpos(newslot);
			newslotpos.shift_y(1);
			newitemslot.draw({ newslotpos }, alpha);
		}

		if (newtab != tab && newtab != InventoryType::NONE)
		{
			Point<int16_t> newtabpos = position + get_tabpos(newtab);
			newitemtab.draw({ newtabpos }, alpha);
		}

		mesolabel.draw(position + Point<int16_t>(127, 262));
		maplepointslabel.draw(position + Point<int16_t>(159, 279));
	}

	void UIItemInventory::update()
	{
		UIElement::update();

		newitemtab.update(6);
		newitemslot.update(6);

		int64_t meso = inventory.get_meso();
		std::string mesostr = std::to_string(meso);
		string_format::split_number(mesostr);
		mesolabel.change_text(mesostr);
		maplepointslabel.change_text("0"); // TODO: Need to bring this value in
	}

	void UIItemInventory::update_slot(int16_t slot)
	{
		if (int32_t item_id = inventory.get_item_id(tab, slot))
		{
			int16_t count;

			if (tab == InventoryType::EQUIP)
				count = -1;
			else
				count = inventory.get_item_count(tab, slot);

			const Texture& texture = ItemData::get(item_id).get_icon(false);
			Equipslot::Id eqslot = inventory.find_equipslot(item_id);

			icons[slot] = std::make_unique<Icon>(
				std::make_unique<ItemIcon>(tab, eqslot, slot),
				texture,
				count
				);
		}
		else if (icons.count(slot))
		{
			icons.erase(slot);
		}
	}

	void UIItemInventory::load_icons()
	{
		icons.clear();

		uint8_t numslots = inventory.get_slotmax(tab);

		for (uint8_t i = 1; i < numslots; i++)
			update_slot(i);
	}

	Button::State UIItemInventory::button_pressed(uint16_t buttonid)
	{
		InventoryType::Id oldtab = tab;

		switch (buttonid)
		{
		case BT_CLOSE:
			active = false;
			return Button::State::NORMAL;
		case BT_TAB_EQUIP:
			tab = InventoryType::EQUIP;
			break;
		case BT_TAB_USE:
			tab = InventoryType::USE;
			break;
		case BT_TAB_SETUP:
			tab = InventoryType::SETUP;
			break;
		case BT_TAB_ETC:
			tab = InventoryType::ETC;
			break;
		case BT_TAB_CASH:
			tab = InventoryType::CASH;
			break;
		case BT_GATHER:
			GatherItemsPacket(tab).dispatch();
			break;
		case BT_SORT:
			SortItemsPacket(tab).dispatch();
			break;
		case BT_DROPMESO:
		case BT_POINTS:
		case BT_EXPAND:
		case BT_ITEMPOT:
		case BT_UPGRADE:
		case BT_MAGNIFY:;
		case BT_EXTRACT:
		case BT_DISASSEMBLE:
		case BT_TOAD:
			return Button::State::NORMAL;
		}

		if (tab != oldtab)
		{
			slotrange.first = 1;
			slotrange.second = 24;

			slider.setrows(6, 1 + inventory.get_slotmax(tab) / 4);

			buttons[button_by_tab(oldtab)]->set_state(Button::NORMAL);

			load_icons();
			enable_gather();
		}

		return Button::State::PRESSED;
	}

	void UIItemInventory::doubleclick(Point<int16_t> cursorpos)
	{
		int16_t slot = slot_by_position(cursorpos - position);

		if (icons.count(slot) && is_visible(slot))
		{
			if (int32_t item_id = inventory.get_item_id(tab, slot))
			{
				switch (tab)
				{
				case InventoryType::EQUIP:
					EquipItemPacket(slot, inventory.find_equipslot(item_id)).dispatch();
					break;
				case InventoryType::USE:
					UseItemPacket(slot, item_id).dispatch();
					break;
				}
			}
		}
	}

	void UIItemInventory::send_icon(const Icon& icon, Point<int16_t> cursorpos)
	{
		int16_t slot = slot_by_position(cursorpos - position);

		if (slot > 0)
		{
			int32_t item_id = inventory.get_item_id(tab, slot);
			Equipslot::Id eqslot;
			bool equip;

			if (item_id && tab == InventoryType::EQUIP)
			{
				eqslot = inventory.find_equipslot(item_id);
				equip = true;
			}
			else
			{
				eqslot = Equipslot::NONE;
				equip = false;
			}

			icon.drop_on_items(tab, eqslot, slot, equip);
		}
	}

	Cursor::State UIItemInventory::send_cursor(bool pressed, Point<int16_t> cursorpos)
	{
		Cursor::State dstate = UIDragElement::send_cursor(pressed, cursorpos);

		if (dragged)
		{
			clear_tooltip();

			return dstate;
		}

		Point<int16_t> cursor_relative = cursorpos - position;

		if (slider.isenabled())
		{
			Cursor::State sstate = slider.send_cursor(cursor_relative, pressed);

			if (sstate != Cursor::IDLE)
			{
				clear_tooltip();

				return sstate;
			}
		}

		int16_t slot = slot_by_position(cursor_relative);
		Icon* icon = get_icon(slot);

		if (icon && is_visible(slot))
		{
			if (pressed)
			{
				Point<int16_t> slotpos = get_slotpos(slot);
				icon->start_drag(cursor_relative - slotpos);
				UI::get().drag_icon(icon);

				clear_tooltip();

				return Cursor::GRABBING;
			}
			else
			{
				show_item(slot);

				return Cursor::CANGRAB;
			}
		}
		else
		{
			clear_tooltip();

			return Cursor::CANGRAB;
		}
	}

	void UIItemInventory::send_key(int32_t keycode, bool pressed)
	{
		if (keycode == KeyAction::ESCAPE)
		{
			active = false;
		}
		else if (keycode == KeyAction::TAB)
		{
			InventoryType::Id oldtab = tab;

			switch (oldtab)
			{
			case InventoryType::EQUIP:
				tab = InventoryType::USE;
				break;
			case InventoryType::USE:
				tab = InventoryType::ETC;
				break;
			case InventoryType::ETC:
				tab = InventoryType::SETUP;
				break;
			case InventoryType::SETUP:
				tab = InventoryType::CASH;
				break;
			case InventoryType::CASH:
				tab = InventoryType::EQUIP;
				break;
			}

			if (tab != oldtab)
			{
				slotrange.first = 1;
				slotrange.second = 24;

				slider.setrows(6, 1 + inventory.get_slotmax(tab) / 4);

				buttons[button_by_tab(oldtab)]->set_state(Button::NORMAL);

				load_icons();
				enable_gather();
			}

			buttons[button_by_tab(tab)]->set_state(Button::PRESSED);
		}
	}

	void UIItemInventory::modify(InventoryType::Id type, int16_t slot, int8_t mode, int16_t arg)
	{
		if (slot <= 0)
			return;

		if (type == tab)
		{
			switch (mode)
			{
			case Inventory::ADD:
				update_slot(slot);
				newtab = type;
				newslot = slot;
				break;
			case Inventory::CHANGECOUNT:
			case Inventory::ADDCOUNT:
				if (auto icon = get_icon(slot))
					icon->set_count(arg);

				break;
			case Inventory::SWAP:
				if (arg != slot)
				{
					update_slot(slot);
					update_slot(arg);
				}

				break;
			case Inventory::REMOVE:
				update_slot(slot);
				break;
			}
		}

		switch (mode)
		{
		case Inventory::ADD:
		case Inventory::ADDCOUNT:
			newtab = type;
			newslot = slot;
			break;
		case Inventory::CHANGECOUNT:
		case Inventory::SWAP:
		case Inventory::REMOVE:
			if (newslot == slot && newtab == type)
			{
				newslot = 0;
				newtab = InventoryType::NONE;
			}

			break;
		}
	}

	void UIItemInventory::enable_sort()
	{
		buttons[BT_GATHER]->set_active(false);
		buttons[BT_SORT]->set_active(true);
		buttons[BT_SORT]->set_state(Button::NORMAL);
	}

	void UIItemInventory::enable_gather()
	{
		buttons[BT_SORT]->set_active(false);
		buttons[BT_GATHER]->set_active(true);
		buttons[BT_GATHER]->set_state(Button::NORMAL);
	}

	void UIItemInventory::toggle_active()
	{
		clear_tooltip();
		UIElement::toggle_active();
	}

	bool UIItemInventory::remove_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (UIDragElement::remove_cursor(clicked, cursorpos))
			return true;

		return slider.remove_cursor(clicked);
	}

	void UIItemInventory::show_item(int16_t slot)
	{
		if (tab == InventoryType::EQUIP)
		{
			UI::get().show_equip(Tooltip::ITEMINVENTORY, slot);
		}
		else
		{
			int32_t item_id = inventory.get_item_id(tab, slot);
			UI::get().show_item(Tooltip::ITEMINVENTORY, item_id);
		}
	}

	void UIItemInventory::clear_tooltip()
	{
		UI::get().clear_tooltip(Tooltip::ITEMINVENTORY);
	}

	bool UIItemInventory::is_visible(int16_t slot) const
	{
		return !is_not_visible(slot);
	}

	bool UIItemInventory::is_not_visible(int16_t slot) const
	{
		return slot < slotrange.first || slot > slotrange.second;
	}

	int16_t UIItemInventory::slot_by_position(Point<int16_t> cursorpos) const
	{
		int16_t xoff = cursorpos.x() - 11;
		int16_t yoff = cursorpos.y() - 51;

		if (xoff < 1 || xoff > 143 || yoff < 1)
			return 0;

		int16_t slot = slotrange.first + (xoff / 36) + 4 * (yoff / 35);

		return is_visible(slot) ? slot : 0;
	}

	Point<int16_t> UIItemInventory::get_slotpos(int16_t slot) const
	{
		int16_t absslot = slot - slotrange.first;

		return Point<int16_t>(
			10 + (absslot % 4) * 36,
			51 + (absslot / 4) * 35
			);
	}

	Point<int16_t> UIItemInventory::get_tabpos(InventoryType::Id tb) const
	{
		switch (tb)
		{
		case InventoryType::EQUIP:
			return Point<int16_t>(10, 28);
		case InventoryType::USE:
			return Point<int16_t>(42, 28);
		case InventoryType::SETUP:
			return Point<int16_t>(74, 28);
		case InventoryType::ETC:
			return Point<int16_t>(105, 28);
		case InventoryType::CASH:
			return Point<int16_t>(138, 28);
		default:
			return Point<int16_t>();
		}
	}

	uint16_t UIItemInventory::button_by_tab(InventoryType::Id tb) const
	{
		switch (tb)
		{
		case InventoryType::EQUIP:
			return BT_TAB_EQUIP;
		case InventoryType::USE:
			return BT_TAB_USE;
		case InventoryType::SETUP:
			return BT_TAB_SETUP;
		case InventoryType::ETC:
			return BT_TAB_ETC;
		default:
			return BT_TAB_CASH;
		}
	}

	Icon* UIItemInventory::get_icon(int16_t slot)
	{
		auto iter = icons.find(slot);

		if (iter != icons.end())
			return iter->second.get();
		else
			return nullptr;
	}


	UIItemInventory::ItemIcon::ItemIcon(InventoryType::Id st, Equipslot::Id eqs, int16_t s)
	{
		sourcetab = st;
		eqsource = eqs;
		source = s;
	}

	void UIItemInventory::ItemIcon::drop_on_stage() const
	{
		MoveItemPacket(sourcetab, source, 0, 1).dispatch();
	}

	void UIItemInventory::ItemIcon::drop_on_equips(Equipslot::Id eqslot) const
	{
		switch (sourcetab)
		{
		case InventoryType::EQUIP:
			if (eqsource == eqslot)
				EquipItemPacket(source, eqslot).dispatch();

			break;
		case InventoryType::USE:
			ScrollEquipPacket(source, eqslot).dispatch();
			break;
		}
	}

	void UIItemInventory::ItemIcon::drop_on_items(InventoryType::Id tab, Equipslot::Id, int16_t slot, bool) const
	{
		if (tab != sourcetab || slot == source)
			return;

		MoveItemPacket(tab, source, slot, 1).dispatch();
	}
}