/*
LUA_PLAYER.CPP

	Copyright (C) 2008 by Gregory Smith
 
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html

	Implements the Lua Player class
*/

#include "ActionQueues.h"
#include "computer_interface.h"
#include "Crosshairs.h"
#include "fades.h"
#include "game_window.h"
#include "lua_map.h"
#include "lua_monsters.h"
#include "lua_objects.h"
#include "lua_player.h"
#include "lua_templates.h"
#include "map.h"
#include "monsters.h"
#include "Music.h"
#include "player.h"
#include "network_games.h"
#include "Random.h"
#include "screen.h"
#include "SoundManager.h"
#include "ViewControl.h"

#define DONT_REPEAT_DEFINITIONS
#include "item_definitions.h"

#ifdef HAVE_LUA

const float AngleConvert = 360/float(FULL_CIRCLE);

char Lua_Action_Flags_Name[] = "action_flags";
typedef L_Class<Lua_Action_Flags_Name> Lua_Action_Flags;

extern ModifiableActionQueues *GetGameQueue();

template<uint32 flag> 
static int Lua_Action_Flags_Get_t(lua_State *L)
{
	int player_index = Lua_Action_Flags::Index(L, 1);

	if (GetGameQueue()->countActionFlags(player_index))
	{
		uint32 flags = GetGameQueue()->peekActionFlags(player_index, 0);
		lua_pushboolean(L, flags & flag);
	}
	else
	{
		return luaL_error(L, "action flags are only accessible in idle()");
	}

	return 1;
}

template<uint32 flag> 
static int Lua_Action_Flags_Set_t(lua_State *L)
{
	if (!lua_isboolean(L, 2))
		return luaL_error(L, "action flags: incorrect argument type");
	
	int player_index = Lua_Action_Flags::Index(L, 1);
	if (GetGameQueue()->countActionFlags(player_index))
	{
		if (lua_toboolean(L, 2))
		{
			GetGameQueue()->modifyActionFlags(player_index, flag, flag);
		}
		else
		{
			GetGameQueue()->modifyActionFlags(player_index, 0, flag);
		}
	}
	else
	{
		return luaL_error(L, "action flags are only accessible in idle()");
	}

	return 0;
}

static int Lua_Action_Flags_Set_Microphone(lua_State *L)
{
	if (!lua_isboolean(L, 2))
		return luaL_error(L, "action flags: incorrect argument type");

	if (lua_toboolean(L, 2))
		return luaL_error(L, "you can only disable the microphone button flag");

	int player_index = Lua_Action_Flags::Index(L, 1);
	if (GetGameQueue()->countActionFlags(player_index))
	{
		GetGameQueue()->modifyActionFlags(player_index, 0, _microphone_button);
	}
	else
	{
		return luaL_error(L, "action flags are only accessible in idle()");
	}

	return 0;
}

const luaL_reg Lua_Action_Flags_Get[] = {
	{"action_trigger", Lua_Action_Flags_Get_t<_action_trigger_state>},
	{"cycle_weapons_backward", Lua_Action_Flags_Get_t<_cycle_weapons_backward>},
	{"cycle_weapons_forward", Lua_Action_Flags_Get_t<_cycle_weapons_forward>},
	{"left_trigger", Lua_Action_Flags_Get_t<_left_trigger_state>},
	{"microphone_button", Lua_Action_Flags_Get_t<_microphone_button>},
	{"right_trigger", Lua_Action_Flags_Get_t<_right_trigger_state>},
	{"toggle_map", Lua_Action_Flags_Get_t<_toggle_map>},
	{0, 0}
};

const luaL_reg Lua_Action_Flags_Set[] = {
	{"action_trigger", Lua_Action_Flags_Set_t<_action_trigger_state>},
	{"cycle_weapons_backward", Lua_Action_Flags_Set_t<_cycle_weapons_backward>},
	{"cycle_weapons_forward", Lua_Action_Flags_Set_t<_cycle_weapons_forward>},
	{"left_trigger", Lua_Action_Flags_Set_t<_left_trigger_state>},
	{"microphone_button", Lua_Action_Flags_Set_Microphone},
	{"right_trigger", Lua_Action_Flags_Set_t<_right_trigger_state>},
	{"toggle_map", Lua_Action_Flags_Set_t<_toggle_map>},
	{0, 0}
};

char Lua_Crosshairs_Name[] = "crosshairs";
typedef L_Class<Lua_Crosshairs_Name> Lua_Crosshairs;

static int Lua_Crosshairs_Get_Active(lua_State *L)
{
	int player_index = Lua_Crosshairs::Index(L, 1);
	if (player_index == local_player_index)
	{
		lua_pushboolean(L, Crosshairs_IsActive());
		return 1;
	}
	else
	{
		return 0;
	}
}

const luaL_reg Lua_Crosshairs_Get[] = {
	{"active", Lua_Crosshairs_Get_Active},
	{0, 0}
};

static int Lua_Crosshairs_Set_Active(lua_State *L)
{
	int player_index = Lua_Crosshairs::Index(L, 1);
	if (player_index == local_player_index)
	{
		if (!lua_isboolean(L, 2))
			return luaL_error(L, "active: incorrect argument type");

		Crosshairs_SetActive(lua_toboolean(L, 2));
	}
	
	return 0;
}

const luaL_reg Lua_Crosshairs_Set[] = {
	{"active", Lua_Crosshairs_Set_Active},
	{0, 0}
};

char Lua_OverlayColor_Name[] = "overlay_color";
typedef L_Enum<Lua_OverlayColor_Name> Lua_OverlayColor;

template<char *name>
class PlayerSubtable : public L_Class<name>
{
public:
	int16 m_player_index;
	static PlayerSubtable *Push(lua_State *L, int16 player_index, int16 index);
	static int16 PlayerIndex(lua_State *L, int index);
};

template<char *name>
PlayerSubtable<name> *PlayerSubtable<name>::Push(lua_State *L, int16 player_index, int16 index)
{
	PlayerSubtable<name> *t = 0;

	if (!L_Class<name, int16>::Valid(index) || !Lua_Player::Valid(player_index))
	{
		lua_pushnil(L);
		return 0;
	}

	t = static_cast<PlayerSubtable<name>*>(lua_newuserdata(L, sizeof(PlayerSubtable<name>)));
	luaL_getmetatable(L, name);
	lua_setmetatable(L, -2);
	t->m_index = index;
	t->m_player_index = player_index;

	return t;
}

template<char *name>
int16 PlayerSubtable<name>::PlayerIndex(lua_State *L, int index)
{
	PlayerSubtable<name> *t = static_cast<PlayerSubtable<name> *>(lua_touserdata(L, index));
	if (!t) luaL_typerror(L, index, name);
	return t->m_player_index;
}

char Lua_Overlay_Name[] = "overlay";
typedef PlayerSubtable<Lua_Overlay_Name> Lua_Overlay;

int Lua_Overlay_Clear(lua_State *L)
{
	int index = Lua_Overlay::Index(L, 1);
	if (Lua_Overlay::PlayerIndex(L, 1) == local_player_index)
	{
		SetScriptHUDIcon(index, 0, 0);
		SetScriptHUDText(index, 0);
	}

	return 0;
}

int Lua_Overlay_Fill_Icon(lua_State *L)
{
	if (Lua_Overlay::PlayerIndex(L, 1) == local_player_index)
	{
		int color = Lua_OverlayColor::ToIndex(L, 2);
		SetScriptHUDSquare(Lua_Overlay::Index(L, 1), color);
	}

	return 0;
}

const luaL_reg Lua_Overlay_Get[] = {
	{"clear", L_TableFunction<Lua_Overlay_Clear>},
	{"fill_icon", L_TableFunction<Lua_Overlay_Fill_Icon>},
	{0, 0}
};

static int Lua_Overlay_Set_Icon(lua_State *L)
{
	if (Lua_Overlay::PlayerIndex(L, 1) == local_player_index)
	{
		if (lua_isstring(L, 2))
		{
			SetScriptHUDIcon(Lua_Overlay::Index(L, 1), lua_tostring(L, 2), lua_strlen(L, 2));
		}
		else
		{
			SetScriptHUDIcon(Lua_Overlay::Index(L, 1), 0, 0);
		}
	}

	return 0;
}

static int Lua_Overlay_Set_Text(lua_State *L)
{
	if (Lua_Overlay::PlayerIndex(L, 1) == local_player_index)
	{
		const char *text = 0;
		if (lua_isstring(L, 2)) 
			text = lua_tostring(L, 2);
		
		SetScriptHUDText(Lua_Overlay::Index(L, 1), text);
	}

	return 0;
}

static int Lua_Overlay_Set_Text_Color(lua_State *L)
{
	if (Lua_Overlay::PlayerIndex(L, 1) == local_player_index)
	{
		int color = Lua_OverlayColor::ToIndex(L, 2);
		SetScriptHUDColor(Lua_Overlay::Index(L, 1), color);
	}

	return 0;
}

const luaL_reg Lua_Overlay_Set[] = {
	{"color", Lua_Overlay_Set_Text_Color},
	{"icon", Lua_Overlay_Set_Icon},
	{"text", Lua_Overlay_Set_Text},
	{0, 0}
};

char Lua_Overlays_Name[] = "overlays";
typedef L_Class<Lua_Overlays_Name> Lua_Overlays;

static int Lua_Overlays_Get(lua_State *L)
{
	if (lua_isnumber(L, 2))
	{
		int player_index = Lua_Overlays::Index(L, 1);
		int index = static_cast<int>(lua_tonumber(L, 2));
		if (Lua_Overlays::Valid(player_index) && index >= 0 && index < MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS)
		{
			Lua_Overlay::Push(L, player_index, index);
		}
		else
		{
			lua_pushnil(L);
		}
	}
	else
	{
		lua_pushnil(L);
	}

	return 1;
}

const luaL_reg Lua_Overlays_Metatable[] = {
	{"__index", Lua_Overlays_Get},
	{0, 0}
};

char Lua_Player_Items_Name[] = "player_items";
typedef L_Class<Lua_Player_Items_Name> Lua_Player_Items;

static int Lua_Player_Items_Get(lua_State *L)
{
	int player_index = Lua_Player_Items::Index(L, 1);
	int item_type = Lua_ItemType::ToIndex(L, 2);

	player_data *player = get_player_data(player_index);
	int item_count = player->items[item_type];
	if (item_count == NONE) item_count = 0;
	lua_pushnumber(L, item_count);
	return 1;
}

extern void destroy_players_ball(short player_index);
extern void select_next_best_weapon(short player_index);

static int Lua_Player_Items_Set(lua_State *L)
{
	if (!lua_isnumber(L, 3)) 
		return luaL_error(L, "items: incorrect argument type");

	int player_index = Lua_Player_Items::Index(L, 1);
	player_data *player = get_player_data(player_index);
	int item_type = Lua_ItemType::ToIndex(L, 2);
	int item_count = player->items[item_type];
	item_definition *definition = get_item_definition_external(item_type);
	int new_item_count = static_cast<int>(lua_tonumber(L, 3));
	
	if (new_item_count < 0) 
		luaL_error(L, "items: invalid item count");

	if (item_count == NONE) item_count = 0;
	if (new_item_count == 0) new_item_count = NONE;

	if (new_item_count < item_count)
	{
		if (definition->item_kind == _ball)
		{
			if (find_player_ball_color(player_index) != NONE)
				destroy_players_ball(player_index);
		}
		else
		{
			player->items[item_type] = new_item_count;
			mark_player_inventory_as_dirty(player_index, item_type);
			if (definition->item_kind == _weapon && player->items[item_type] == NONE)
			{
				select_next_best_weapon(player_index);
			}
		}
	}
	else if (new_item_count > item_count)
	{
		while (new_item_count-- > item_count)
		{
			try_and_add_player_item(player_index, item_type);
		}
	}

	return 0;
}

const luaL_reg Lua_Player_Items_Metatable[] = {
	{"__index", Lua_Player_Items_Get},
	{"__newindex", Lua_Player_Items_Set},
	{0, 0}
};

char Lua_InternalVelocity_Name[] = "internal_velocity";
typedef L_Class<Lua_InternalVelocity_Name> Lua_InternalVelocity;

static int Lua_InternalVelocity_Get_Forward(lua_State *L)
{
	int player_index = Lua_InternalVelocity::Index(L, 1);
	player_data *player = get_player_data(player_index);
	lua_pushnumber(L, (double) player->variables.velocity / FIXED_ONE);
	return 1;
}

static int Lua_InternalVelocity_Get_Perpendicular(lua_State *L)
{
	int player_index = Lua_InternalVelocity::Index(L, 1);
	player_data *player = get_player_data(player_index);
	lua_pushnumber(L, (double) player->variables.perpendicular_velocity / FIXED_ONE);
	return 1;
}

const luaL_reg Lua_InternalVelocity_Get[] = {
	{"forward", Lua_InternalVelocity_Get_Forward},
	{"perpendicular", Lua_InternalVelocity_Get_Perpendicular},
	{0, 0}
};

char Lua_ExternalVelocity_Name[] = "external_velocity";
typedef L_Class<Lua_ExternalVelocity_Name> Lua_ExternalVelocity;

static int Lua_ExternalVelocity_Get_I(lua_State *L)
{
	lua_pushnumber(L, (double) get_player_data(Lua_ExternalVelocity::Index(L, 1))->variables.external_velocity.i / WORLD_ONE);
	return 1;
}

static int Lua_ExternalVelocity_Get_J(lua_State *L)
{
	lua_pushnumber(L, (double) get_player_data(Lua_ExternalVelocity::Index(L, 1))->variables.external_velocity.j / WORLD_ONE);
	return 1;
}

static int Lua_ExternalVelocity_Get_K(lua_State *L)
{
	lua_pushnumber(L, (double) get_player_data(Lua_ExternalVelocity::Index(L, 1))->variables.external_velocity.k / WORLD_ONE);
	return 1;
}

const luaL_reg Lua_ExternalVelocity_Get[] = {
	{"i", Lua_ExternalVelocity_Get_I},
	{"j", Lua_ExternalVelocity_Get_J},
	{"k", Lua_ExternalVelocity_Get_K},
	{"x", Lua_ExternalVelocity_Get_I},
	{"y", Lua_ExternalVelocity_Get_J},
	{"z", Lua_ExternalVelocity_Get_K},
	{0, 0}
};

static int Lua_ExternalVelocity_Set_I(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "i: incorrect argument type");

	int raw_velocity = static_cast<int>(lua_tonumber(L, 2) * WORLD_ONE);
	get_player_data(Lua_ExternalVelocity::Index(L, 1))->variables.external_velocity.i = raw_velocity;
}

static int Lua_ExternalVelocity_Set_J(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "j: incorrect argument type");

	int raw_velocity = static_cast<int>(lua_tonumber(L, 2) * WORLD_ONE);
	get_player_data(Lua_ExternalVelocity::Index(L, 1))->variables.external_velocity.j = raw_velocity;
}

static int Lua_ExternalVelocity_Set_K(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "k: incorrect argument type");

	int raw_velocity = static_cast<int>(lua_tonumber(L, 2) * WORLD_ONE);
	get_player_data(Lua_ExternalVelocity::Index(L, 1))->variables.external_velocity.k = raw_velocity;
}

const luaL_reg Lua_ExternalVelocity_Set[] = {
	{"i", Lua_ExternalVelocity_Set_I},
	{"j", Lua_ExternalVelocity_Set_J},
	{"k", Lua_ExternalVelocity_Set_K},
	{"x", Lua_ExternalVelocity_Set_I},
	{"y", Lua_ExternalVelocity_Set_J},
	{"z", Lua_ExternalVelocity_Set_K},
	{0, 0}
};

char Lua_FadeType_Name[] = "fade_type";
typedef L_Enum<Lua_FadeType_Name> Lua_FadeType;

char Lua_FadeTypes_Name[] = "FadeTypes";
typedef L_EnumContainer<Lua_FadeTypes_Name, Lua_FadeType> Lua_FadeTypes;

char Lua_WeaponType_Name[] = "weapon_type";
typedef L_Enum<Lua_WeaponType_Name> Lua_WeaponType;

char Lua_WeaponTypes_Name[] = "WeaponTypes";
typedef L_EnumContainer<Lua_WeaponTypes_Name, Lua_WeaponType> Lua_WeaponTypes;
	
char Lua_Player_Weapon_Trigger_Name[] = "player_weapon_trigger";
class Lua_Player_Weapon_Trigger : public PlayerSubtable<Lua_Player_Weapon_Trigger_Name>
{
public:
	int16 m_weapon_index;

	static Lua_Player_Weapon_Trigger *Push(lua_State *L, int16 player_index, int16 weapon_index, int16 index);
	static int16 WeaponIndex(lua_State *L, int index);
};

Lua_Player_Weapon_Trigger *Lua_Player_Weapon_Trigger::Push(lua_State *L, int16 player_index, int16 weapon_index, int16 index)
{
	Lua_Player_Weapon_Trigger *t = static_cast<Lua_Player_Weapon_Trigger *>(PlayerSubtable<Lua_Player_Weapon_Trigger_Name>::Push(L, player_index, index));
	if (t)
	{
		t->m_weapon_index = weapon_index;
	}

	return t;
}

int16 Lua_Player_Weapon_Trigger::WeaponIndex(lua_State *L, int index)
{
	Lua_Player_Weapon_Trigger *t = static_cast<Lua_Player_Weapon_Trigger*>(lua_touserdata(L, index));
	if (!t) luaL_typerror(L, index, Lua_Player_Weapon_Trigger_Name);
	return t->m_weapon_index;
}

int Lua_Player_Weapon_Trigger_Get_Rounds(lua_State *L)
{
	short rounds = get_player_weapon_ammo_count(
		Lua_Player_Weapon_Trigger::PlayerIndex(L, 1), 
		Lua_Player_Weapon_Trigger::WeaponIndex(L, 1),
		Lua_Player_Weapon_Trigger::Index(L, 1));
	lua_pushnumber(L, rounds);
	return 1;
}

const luaL_reg Lua_Player_Weapon_Trigger_Get[] = {
	{"rounds", Lua_Player_Weapon_Trigger_Get_Rounds},
	{0, 0}
};

char Lua_Player_Weapon_Name[] = "player_weapon";
typedef PlayerSubtable<Lua_Player_Weapon_Name> Lua_Player_Weapon;

template<int trigger>
static int get_weapon_trigger(lua_State *L)
{
	Lua_Player_Weapon_Trigger::Push(L, Lua_Player_Weapon::PlayerIndex(L, 1), Lua_Player_Weapon::Index(L, 1), trigger);
	return 1;
}

static int Lua_Player_Weapon_Get_Type(lua_State *L)
{
	Lua_WeaponType::Push(L, Lua_Player_Weapon::Index(L, 1));
	return 1;
}

extern bool ready_weapon(short player_index, short weapon_index);

int Lua_Player_Weapon_Select(lua_State *L)
{
	ready_weapon(Lua_Player_Weapon::PlayerIndex(L, 1), Lua_Player_Weapon::Index(L, 1));
	return 0;
}

const luaL_reg Lua_Player_Weapon_Get[] = { 
	{"primary", get_weapon_trigger<_primary_weapon>},
	{"secondary", get_weapon_trigger<_secondary_weapon>},
	{"select", L_TableFunction<Lua_Player_Weapon_Select>},
	{"type", Lua_Player_Weapon_Get_Type},
	{0, 0} 
};

extern player_weapon_data *get_player_weapon_data(const short player_index);
extern bool player_has_valid_weapon(short player_index);

char Lua_Player_Weapons_Name[] = "player_weapons";
typedef L_Class<Lua_Player_Weapons_Name> Lua_Player_Weapons;

static int Lua_Player_Weapons_Get(lua_State *L)
{
	if (lua_isnumber(L, 2) || Lua_WeaponType::Is(L, 2))
	{
		int player_index = Lua_Player_Weapons::Index(L, 1);
		int index = Lua_WeaponType::ToIndex(L, 2);
		if (!Lua_Player_Weapons::Valid(player_index) || !Lua_WeaponType::Valid(index))
		{
			lua_pushnil(L);
		}
		else
		{
			Lua_Player_Weapon::Push(L, player_index, index);
		}
	}
	else if (lua_isstring(L, 2))
	{
		if (strcmp(lua_tostring(L, 2), "current") == 0)
		{
			int player_index = Lua_Player_Weapons::Index(L, 1);
			if (player_has_valid_weapon(player_index))
			{
				player_weapon_data *weapon_data = get_player_weapon_data(player_index);
				player_data *player = get_player_data(player_index);
				Lua_Player_Weapon::Push(L, player_index, weapon_data->current_weapon);
			}
			else
			{
				lua_pushnil(L);
			}
		}
		else
		{
			lua_pushnil(L);
		}
	}
	else
	{
		lua_pushnil(L);
	}

	return 1;
}

const luaL_reg Lua_Player_Weapons_Metatable[] = {
	{"__index", Lua_Player_Weapons_Get},
	{0, 0}
};

char Lua_Player_Kills_Name[] = "player_kills";
typedef L_Class<Lua_Player_Kills_Name> Lua_Player_Kills;

static int Lua_Player_Kills_Get(lua_State *L)
{
	int player_index = Lua_Player_Kills::Index(L, 1);
	int slain_player_index = Lua_Player::Index(L, 2);
	
	player_data *slain_player = get_player_data(slain_player_index);

	lua_pushnumber(L, slain_player->damage_taken[player_index].kills);
	return 1;
}			

static int Lua_Player_Kills_Set(lua_State *L)
{
	if (!lua_isnumber(L, 3))
		return luaL_error(L, "kills: incorrect argument type");

	int player_index = Lua_Player_Kills::Index(L, 1);
	int slain_player_index = Lua_Player::Index(L, 2);	
	int kills = static_cast<int>(lua_tonumber(L, 3));

	player_data *player = get_player_data(player_index);
	player_data *slain_player = get_player_data(slain_player_index);

	if (slain_player->damage_taken[player_index].kills != kills)
	{
		team_damage_taken[slain_player->team].kills += kills - slain_player->damage_taken[player_index].kills;
		if (slain_player_index != player_index)
		{
			team_damage_given[player->team].kills += kills - slain_player->damage_taken[player_index].kills;
		}
		if (slain_player->team == player->team)
		{
			team_friendly_fire[slain_player->team].kills += kills - slain_player->damage_taken[player_index].kills;
		}
		slain_player->damage_taken[player_index].kills = kills;
		mark_player_network_stats_as_dirty(current_player_index);
	}
	return 0;
}

const luaL_reg Lua_Player_Kills_Metatable[] = {
	{"__index", Lua_Player_Kills_Get},
	{"__newindex", Lua_Player_Kills_Set},
	{0, 0}
};


char Lua_Player_Name[] = "player";

// methods

// accelerate(direction, velocity, vertical_velocity)
int Lua_Player_Accelerate(lua_State *L)
{
	if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
		return luaL_error(L, "accelerate: incorrect argument type");

	player_data *player = get_player_data(Lua_Player::Index(L, 1));
	double direction = static_cast<double>(lua_tonumber(L, 2));
	double velocity = static_cast<double>(lua_tonumber(L, 3));
	double vertical_velocity = static_cast<double>(lua_tonumber(L, 4));

	accelerate_player(player->monster_index, static_cast<int>(vertical_velocity * WORLD_ONE), static_cast<int>(direction/AngleConvert), static_cast<int>(velocity * WORLD_ONE));
	return 0;
}

int Lua_Player_Activate_Terminal(lua_State *L)
{
	int16 text_index = NONE;
	if (lua_isnumber(L, 2))
		text_index = static_cast<int16>(lua_tonumber(L, 2));
	else if (Lua_Terminal::Is(L, 2))
		text_index = Lua_Terminal::Index(L, 2);
	else
		return luaL_error(L, "activate_terminal: invalid terminal index");

	enter_computer_interface(Lua_Player::Index(L, 1), text_index, calculate_level_completion_state());
	return 0;
}

int Lua_Player_Find_Action_Key_Target(lua_State *L)
{
	// no arguments
	short target_type;
	short object_index = find_action_key_target(Lua_Player::Index(L, 1), MAXIMUM_ACTIVATION_RANGE, &target_type);

	if (object_index != NONE)
	{
		switch (target_type)
		{
		case _target_is_platform:
			Lua_Platform::Push(L, object_index);
			break;

		case _target_is_control_panel:
			Lua_Side::Push(L, object_index);
			break;

		default:
			lua_pushnil(L);
			break;
		}
	}
	else
	{
		lua_pushnil(L);
	}

	return 1;
}

int Lua_Player_Damage(lua_State *L)
{
	int args = lua_gettop(L);
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "damage: incorrect argument type");
	
	player_data *player = get_player_data(Lua_Player::Index(L, 1));
	if (PLAYER_IS_DEAD(player) || PLAYER_IS_TOTALLY_DEAD(player))
		return 0;

	damage_definition damage;
	damage.type = _damage_crushing;
	damage.base = static_cast<int>(lua_tonumber(L, 2));
	damage.random = 0;
	damage.scale = FIXED_ONE;

	if (args > 2)
	{
		damage.type = Lua_DamageType::ToIndex(L, 3);
	}

	damage_player(player->monster_index, NONE, NONE, &damage, NONE);
	return 0;
}

int Lua_Player_Fade_Screen(lua_State *L)
{
	short player_index = Lua_Player::Index(L, 1);
	if (player_index == local_player_index)
	{
		int fade_index = Lua_FadeType::ToIndex(L, 2);
		start_fade(fade_index);
	}
	return 0;
}

int Lua_Player_Play_Sound(lua_State *L)
{
	int args = lua_gettop(L);
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "play_sound: incorrect argument type");
	
	int player_index = Lua_Player::Index(L, 1);
	int sound_index = static_cast<int>(lua_tonumber(L, 2));
	float pitch = 1.0;
	if (args > 2)
	{
		if (lua_isnumber(L, 3))
			pitch = static_cast<float>(lua_tonumber(L, 3));
		else
			return luaL_error(L, "play_sound: incorrect argument type");
	}

	if (local_player_index != player_index)
		return 0;

	SoundManager::instance()->PlaySound(sound_index, NULL, NONE, _fixed(FIXED_ONE * pitch));
	return 0;
}

extern struct physics_constants *get_physics_constants_for_model(short physics_model, uint32 action_flags);
extern void instantiate_physics_variables(struct physics_constants *constants, struct physics_variables *variables, short player_index, bool first_time, bool take_action);

int Lua_Player_Position(lua_State *L)
{
	if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
		return luaL_error(L, ("position: incorrect argument type"));

	int polygon_index = 0;
	if (lua_isnumber(L, 5))
	{
		polygon_index = static_cast<int>(lua_tonumber(L, 5));
		if (!Lua_Polygon::Valid(polygon_index))
			return luaL_error(L, ("position: invalid polygon index"));
	}
	else if (Lua_Polygon::Is(L, 5))
	{
		polygon_index = Lua_Polygon::Index(L, 5);
	}
	else
		return luaL_error(L, ("position: incorrect argument type"));

	int player_index = Lua_Player::Index(L, 1);
	player_data *player = get_player_data(player_index);
	object_data *object = get_object_data(player->object_index);

	world_point3d location;
	location.x = static_cast<int>(lua_tonumber(L, 2) * WORLD_ONE);
	location.y = static_cast<int>(lua_tonumber(L, 3) * WORLD_ONE);
	location.z = static_cast<int>(lua_tonumber(L, 4) * WORLD_ONE);
	
	translate_map_object(player->object_index, &location, polygon_index);
	player->variables.position.x = WORLD_TO_FIXED(object->location.x);
	player->variables.position.y = WORLD_TO_FIXED(object->location.y);
	player->variables.position.z = WORLD_TO_FIXED(object->location.z);
	
	instantiate_physics_variables(get_physics_constants_for_model(static_world->physics_model, 0), &player->variables, player_index, false, false);
	return 0;
}

int Lua_Player_Teleport(lua_State *L)
{
	if (!lua_isnumber(L, 2) && !Lua_Polygon::Is(L, 2))
		return luaL_error(L, "teleport(): incorrect argument type");

	int destination = -1;
	if (lua_isnumber(L, 2))
		destination = static_cast<int>(lua_tonumber(L, 2));
	else 
		destination = Lua_Polygon::Index(L, 2);

	int player_index = Lua_Player::Index(L, 1);
	
	player_data *player = get_player_data(player_index);
	monster_data *monster = get_monster_data(player->monster_index);

	SET_PLAYER_TELEPORTING_STATUS(player, true);
	monster->action = _monster_is_teleporting;
	player->teleporting_phase = 0;
	player->delay_before_teleport = 0;

	player->teleporting_destination = destination;
	if (local_player_index == player_index)
		start_teleporting_effect(true);
	play_object_sound(player->object_index, Sound_TeleportOut());
	return 0;
}

int Lua_Player_Teleport_To_Level(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "teleport_to_level(): incorrect argument type");

	int level = static_cast<int>(lua_tonumber(L, 2));
	int player_index = Lua_Player::Index(L, 1);
	
	player_data *player = get_player_data(player_index);
	monster_data *monster = get_monster_data(player->monster_index);
	
	SET_PLAYER_TELEPORTING_STATUS(player, true);
	monster->action = _monster_is_teleporting;
	player->teleporting_phase = 0;
	player->delay_before_teleport = 0;

	player->teleporting_destination = -level - 1;
	if (View_DoInterlevelTeleportOutEffects()) {
		start_teleporting_effect(true);
		play_object_sound(player->object_index, Sound_TeleportOut());
	}
	return 0;
}

// get accessors

static int Lua_Player_Get_Action_Flags(lua_State *L)
{
	Lua_Action_Flags::Push(L, Lua_Player::Index(L, 1));
	return 1;
}

static int Lua_Player_Get_Color(lua_State *L)
{
	lua_pushnumber(L, get_player_data(Lua_Player::Index(L, 1))->color);
	return 1;
}

static int Lua_Player_Get_Crosshairs(lua_State *L)
{
	Lua_Crosshairs::Push(L, Lua_Player::Index(L, 1));
	return 1;
}

static int Lua_Player_Get_Dead(lua_State *L)
{
	player_data *player = get_player_data(Lua_Player::Index(L, 1));
	lua_pushboolean(L, (PLAYER_IS_DEAD(player) || PLAYER_IS_TOTALLY_DEAD(player)));
	return 1;
}

static int Lua_Player_Get_Deaths(lua_State *L)
{
	player_data *player = get_player_data(Lua_Player::Index(L, 1));
	lua_pushnumber(L, player->monster_damage_taken.kills);
	return 1;
}

static int Lua_Player_Get_Energy(lua_State *L)
{
	lua_pushnumber(L, get_player_data(Lua_Player::Index(L, 1))->suit_energy);
	return 1;
}

static int Lua_Player_Get_Elevation(lua_State *L)
{
	double angle = FIXED_INTEGERAL_PART(get_player_data(Lua_Player::Index(L, 1))->variables.elevation) * AngleConvert;
	lua_pushnumber(L, angle);
	return 1;
}

static int Lua_Player_Get_Direction(lua_State *L)
{
	double angle = FIXED_INTEGERAL_PART(get_player_data(Lua_Player::Index(L, 1))->variables.direction) * AngleConvert;
	lua_pushnumber(L, angle);
	return 1;
}

static int Lua_Player_Get_External_Velocity(lua_State *L)
{
	Lua_ExternalVelocity::Index(L, Lua_Player::Index(L, 1));
	return 1;
}

static int Lua_Player_Get_Extravision_Duration(lua_State *L)
{
	lua_pushnumber(L, get_player_data(Lua_Player::Index(L, 1))->extravision_duration);
	return 1;
}

template<uint16 flag>
static int Lua_Player_Get_Flag(lua_State *L)
{
	player_data *player = get_player_data(Lua_Player::Index(L, 1));
	lua_pushboolean(L, player->variables.flags & flag);
	return 1;
}

static int Lua_Player_Get_Infravision_Duration(lua_State *L)
{
	lua_pushnumber(L, get_player_data(Lua_Player::Index(L, 1))->infravision_duration);
	return 1;
}

static int Lua_Player_Get_Internal_Velocity(lua_State *L)
{
	Lua_InternalVelocity::Push(L, Lua_Player::Index(L, 1));
	return 1;
}

static int Lua_Player_Get_Invincibility_Duration(lua_State *L)
{
	lua_pushnumber(L, get_player_data(Lua_Player::Index(L, 1))->invincibility_duration);
	return 1;
}

static int Lua_Player_Get_Invisibility_Duration(lua_State *L)
{
	lua_pushnumber(L, get_player_data(Lua_Player::Index(L, 1))->invisibility_duration);
	return 1;
}

static int Lua_Player_Get_Items(lua_State *L)
{
	Lua_Player_Items::Push(L, Lua_Player::Index(L, 1));
	return 1;
}

static int Lua_Player_Get_Kills(lua_State *L)
{
	Lua_Player_Kills::Push(L, Lua_Player::Index(L, 1));
	return 1;
}

static int Lua_Player_Get_Local(lua_State *L)
{
	lua_pushboolean(L, Lua_Player::Index(L, 1) == local_player_index);
	return 1;
}

extern bool MotionSensorActive;

static int Lua_Player_Get_Motion_Sensor(lua_State *L)
{
	short player_index = Lua_Player::Index(L, 1);
	if (player_index == local_player_index)
	{
		lua_pushboolean(L, MotionSensorActive);
		return 1;
	}
	else
	{
		return 0;
	}
}

static int Lua_Player_Get_Monster(lua_State *L)
{
	Lua_Monster::Push(L, get_player_data(Lua_Player::Index(L, 1))->monster_index);
	return 1;
}

static int Lua_Player_Get_Name(lua_State *L)
{
	lua_pushstring(L, get_player_data(Lua_Player::Index(L, 1))->name);
	return 1;
}

static int Lua_Player_Get_Overlays(lua_State *L)
{
	Lua_Overlays::Push(L, Lua_Player::Index(L, 1));
	return 1;
}

static int Lua_Player_Get_Oxygen(lua_State *L)
{
	lua_pushnumber(L, get_player_data(Lua_Player::Index(L, 1))->suit_oxygen);
	return 1;
}

static int Lua_Player_Get_Points(lua_State *L)
{
	lua_pushnumber(L, get_player_data(Lua_Player::Index(L, 1))->netgame_parameters[0]);
	return 1;
}

static int Lua_Player_Get_Polygon(lua_State *L)
{
	Lua_Polygon::Push(L, get_player_data(Lua_Player::Index(L, 1))->supporting_polygon_index);
	return 1;
}

static int Lua_Player_Get_Team(lua_State *L)
{
	lua_pushnumber(L, get_player_data(Lua_Player::Index(L, 1))->team);
	return 1;
}

static int Lua_Player_Get_Weapons(lua_State *L)
{
	Lua_Player_Weapons::Push(L, Lua_Player::Index(L, 1));
	return 1;
}

static int Lua_Player_Get_X(lua_State *L)
{
	lua_pushnumber(L, (double) get_player_data(Lua_Player::Index(L, 1))->location.x / WORLD_ONE);
	return 1;
}

static int Lua_Player_Get_Y(lua_State *L)
{
	lua_pushnumber(L, (double) get_player_data(Lua_Player::Index(L, 1))->location.y / WORLD_ONE);
	return 1;
}

static int Lua_Player_Get_Z(lua_State *L)
{
	lua_pushnumber(L, (double) get_player_data(Lua_Player::Index(L, 1))->location.z / WORLD_ONE);
	return 1;
}

static int Lua_Player_Get_Zoom(lua_State *L)
{
	short player_index = Lua_Player::Index(L, 1);
	if (player_index == local_player_index)
	{
		lua_pushboolean(L, GetTunnelVision());
		return 1;
	}
	else
	{
		return 0;
	}
}

const luaL_reg Lua_Player_Get[] = {
	{"accelerate", L_TableFunction<Lua_Player_Accelerate>},
	{"action_flags", Lua_Player_Get_Action_Flags},
	{"activate_terminal", L_TableFunction<Lua_Player_Activate_Terminal>},
	{"color", Lua_Player_Get_Color},
	{"crosshairs", Lua_Player_Get_Crosshairs},
	{"damage", L_TableFunction<Lua_Player_Damage>},
	{"dead", Lua_Player_Get_Dead},
	{"deaths", Lua_Player_Get_Deaths},
	{"direction", Lua_Player_Get_Direction},
	{"energy", Lua_Player_Get_Energy},
	{"elevation", Lua_Player_Get_Elevation},
	{"external_velocity", Lua_Player_Get_External_Velocity},
	{"extravision_duration", Lua_Player_Get_Extravision_Duration},
	{"feet_below_media", Lua_Player_Get_Flag<_FEET_BELOW_MEDIA_BIT>},
	{"fade_screen", L_TableFunction<Lua_Player_Fade_Screen>},
	{"find_action_key_target", L_TableFunction<Lua_Player_Find_Action_Key_Target>},
	{"head_below_media", Lua_Player_Get_Flag<_HEAD_BELOW_MEDIA_BIT>},
	{"infravision_duration", Lua_Player_Get_Infravision_Duration},
	{"internal_velocity", Lua_Player_Get_Internal_Velocity},
	{"invincibility_duration", Lua_Player_Get_Invincibility_Duration},
	{"invisibility_duration", Lua_Player_Get_Invisibility_Duration},
	{"items", Lua_Player_Get_Items},
	{"local_", Lua_Player_Get_Local},
	{"juice", Lua_Player_Get_Energy},
	{"kills", Lua_Player_Get_Kills},
	{"life", Lua_Player_Get_Energy},
	{"monster", Lua_Player_Get_Monster},
	{"motion_sensor_active", Lua_Player_Get_Motion_Sensor},
	{"name", Lua_Player_Get_Name},
	{"overlays", Lua_Player_Get_Overlays},
	{"oxygen", Lua_Player_Get_Oxygen},
	{"pitch", Lua_Player_Get_Elevation},
	{"play_sound", L_TableFunction<Lua_Player_Play_Sound>},
	{"points", Lua_Player_Get_Points},
	{"polygon", Lua_Player_Get_Polygon},
	{"position", L_TableFunction<Lua_Player_Position>},
	{"team", Lua_Player_Get_Team},
	{"teleport", L_TableFunction<Lua_Player_Teleport>},
	{"teleport_to_level", L_TableFunction<Lua_Player_Teleport_To_Level>},
	{"weapons", Lua_Player_Get_Weapons},
	{"x", Lua_Player_Get_X},
	{"y", Lua_Player_Get_Y},
	{"yaw", Lua_Player_Get_Direction},
	{"z", Lua_Player_Get_Z},
	{"zoom_active", Lua_Player_Get_Zoom},
	{0, 0}
};

extern void mark_shield_display_as_dirty();

static int Lua_Player_Set_Color(lua_State *L)
{
	if (!lua_isnumber(L, 2))
	{
		return luaL_error(L, "color: incorrect argument type");
	}
	

	int color = static_cast<int>(lua_tonumber(L, 2));
	if (color < 0 || color > NUMBER_OF_TEAM_COLORS)
	{
		luaL_error(L, "player.color: invalid color");
	}
	get_player_data(Lua_Player::Index(L, 1))->color = color;
	
	return 0;
}

static int Lua_Player_Set_Deaths(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "deaths: incorrect argument type");

	player_data *player = get_player_data(Lua_Player::Index(L, 1));
	int kills = static_cast<int>(lua_tonumber(L, 2));
	if (player->monster_damage_taken.kills != kills)
	{
		team_monster_damage_taken[player->team].kills += (kills - player->monster_damage_taken.kills);
		player->monster_damage_taken.kills = kills;
		mark_player_network_stats_as_dirty(current_player_index);
	}

	return 0;
}

static int Lua_Player_Set_Direction(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "direction: incorrect argument type");

	double facing = static_cast<double>(lua_tonumber(L, 2));
	int player_index = Lua_Player::Index(L, 1);
	player_data *player = get_player_data(player_index);
	player->variables.direction = INTEGER_TO_FIXED((int)(facing/AngleConvert));
	instantiate_physics_variables(get_physics_constants_for_model(static_world->physics_model, 0), &player->variables, player_index, false, false);
	return 0;
}

static int Lua_Player_Set_Elevation(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "elevation: incorrect argument type");
	
	double elevation = static_cast<double>(lua_tonumber(L, 2));
	if (elevation > 180) elevation -= 360.0;
	int player_index = Lua_Player::Index(L, 1);
	player_data *player = get_player_data(player_index);
	player->variables.elevation = INTEGER_TO_FIXED((int)(elevation/AngleConvert));
	instantiate_physics_variables(get_physics_constants_for_model(static_world->physics_model, 0), &player->variables, player_index, false, false);
	return 0;
}

static int Lua_Player_Set_Infravision_Duration(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "extravision: incorrect argument type");

	player_data *player = get_player_data(Lua_Player::Index(L, 1));
	player->infravision_duration = static_cast<int>(lua_tonumber(L, 2));
	return 0;
}

static int Lua_Player_Set_Invincibility_Duration(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "extravision: incorrect argument type");

	player_data *player = get_player_data(Lua_Player::Index(L, 1));
	player->invincibility_duration = static_cast<int>(lua_tonumber(L, 2));
	return 0;
}

static int Lua_Player_Set_Invisibility_Duration(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "extravision: incorrect argument type");

	player_data *player = get_player_data(Lua_Player::Index(L, 1));
	player->invisibility_duration = static_cast<int>(lua_tonumber(L, 2));
	return 0;
}

static int Lua_Player_Set_Energy(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "energy: incorrect argument type");

	int energy = static_cast<int>(lua_tonumber(L, 2));
	if (energy > 3 * PLAYER_MAXIMUM_SUIT_ENERGY)
		energy = 3 * PLAYER_MAXIMUM_SUIT_ENERGY;

	get_player_data(Lua_Player::Index(L, 1))->suit_energy = energy;
	mark_shield_display_as_dirty();

	return 0;
}

static int Lua_Player_Set_Extravision_Duration(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "extravision: incorrect argument type");

	int player_index = Lua_Player::Index(L, 1);
	player_data *player = get_player_data(player_index);
	short extravision_duration = static_cast<short>(lua_tonumber(L, 2));
	if ((player_index == local_player_index) && (extravision_duration == 0) != (player->extravision_duration == 0))
	{
		start_extravision_effect(extravision_duration);
	}
	player->extravision_duration = static_cast<int>(lua_tonumber(L, 2));
	return 0;
}

extern void draw_panels();

int Lua_Player_Set_Motion_Sensor(lua_State *L)
{
	short player_index = Lua_Player::Index(L, 1);
	if (player_index == local_player_index)
	{
		if (!lua_isboolean(L, 2))
			return luaL_error(L, "motion_sensor: incorrect argument type");
		bool state = lua_toboolean(L, 2);
		if (MotionSensorActive != state)
		{
			MotionSensorActive = lua_toboolean(L, 2);
			draw_panels();
		}
	}
	
	return 0;
}	

static int Lua_Player_Set_Oxygen(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "oxygen: incorrect argument type");
	
	int oxygen = static_cast<int>(lua_tonumber(L, 2));
	if (oxygen > PLAYER_MAXIMUM_SUIT_OXYGEN)
		oxygen = PLAYER_MAXIMUM_SUIT_OXYGEN;

	get_player_data(Lua_Player::Index(L, 1))->suit_oxygen = oxygen;
	mark_shield_display_as_dirty();

	return 0;
}

int Lua_Player_Set_Points(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "points: incorrect argument type");

	int points = static_cast<int>(lua_tonumber(L, 2));

	player_data *player = get_player_data(Lua_Player::Index(L, 1));
	if (player->netgame_parameters[0] != points)
	{
#if !defined(DISABLE_NETWORKING)
		team_netgame_parameters[player->team][0] += points - player->netgame_parameters[0];
#endif
		player->netgame_parameters[0] = points;
		mark_player_network_stats_as_dirty(current_player_index);
	}

	return 0;
}

static int Lua_Player_Set_Team(lua_State *L)
{
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "team: incorrect argument type");

	int team = static_cast<int>(lua_tonumber(L, 2));
	if (team < 0 || team >= NUMBER_OF_TEAM_COLORS)
	{
		luaL_error(L, "player.team: invalid team");
	}
	get_player_data(Lua_Player::Index(L, 1))->team = team;

	return 0;
}

static int Lua_Player_Set_Zoom(lua_State *L)
{
	short player_index = Lua_Player::Index(L, 1);
	if (player_index == local_player_index)
	{
		if (!lua_isboolean(L, 2))
			return luaL_error(L, "zoom_active: incorrect argument type");
		
		SetTunnelVision(lua_toboolean(L, 2));
	}

	return 0;
}

const luaL_reg Lua_Player_Set[] = {
	{"color", Lua_Player_Set_Color},
	{"deaths", Lua_Player_Set_Deaths},
	{"direction", Lua_Player_Set_Direction},
	{"elevation", Lua_Player_Set_Elevation},
	{"energy", Lua_Player_Set_Energy},
	{"extravision_duration", Lua_Player_Set_Extravision_Duration},
	{"infravision_duration", Lua_Player_Set_Infravision_Duration},
	{"invincibility_duration", Lua_Player_Set_Invincibility_Duration},
	{"invisibility_duration", Lua_Player_Set_Invisibility_Duration},
	{"juice", Lua_Player_Set_Energy},
	{"life", Lua_Player_Set_Energy},
	{"motion_sensor_active", Lua_Player_Set_Motion_Sensor},
	{"oxygen", Lua_Player_Set_Oxygen},
	{"pitch", Lua_Player_Set_Elevation},
	{"points", Lua_Player_Set_Points},
	{"team", Lua_Player_Set_Team},
	{"yaw", Lua_Player_Set_Direction},
	{"zoom_active", Lua_Player_Set_Zoom},
	{0, 0}
};

bool Lua_Player_Valid(int16 index)
{
	return index >= 0 && index < dynamic_world->player_count;
}

char Lua_Players_Name[] = "Players";

int16 Lua_Players_Length() {
	return dynamic_world->player_count;
}

char Lua_DifficultyType_Name[] = "difficulty_type";
typedef L_Enum<Lua_DifficultyType_Name> Lua_DifficultyType;

char Lua_GameType_Name[] = "game_type";
typedef L_Enum<Lua_GameType_Name> Lua_GameType;

char Lua_Game_Name[] = "Game";
typedef L_Class<Lua_Game_Name> Lua_Game;

static int Lua_Game_Get_Difficulty(lua_State *L)
{
	Lua_DifficultyType::Push(L, dynamic_world->game_information.difficulty_level);
	return 1;
}

static int Lua_Game_Get_Kill_Limit(lua_State *L)
{
	lua_pushnumber(L, dynamic_world->game_information.kill_limit);
	return 1;
}

static int Lua_Game_Get_Type(lua_State *L)
{
	Lua_GameType::Push(L, GET_GAME_TYPE());
	return 1;
}

extern GM_Random lua_random_generator;

int Lua_Game_Better_Random(lua_State *L)
{
	if (lua_isnumber(L, 1))
	{
		lua_pushnumber(L, lua_random_generator.KISS() % static_cast<uint32>(lua_tonumber(L, 1)));
	}
	else
	{
		lua_pushnumber(L, lua_random_generator.KISS());
	}
	return 1;
}

int Lua_Game_Global_Random(lua_State *L)
{
	if (lua_isnumber(L, 1))
	{
		lua_pushnumber(L, ::global_random() % static_cast<uint16>(lua_tonumber(L, 1)));
	}
	else
	{
		lua_pushnumber(L, ::global_random());
	}
	return 1;
}

int Lua_Game_Local_Random(lua_State *L)
{
	if (lua_isnumber(L, 1))
	{
		lua_pushnumber(L, ::local_random() % static_cast<uint16>(lua_tonumber(L, 1)));
	}
	else
	{
		lua_pushnumber(L, ::local_random());
	}
	return 1;
}

const luaL_reg Lua_Game_Get[] = {
	{"difficulty", Lua_Game_Get_Difficulty},
	{"global_random", L_TableFunction<Lua_Game_Global_Random>},
	{"kill_limit", Lua_Game_Get_Kill_Limit},
	{"local_random", L_TableFunction<Lua_Game_Local_Random>},
	{"random", L_TableFunction<Lua_Game_Better_Random>},
	{"type", Lua_Game_Get_Type},
	{0, 0}
};

char Lua_Music_Name[] = "Music";
typedef L_Class<Lua_Music_Name> Lua_Music;

int Lua_Music_Clear(lua_State *L)
{
	Music::instance()->ClearLevelMusic();
	return 0;
}

int Lua_Music_Fade(lua_State *L)
{
	int duration;
	if (!lua_isnumber(L, 1))
		duration = 1000;
	else
		duration = static_cast<int>(lua_tonumber(L, 1) * 1000);
	Music::instance()->FadeOut(duration);
	Music::instance()->ClearLevelMusic();
	return 0;
}

int Lua_Music_Play(lua_State *L)
{
	bool restart_music;
	restart_music = !Music::instance()->IsLevelMusicActive() && !Music::instance()->Playing();
	for (int n = 1; n <= lua_gettop(L); n++)
	{
		if (!lua_isstring(L, n))
			return luaL_error(L, "play: invalid file specifier");
	
		FileSpecifier file;
		if (file.SetNameWithPath(lua_tostring(L, n)))
			Music::instance()->PushBackLevelMusic(file);
	}

	if (restart_music)
		Music::instance()->PreloadLevelMusic();
	return 0;
}

int Lua_Music_Stop(lua_State *L)
{
	Music::instance()->ClearLevelMusic();
	Music::instance()->StopLevelMusic();
	return 0;
}

const luaL_reg Lua_Music_Get[] = {
	{"clear", L_TableFunction<Lua_Music_Clear>},
	{"fade", L_TableFunction<Lua_Music_Fade>},
	{"play", L_TableFunction<Lua_Music_Play>},
	{"stop", L_TableFunction<Lua_Music_Stop>},
	{0, 0}
};

static int Lua_Player_load_compatibility(lua_State *L);

int Lua_Player_register (lua_State *L)
{
	Lua_Action_Flags::Register(L, Lua_Action_Flags_Get, Lua_Action_Flags_Set);
	Lua_Crosshairs::Register(L, Lua_Crosshairs_Get, Lua_Crosshairs_Set);
	Lua_Player_Items::Register(L, 0, 0, Lua_Player_Items_Metatable);
	Lua_Player_Kills::Register(L, 0, 0, Lua_Player_Kills_Metatable);

	Lua_InternalVelocity::Register(L, Lua_InternalVelocity_Get);
	Lua_ExternalVelocity::Register(L, Lua_ExternalVelocity_Get, Lua_ExternalVelocity_Set);
	Lua_FadeType::Register(L);
	Lua_FadeType::Valid = Lua_FadeType::ValidRange<NUMBER_OF_FADE_TYPES>;
	
	Lua_FadeTypes::Register(L);
	Lua_FadeTypes::Length = Lua_FadeTypes::ConstantLength<NUMBER_OF_FADE_TYPES>;

	Lua_WeaponType::Register(L);
	Lua_WeaponType::Valid = Lua_WeaponType::ValidRange<MAXIMUM_NUMBER_OF_WEAPONS>;

	Lua_WeaponTypes::Register(L);
	Lua_WeaponTypes::Length = Lua_WeaponTypes::ConstantLength<MAXIMUM_NUMBER_OF_WEAPONS>;

	Lua_Player_Weapon::Register(L, Lua_Player_Weapon_Get);
	Lua_Player_Weapon::Valid = Lua_Player_Weapon::ValidRange<MAXIMUM_NUMBER_OF_WEAPONS>;

	Lua_Player_Weapons::Register(L, 0, 0, Lua_Player_Weapons_Metatable);
	Lua_Player_Weapons::Valid = Lua_Player_Valid;

	Lua_Player_Weapon_Trigger::Register(L, Lua_Player_Weapon_Trigger_Get);
	Lua_Player_Weapon_Trigger::Valid = Lua_Player_Weapon_Trigger::ValidRange<(int) _secondary_weapon + 1>;

	Lua_OverlayColor::Register(L);
	Lua_OverlayColor::Valid = Lua_OverlayColor::ValidRange<8>;

	Lua_Overlays::Register(L, 0, 0, Lua_Overlays_Metatable);
	Lua_Overlays::Valid = Lua_Player_Valid;

	Lua_Overlay::Register(L, Lua_Overlay_Get, Lua_Overlay_Set);
	Lua_Overlay::Valid = Lua_Overlay::ValidRange<MAXIMUM_NUMBER_OF_SCRIPT_HUD_ELEMENTS>;

	Lua_Player::Register(L, Lua_Player_Get, Lua_Player_Set);
	Lua_Player::Valid = Lua_Player_Valid;
	
	Lua_Players::Register(L);
	Lua_Players::Length = Lua_Players_Length;

	Lua_Game::Register(L, Lua_Game_Get);

	Lua_GameType::Register(L);
	Lua_GameType::Valid = Lua_GameType::ValidRange<NUMBER_OF_GAME_TYPES>;

	Lua_DifficultyType::Register(L);
	Lua_DifficultyType::Valid = Lua_DifficultyType::ValidRange<NUMBER_OF_GAME_DIFFICULTY_LEVELS>;

	Lua_Music::Register(L, Lua_Music_Get);

	// register one Game userdatum globally
	Lua_Game::Push(L, 0);
	lua_setglobal(L, Lua_Game_Name);

	// register one Music userdatum
	Lua_Music::Push(L, 0);
	lua_setglobal(L, Lua_Music_Name);
	
	Lua_Player_load_compatibility(L);
	
	return 0;
}

static const char *compatibility_script = ""
	"function accelerate_player(player, vertical_velocity, direction, velocity) Players[player]:accelerate(direction, velocity, vertical_velocity) end\n"
	"function activate_terminal(player, text) Players[player]:activate_terminal(text) end\n"
	"function add_item(player, item_type) Players[player].items[item_type] = Players[player].items[item_type] + 1 end\n"
	"function award_kills(player, slain_player, amount) if player == -1 then Players[slain_player].deaths = Players[slain_player].deaths + amount else Players[player].kills[slain_player] = Players[player].kills[slain_player] + amount end end\n"
	"function add_to_player_external_velocity(player, x, y, z) Players[player].external_velocity.i = Players[player].external_velocity.i + x Players[player].external_velocity.j = Players[player].external_velocity.j + y Players[player].external_velocity.k = Players[player].external_velocity.k + z end\n"
	"function award_points(player, amount) Players[player].points = Players[player].points + amount end\n"
	"function better_random() return Game.random() end\n"
	"function clear_music() Music.clear() end\n"
	"function count_item(player, item_type) return Players[player].items[item_type] end\n"
	"function crosshairs_active(player) return Players[player].crosshairs.active end\n"
	"function destroy_ball(player) for i in ItemTypes() do if i.ball then Players[player].items[i] = 0 end end end\n"
	"function fade_music(duration) if duration then Music.fade(duration * 60 / 1000) else Music.fade(60 / 1000) end end\n"
	"function get_game_difficulty() return Game.difficulty.index end\n"
	"function get_game_type() return Game.type.index end\n"
	"function get_kills(player, slain_player) if player == -1 then return Players[slain_player].deaths else return Players[player].kills[slain_player] end end\n"
	"function get_kill_limit() return Game.kill_limit end\n"
	"function get_life(player) return Players[player].energy end\n"
	"function get_motion_sensor_state(player) return Players[player].motion_sensor_active end\n"
	"function get_oxygen(player) return Players[player].oxygen end\n"
	"function get_player_angle(player) return Players[player].yaw, Players[player].pitch end\n"
	"function get_player_color(player) return Players[player].color end\n"
	"function get_player_external_velocity(player) return Players[player].external_velocity.i * 1024, Players[player].external_velocity.j * 1024, Players[player].external_velocity.k * 1024 end\n"
	"function get_player_internal_velocity(player) return Players[player].internal_velocity.forward * 65536, Players[player].internal_velocity.perpendicular * 65536 end\n"
	"function get_player_name(player) return Players[player].name end\n"
	"function get_player_polygon(player) return Players[player].polygon.index end\n"
	"function get_player_position(player) return Players[player].x, Players[player].y, Players[player].z end\n"
	"function get_player_powerup_duration(player, powerup) if powerup == _powerup_invisibility then return Players[player].invisibility_duration elseif powerup == _powerup_invincibility then return Players[player].invincibility_duration elseif powerup == _powerup_infravision then return Players[player].infravision_duratiohn elseif powerup == _powerup_extravision then return Players[player].extravision_duration end end\n"
	"function get_player_team(player) return Players[player].team end\n"
	"function get_player_weapon(player) if Players[player].weapons.current then return Players[player].weapons.current.index else return nil end end\n"
	"function get_points(player) return Players[player].points end\n"
	"function global_random() return Game.global_random() end\n"
	"function inflict_damage(player, amount, type) if (type) then Players[player]:damage(amount, type) else Players[player]:damage(amount) end end\n"
	"function local_player_index() for p in Players() do if p.local_ then return p.index end end end\n"
	"function local_random() return Game.local_random() end\n"
	"function number_of_players() return # Players end\n"
	"function play_music(...) Music.play(...) end\n"
	"function player_is_dead(player) return Players[player].dead end\n"
	"function player_media(player) if Players[player].head_below_media then return Players[player].polygon.media.index else return nil end end\n"
	"function player_to_monster_index(player) return Players[player].monster.index end\n"
	"function play_sound(player, sound, pitch) Players[player]:play_sound(sound, pitch) end\n"
	"function remove_item(player, item_type) if Players[player].items[item_type] > 0 then Players[player].items[item_type] = Players[player].items[item_type] - 1 end end\n"
	"function screen_fade(player, fade) if fade then Players[player]:fade_screen(fade) else for p in Players() do p:fade_screen(player) end end end\n"
	"function select_weapon(player, weapon) Players[player].weapons[weapon]:select() end\n"
	"function set_crosshairs_active(player, state) Players[player].crosshairs.active = state end\n"
	"function set_kills(player, slain_player, amount) if player == -1 then Players[slain_player].deaths = amount else Players[player].kills[slain_player] = amount end end\n"
	"function set_life(player, shield) Players[player].energy = shield end\n"
	"function set_motion_sensor_state(player, state) Players[player].motion_sensor_active = state end\n"
	"function set_overlay_color(overlay, color) for p in Players() do if p.local_ then p.overlays[overlay].color = color end end end\n"
	"function set_overlay_icon(overlay, icon) for p in Players() do if p.local_ then p.overlays[overlay].icon = icon end end end\n"
	"function set_overlay_icon_by_color(overlay, color) for p in Players() do if p.local_ then p.overlays[overlay]:fill_icon(color) end end end\n"
	"function set_overlay_text(overlay, text) for p in Players() do if p.local_ then p.overlays[overlay].text = text end end end\n"
	"function set_oxygen(player, oxygen) Players[player].oxygen = oxygen end\n"
	"function set_player_angle(player, yaw, pitch) Players[player].yaw = yaw Players[player].pitch = pitch + 360.0 end\n"
	"function set_player_color(player, color) Players[player].color = color end\n"
	"function set_player_external_velocity(player, x, y, z) Players[player].external_velocity.i = x / 1024 Players[player].external_velocity.j = y / 1024 Players[player].external_velocity.k = z / 1024 end\n"
	"function set_player_position(player, x, y, z, polygon) Players[player]:position(x, y, z, polygon) end\n"
	"function set_player_powerup_duration(player, powerup, duration) if powerup == _powerup_invisibility then Players[player].invisibility_duration = duration elseif powerup == _powerup_invincibility then Players[player].invincibility_duration = duration elseif powerup == _powerup_infravision then Players[player].infravision_duration = duration elseif powerup == _powerup_extravision then Players[player].extravision_duration = duration end end\n"
	"function set_player_team(player, team) Players[player].team = team end\n"
	"function set_points(player, amount) Players[player].points = amount end\n"
	"function stop_music() Music.stop() end\n"
	"function set_zoom_state(player, state) Players[player].zoom_active = state end\n"
	"function teleport_player(player, polygon) Players[player]:teleport(polygon) end\n"
	"function teleport_player_to_level(player, level) Players[player]:teleport_to_level(level) end\n"
	"function zoom_active(player) return Players[player].zoom_active end\n"
	;

static int Lua_Player_load_compatibility(lua_State *L)
{
	luaL_loadbuffer(L, compatibility_script, strlen(compatibility_script), "player_compatibility");
	lua_pcall(L, 0, 0, 0);
};

#endif