#ifndef EXTERN_H
#define EXTERN_H

#include <glib.h>
#include "panel.h"
#include "panel-widget.h"
#include "panel2.h"

#include <libgnorba/gnorba.h>

BEGIN_GNOME_DECLS

typedef struct _Extern Extern;
struct _Extern {
	POA_GNOME_PanelSpot servant;
        GNOME_Applet applet;

	char *goad_id;
	char *cfg;
	GtkWidget *ebox;

	AppletInfo *info;
};
void extern_clean(Extern *ext);

void load_extern_applet(char *goad_id, char *cfgpath, PanelWidget *panel, int pos);

/*stuff for corba*/
CORBA_PanelSpot add_applet (CORBA_Applet applet_obj,
			    const char *goad_id,
			    char **cfgpath,
			    char **globcfgpath,
			    guint32 *winid);
void applet_register (AppletInfo *info);
guint32 reserve_applet_spot (Extern *ext, PanelWidget *panel, int pos,
			     AppletType type);
void applet_abort_id(AppletInfo *info);
int applet_get_panel(AppletInfo *info);
int applet_get_pos(AppletInfo *info);
PanelOrientType applet_get_panel_orient(AppletInfo *info);
void applet_show_menu(AppletInfo *info);
void applet_drag_start(AppletInfo *info);
void applet_drag_stop(AppletInfo *info);
void applet_set_tooltip(AppletInfo *info, const char *tooltip);

/*start the next applet in queue*/
void extern_start_next(void);

END_GNOME_DECLS

#endif
