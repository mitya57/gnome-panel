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

/*start the next applet in queue*/
void extern_start_next(void);

END_GNOME_DECLS

#endif
