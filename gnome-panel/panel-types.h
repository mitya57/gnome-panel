/*global type definitions*/
#ifndef PANEL_TYPES_H
#define PANEL_TYPES_H

#include <gtk/gtktypeutils.h>

typedef enum {
	ORIENT_UP=0,
	ORIENT_DOWN,
	ORIENT_LEFT,
	ORIENT_RIGHT
} PanelOrientType;

typedef enum {
	SIZE_TINY=0,
	SIZE_STANDARD,
	SIZE_LARGE,
	SIZE_HUGE
} PanelSizeType;

typedef enum {
	LAUNCHER_TILE=0,
	DRAWER_TILE,
	MENU_TILE,
	MISC_TILE,
	LAST_TILE
} PanelTileType;

typedef enum {
	EDGE_PANEL,
	DRAWER_PANEL,
	ALIGNED_PANEL,
	SLIDING_PANEL,
	FREE_PANEL,
	PANEL_LAST_TYPE
} PanelType;


typedef GtkType (*CreateGtkTypeFunc) (void);
extern CreateGtkTypeFunc create_panel_type [PANEL_LAST_TYPE];

typedef enum {
	STANDARD_PANEL     =0,
	BORDER_PANEL       =1,
	TABBED_PANEL       =2,
	PANEL_LAST_SUBTYPE =4
} PanelSubType;

#endif
