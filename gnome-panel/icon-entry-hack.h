/*
 * This is an utter hack
 *
 * Makes the dentry icon entry somewhat reasonable in UI
 *
 * Copyright (C) 2001 Eazel, Inc.
 *
 * Author: George Lebl <jirka@5z.com>
 */

#ifndef ICON_ENTRY_HACK_H
#define ICON_ENTRY_HACK_H

#include <libgnomeui/gnome-ditem-edit.h>
#include <libgnomeui/gnome-icon-item.h>

void	hack_ditem_edit		        (GnomeDItemEdit *dedit);
void	hack_icon_entry		        (GnomeIconEntry *ientry);
char *	hack_icon_entry_get_icon	(GnomeIconEntry *ientry);
void 	hack_icon_entry_set_icon	(GnomeIconEntry *ientry,
					 const char *icon);

#endif /* ICON_ENTRY_HACK_H */
