#ifndef __TAGS_H
#define __TAGS_H

#include "cstypes.h"

/*
	TAGS.H
	Sunday, July 3, 1994 5:33:15 PM

	This is a list of all of the tags used by code that uses the wad file format. 
	One tag, KEY_TAG, has special meaning, and KEY_TAG_SIZE must be set to the 
	size of an index entry.  Each wad can only have one index entry.  You can get the
	index entry from a wad, or from all of the wads in the file easily.
	
	Marathon uses the KEY_TAG as the name of the level.

Feb 2, 2000 (Loren Petrich):
	Changed application creator to 26.A "Aleph One"
	Changed soundfile type to 'snd�' to be Marathon-Infinity compatible

Feb 3, 2000 (Loren Petrich):
	Changed shapes-file type to 'shp�' to be Marathon-Infinity compatible

Feb 4, 2000 (Loren Petrich):
	Changed most of the other 2's to �'s to be Marathon-Infinity compatible,
	except for the map file type.

Feb 6, 2000 (Loren Petrich):
	Added loading of typecodes from the resource fork

Aug 21, 2000 (Loren Petrich):
	Added a preferences filetype

Aug 22, 2000 (Loren Petrich):
	Added an images filetype

Aug 28, 2000 (Loren Petrich):
	get_typecode() now defaults to '????' for unrecognized typecodes

Mar 14, 2001 (Loren Petrich):
	Added a music filetype
*/

#define MAXIMUM_LEVEL_NAME_SIZE 64

/* OSTypes.. */
// LP change: moved values to filetypes_macintosh.c
enum {
	_typecode_creator,
	_typecode_scenario,
	_typecode_savegame,
	_typecode_film,
	_typecode_physics,
	_typecode_shapes,
	_typecode_sounds,
	_typecode_patch,
	_typecode_images,
	_typecode_preferences,
	_typecode_music,
	_typecode_theme,	// pseudo type code
	NUMBER_OF_TYPECODES
};

// LP addition: typecode handling
// Initializer: loads from resource fork
void initialize_typecodes();
// Accessor
uint32 get_typecode(int which);

// These are no longer constants, which will cause trouble for switch/case constructions
// These have been eliminated in favor of using the above enum of abstracted filetypes
// as much as possible
/*
#define APPLICATION_CREATOR (get_typecode(_typecode_creator))
#define SCENARIO_FILE_TYPE (get_typecode(_typecode_scenario))
#define SAVE_GAME_TYPE (get_typecode(_typecode_savegame))
#define FILM_FILE_TYPE (get_typecode(_typecode_film))
#define PHYSICS_FILE_TYPE (get_typecode(_typecode_physics))
#define SHAPES_FILE_TYPE (get_typecode(_typecode_shapes))
#define SOUNDS_FILE_TYPE (get_typecode(_typecode_sounds))
#define PATCH_FILE_TYPE (get_typecode(_typecode_patch))
#define IMAGES_FILE_TYPE (get_typecode(_typcode_images))
#define PREFERENCES_FILE_TYPE (get_typecode(_typecode_prefs))
*/

/* Other tags-  */
#define POINT_TAG FOUR_CHARS_TO_INT('P','N','T','S')
#define LINE_TAG FOUR_CHARS_TO_INT('L','I','N','S')
#define SIDE_TAG FOUR_CHARS_TO_INT('S','I','D','S')
#define POLYGON_TAG FOUR_CHARS_TO_INT('P','O','L','Y')
#define LIGHTSOURCE_TAG FOUR_CHARS_TO_INT('L','I','T','E')
#define ANNOTATION_TAG FOUR_CHARS_TO_INT('N','O','T','E')
#define OBJECT_TAG FOUR_CHARS_TO_INT('O','B','J','S')
#define GUARDPATH_TAG FOUR_CHARS_TO_INT('p','\x8c','t','h')
#define MAP_INFO_TAG FOUR_CHARS_TO_INT('M','i','n','f')
#define ITEM_PLACEMENT_STRUCTURE_TAG FOUR_CHARS_TO_INT('p','l','a','c')
#define DOOR_EXTRA_DATA_TAG FOUR_CHARS_TO_INT('d','o','o','r')
#define PLATFORM_STATIC_DATA_TAG FOUR_CHARS_TO_INT('p','l','a','t')
#define ENDPOINT_DATA_TAG FOUR_CHARS_TO_INT('E','P','N','T')
#define MEDIA_TAG FOUR_CHARS_TO_INT('m','e','d','i')
#define AMBIENT_SOUND_TAG FOUR_CHARS_TO_INT('a','m','b','i')
#define RANDOM_SOUND_TAG FOUR_CHARS_TO_INT('b','o','n','k')
#define TERMINAL_DATA_TAG FOUR_CHARS_TO_INT('t','e','r','m')

/* Save/Load game tags. */
#define PLAYER_STRUCTURE_TAG FOUR_CHARS_TO_INT('p','l','y','r')
#define DYNAMIC_STRUCTURE_TAG FOUR_CHARS_TO_INT('d','w','o','l')
#define OBJECT_STRUCTURE_TAG FOUR_CHARS_TO_INT('m','o','b','j')
#define DOOR_STRUCTURE_TAG FOUR_CHARS_TO_INT('d','o','o','r')
#define MAP_INDEXES_TAG FOUR_CHARS_TO_INT('i','i','d','x')
#define AUTOMAP_LINES FOUR_CHARS_TO_INT('a','l','i','n')
#define AUTOMAP_POLYGONS FOUR_CHARS_TO_INT('a','p','o','l')
#define MONSTERS_STRUCTURE_TAG FOUR_CHARS_TO_INT('m','O','n','s')
#define EFFECTS_STRUCTURE_TAG FOUR_CHARS_TO_INT('f','x',' ',' ')
#define PROJECTILES_STRUCTURE_TAG FOUR_CHARS_TO_INT('b','a','n','g')
#define PLATFORM_STRUCTURE_TAG FOUR_CHARS_TO_INT('P','L','A','T')
#define WEAPON_STATE_TAG FOUR_CHARS_TO_INT('w','e','a','p')
#define TERMINAL_STATE_TAG FOUR_CHARS_TO_INT('c','i','n','t')

/* Physix model tags */
#define MONSTER_PHYSICS_TAG FOUR_CHARS_TO_INT('M','N','p','x')
#define EFFECTS_PHYSICS_TAG FOUR_CHARS_TO_INT('F','X','p','x')
#define PROJECTILE_PHYSICS_TAG FOUR_CHARS_TO_INT('P','R','p','x')
#define PHYSICS_PHYSICS_TAG FOUR_CHARS_TO_INT('P','X','p','x')
#define WEAPONS_PHYSICS_TAG FOUR_CHARS_TO_INT('W','P','p','x')

/* Preferences Tags.. */
#define prefGRAPHICS_TAG FOUR_CHARS_TO_INT('g','r','a','f')
#define prefSERIAL_TAG FOUR_CHARS_TO_INT('s','e','r','l')
#define prefNETWORK_TAG FOUR_CHARS_TO_INT('n','e','t','w')
#define prefPLAYER_TAG FOUR_CHARS_TO_INT('p','l','y','r')
#define prefINPUT_TAG FOUR_CHARS_TO_INT('i','n','p','u')
#define prefSOUND_TAG FOUR_CHARS_TO_INT('s','n','d',' ')
#define prefENVIRONMENT_TAG FOUR_CHARS_TO_INT('e','n','v','r')

#endif
