/* Gnome panel: extern applet functions
 * (C) 1997 the Free Software Foundation
 *
 * Authors:  George Lebl
 *           Federico Mena
 *           Miguel de Icaza
 */

#include <gdk/gdkx.h>
#include <config.h>
#include <string.h>
#include <signal.h>
#include <gnome.h>

#include "panel-include.h"
#include "gnome-panel.h"

#define APPLET_EVENT_MASK (GDK_BUTTON_PRESS_MASK |		\
			   GDK_BUTTON_RELEASE_MASK |		\
			   GDK_POINTER_MOTION_MASK |		\
			   GDK_POINTER_MOTION_HINT_MASK)

extern GList *applets;
extern GList *applets_last;
extern int applet_count;

extern GtkTooltips *panel_tooltips;

extern GlobalConfig global_config;

extern char *panel_cfg_path;
extern char *old_panel_cfg_path;

static GList *extern_applets = NULL;
static char *goad_id_starting = NULL; /*the goad id of the applet that is
					 being started right now, before it
					 does applet_register*/
static GList *start_queue = NULL; /*the queue of the applets to be
				    started*/
static int start_timeout = -1; /*id of the timeout for starting new applet*/

CORBA_ORB orb = NULL;
CORBA_Environment ev;
PortableServer_POA thepoa;

static int
start_timeout_handler(gpointer data)
{
	start_timeout = -1;
	extern_start_next();
	return FALSE;
}


/*queue up a new goad id to start or start it if nothing else is
  starting*/
static void
extern_start_new_goad_id(Extern *e)
{
        CORBA_Environment ev;
	if(!goad_id_starting) {
		CORBA_Object_release(goad_server_activate_with_id(NULL, e->goad_id, GOAD_ACTIVATE_NEW_ONLY|GOAD_ACTIVATE_ASYNC, NULL),&ev);
		goad_id_starting = g_strdup(e->goad_id);
	} else {
		if(start_timeout>-1)
			gtk_timeout_remove(start_timeout);
		start_timeout = -1;
		start_queue = g_list_append(start_queue,e);
		start_timeout = gtk_timeout_add(100*1000,
						start_timeout_handler,NULL);
	}
}

void
extern_start_next(void)
{
	Extern *e;
	if(goad_id_starting)
		g_free(goad_id_starting);
	goad_id_starting = NULL;
	if(!start_queue)
		return;
	e = start_queue->data;
	start_queue = g_list_remove(start_queue,e);
	
	extern_start_new_goad_id(e);
}


void
extern_clean(Extern *ext)
{
	extern_applets = g_list_remove(extern_applets,ext);

	if(ext->applet != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;
		CORBA_exception_init(&ev);
		CORBA_Object_release(ext->applet, &ev);
		CORBA_exception_free(&ev);
	}
	/*FIXME: is this correct???*/
	if(ext->servant != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;
		CORBA_exception_init(&ev);
		CORBA_Object_release(ext->servant, &ev);
		CORBA_exception_free(&ev);
	}

	g_free(ext->goad_id);
	g_free(ext->cfg);
	g_free(ext);
}




static int
extern_socket_destroy(GtkWidget *w, gpointer data)
{
	Extern *ext = data;
	gtk_widget_destroy(ext->ebox);
	extern_clean(ext);
	return FALSE;
}

/*note that type should be APPLET_EXTERN_RESERVED or APPLET_EXTERN_PENDING
  only*/
guint32
reserve_applet_spot (Extern *ext, PanelWidget *panel, int pos,
		     AppletType type)
{
	GtkWidget *socket;

	ext->ebox = gtk_event_box_new();
	gtk_widget_set_events(ext->ebox, (gtk_widget_get_events(ext->ebox) |
					  APPLET_EVENT_MASK) &
			      ~( GDK_POINTER_MOTION_MASK |
				 GDK_POINTER_MOTION_HINT_MASK));

	socket = gtk_socket_new();

	g_return_val_if_fail(socket!=NULL,0);

	gtk_container_add(GTK_CONTAINER(ext->ebox),socket);

	gtk_signal_connect(GTK_OBJECT(socket),"destroy",
			   GTK_SIGNAL_FUNC(extern_socket_destroy),
			   ext);

	gtk_widget_show_all (ext->ebox);
	
	/*we save the obj in the id field of the appletinfo and the 
	  path in the path field */
	if(!register_toy(ext->ebox,ext,panel,pos,type)) {
		g_warning("Couldn't add applet");
		return 0;
	}
	
	if(!GTK_WIDGET_REALIZED(socket))
		gtk_widget_realize(socket);

	return GDK_WINDOW_XWINDOW(socket->window);
}

void
load_extern_applet(char *goad_id, char *cfgpath, PanelWidget *panel, int pos)
{
	Extern *ext;
	POA_GNOME_PanelSpot *panelspot_servant;
	PortableServer_ObjectId objid = {0, sizeof("PanelSpot"), "PanelSpot" };

	if(!cfgpath || !*cfgpath)
		cfgpath = g_copy_strings(old_panel_cfg_path,
					 "Applet_Dummy/",NULL);
	else
		/*we will free this lateer*/
		cfgpath = g_strdup(cfgpath);
	
	ext = g_new(Extern,1);

	panelspot_servant = (POA_GNOME_Panel *)ext;
	panelspot_servant->_private = NULL;
	panelspot_servant->vepv = &panelspot_vepv;

	POA_GNOME_PanelSpot__init(panelspot_servant, &ev);
	
	PortableServer_POA_activate_object_with_id(thepoa, &objid, panelspot_servant, &ev);
	g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

	ext->applet = CORBA_OBJECT_NIL;
	ext->goad_id = g_strdup(goad_id);
	ext->cfg = cfgpath;
	extern_applets = g_list_prepend(extern_applets,ext);

	if(reserve_applet_spot (ext, panel, pos, APPLET_EXTERN_PENDING)==0) {
		g_warning("Whoops! for some reason we can't add "
			  "to the panel");
		extern_clean(ext);
		return;
	}

	extern_start_new_goad_id(goad_id);
}

/********************* CORBA Stuff *******************/

/***Panel stuff***/
static CORBA_short
s_panel_add_applet(POA_GNOME_Panel *servant,
		   CORBA_Applet panel_applet,
		   CORBA_char *goad_id,
		   CORBA_char ** cfgpath,
		   CORBA_char ** globcfgpath,
		   CORBA_unsigned_long* wid,
		   CORBA_Environment *ev);

static CORBA_short
s_panel_add_applet_full(POA_GNOME_Panel *servant,
			CORBA_Applet panel_applet,
			CORBA_char *goad_id,
			CORBA_short panel,
			CORBA_short pos,
			CORBA_char ** cfgpath,
			CORBA_char ** globcfgpath,
			CORBA_unsigned_long* wid,
			CORBA_Environment *ev);

static void
s_panel_quit(POA_GNOME_Panel *servant, CORBA_Environment *ev);

static CORBA_boolean
s_panel_get_in_drag(POA_GNOME_Panel *servant, CORBA_Environment *ev);



/*** PanelSpot stuff ***/

static CORBA_char *
s_panelspot_get_tooltip(POA_GNOME_PanelSpot *servant,
			CORBA_Environment *ev);

static void
s_panelspot_set_tooltip(POA_GNOME_PanelSpot *servant,
			CORBA_char *val,
			CORBA_Environment *ev);

static CORBA_short
s_panelspot_get_parent_panel(POA_GNOME_PanelSpot *servant,
			     CORBA_Environment *ev);

static CORBA_short
s_panelspot_get_spot_pos(POA_GNOME_PanelSpot *servant,
			 CORBA_Environment *ev);

static GNOME_Panel_OrientType
s_panelspot_get_parent_orient(POA_GNOME_PanelSpot *servant,
			      CORBA_Environment *ev);

static void
s_panelspot_register(POA_GNOME_PanelSpot *servant,
		     CORBA_Environment *ev);

static void
s_panelspot_unregister(POA_GNOME_PanelSpot *servant,
		       CORBA_Environment *ev);

static void
s_panelspot_abort_load(POA_GNOME_PanelSpot *servant,
		       CORBA_Environment *ev);

static void
s_panelspot_show_menu(POA_GNOME_PanelSpot *servant,
		      CORBA_Environment *ev);

static void
s_panelspot_drag_start(POA_GNOME_PanelSpot *servant,
		       CORBA_Environment *ev);

static void
s_panelspot_drag_stop(POA_GNOME_PanelSpot *servant,
		      CORBA_Environment *ev);

static void
s_panelspot_add_callback(POA_GNOME_PanelSpot *servant,
			 CORBA_char *callback_name,
			 CORBA_char *stock_item,
			 CORBA_char *menuitem_text,
			 CORBA_Environment *ev);

static void
s_panelspot_remove_callback(POA_GNOME_PanelSpot *servant,
			    CORBA_char *callback_name,
			    CORBA_Environment *ev);

static void
s_panelspot_sync_config(POA_GNOME_PanelSpot *servant,
			CORBA_Environment *ev);

static PortableServer_ServantBase__epv panel_base_epv = {
  NULL, /* _private */
  NULL, /* finalize */
  NULL, /* use base default_POA function */
};

static POA_GNOME_Panel__epv panel_epv = {
  NULL, /* private data */
  (gpointer)&s_panel_add_applet,
  (gpointer)&s_panel_add_applet_full,
  (gpointer)&s_panel_quit,
  (gpointer)&s_panel_get_indrag
};
static POA_GNOME_Panel__vepv panel_vepv = { &panel_base_epv, &panel_epv };
static POA_GNOME_Panel panel_servant = { NULL, &panel_vepv };


static PortableServer_ServantBase__epv panelspot_base_epv = {
  NULL, /* _private */
  NULL, /* finalize */
  NULL, /* use base default_POA function */
};

static POA_GNOME_PanelSpot__epv panelspot_epv = {
  NULL, /* private data */
  (gpointer)&s_panelspot_get_tooltip,
  (gpointer)&s_panelspot_set_tooltip,
  (gpointer)&s_panelspot_get_parent_panel,
  (gpointer)&s_panelspot_get_spot_pos,
  (gpointer)&s_panelspot_get_parent_orient,
  (gpointer)&s_panelspot_register,
  (gpointer)&s_panelspot_unregister,
  (gpointer)&s_panelspot_abort_load,
  (gpointer)&s_panelspot_start_menu,
  (gpointer)&s_panelspot_drag_start,
  (gpointer)&s_panelspot_drag_stop,
  (gpointer)&s_panelspot_add_callback,
  (gpointer)&s_panelspot_remove_callback,
  (gpointer)&s_panelspot_sync_config
};
static POA_GNOME_Panel__vepv panelspot_vepv = { &panelspot_base_epv, &panelspot_epv };
//static POA_GNOME_Panel panelspot_servant = { NULL, &panelspot_vepv };


static CORBA_PanelSpot
s_panel_add_applet(POA_GNOME_Panel *servant,
		   CORBA_Applet panel_applet,
		   CORBA_char *goad_id,
		   CORBA_char ** cfgpath,
		   CORBA_char ** globcfgpath,
		   CORBA_unsigned_long* wid,
		   CORBA_Environment *ev)
{
	return s_panel_add_applet_full(servant,panel_applet,goad_id,0,0,
				       cfgpath,globcfgpath,wid,ev);
}

static CORBA_PanelSpot
s_panel_add_applet_full(POA_GNOME_Panel *servant,
			CORBA_Applet panel_applet,
			CORBA_char *goad_id,
			CORBA_short panel,
			CORBA_short pos,
			CORBA_char ** cfgpath,
			CORBA_char ** globcfgpath,
			CORBA_unsigned_long* wid,
			CORBA_Environment *ev)
{
	GList *li;
	int i;
	Extern *ext;
	char *p;
	POA_GNOME_PanelSpot *panelspot_servant;
	GNOME_PanelSpot acc;
	PortableServer_ObjectId objid = {0, sizeof("PanelSpot"), "PanelSpot" };
	
	for(li=applets;li!=NULL;li=g_list_next(li)) {
		AppletInfo *info = li->data;
		if(info && info->type == APPLET_EXTERN_PENDING) {
			Extern *ext = info->data;
			g_assert(ext);
			if(strcmp(ext->goad_id,goad_id)==0) {
				/*we started this and already reserved a spot
				  for it, including the socket widget*/
				GtkWidget *socket =
					GTK_BIN(info->widget)->child;
				g_return_val_if_fail(GTK_IS_SOCKET(socket),-1);

				ext->applet = panel_applet;
				*cfgpath = CORBA_string_dup(ext->cfg);
				g_free(ext->cfg);
				ext->cfg = NULL;
				*globcfgpath = CORBA_string_dup(old_panel_cfg_path);
				info->type = APPLET_EXTERN_RESERVED;
				*wid=GDK_WINDOW_XWINDOW(socket->window);

				panelspot_servant = (POA_GNOME_Panel *)ext;
				acc = PortableServer_POA_servant_to_reference(thepoa, panelspot_servant, &ev);
				g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

				return acc;
			}
		}
	}
	
	/*this is an applet that was started from outside, otherwise we would
	  have already reserved a spot for it*/
	ext = g_new(Extern,1);

	panelspot_servant = (POA_GNOME_Panel *)ext;
	panelspot_servant->_private = NULL;
	panelspot_servant->vepv = &panelspot_vepv;

	POA_GNOME_PanelSpot__init(panelspot_servant, &ev);
	
	PortableServer_POA_activate_object_with_id(thepoa, &objid, panelspot_servant, &ev);
	g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

	acc = PortableServer_POA_servant_to_reference(thepoa, panelspot_servant, &ev);
	g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

	ext->applet = applet_obj;
	ext->goad_id = g_strdup(goad_id);
	ext->cfg = NULL;
	extern_applets = g_list_prepend(extern_applets,ext);

	/*select the nth panel*/
	if(panel)
		li = g_list_nth(panels,panel);
	else
		li = NULL;
	if(!li)
		li = panels;

	*winid = reserve_applet_spot (ext, li->data, pos,
				      APPLET_EXTERN_RESERVED);
	if(*winid == 0) {
		extern_clean(ext);
		*globcfgpath = NULL;
		*cfgpath = NULL;
		return CORBA_OBJECT_NIL;
	}
	p = g_copy_strings(old_panel_cfg_path,"Applet_Dummy/",NULL);
	*cfgpath = CORBA_string_dup(p);
	g_free(p);
	*globcfgpath = CORBA_string_dup(old_panel_cfg_path);

	return acc;
}

static void
s_panel_quit(POA_GNOME_Panel *servant, CORBA_Environment *ev)
{
	panel_quit();
}

static CORBA_boolean
s_panel_get_in_drag(POA_GNOME_Panel *servant, CORBA_Environment *ev)
{
	return panel_applet_in_drag;
}



/*** PanelSpot stuff ***/

static CORBA_char *
s_panelspot_get_tooltip(POA_GNOME_PanelSpot *servant,
			CORBA_Environment *ev)
{
	/*FIXME: get the tooltip text*/
	return GNOME_string_dup("");
}

static void
s_panelspot_set_tooltip(POA_GNOME_PanelSpot *servant,
			CORBA_char *val,
			CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
	if(val && *val)
		gtk_tooltips_set_tip (panel_tooltips,ext->ebox,val,NULL);
	else
		gtk_tooltips_set_tip (panel_tooltips,ext->ebox,NULL,NULL);
}

static CORBA_short
s_panelspot_get_parent_panel(POA_GNOME_PanelSpot *servant,
			     CORBA_Environment *ev)
{
	int panel;
	GList *list;
	gpointer p;
	Extern *ext = (Extern *)servant;
	PanelWidget *panel;
	AppletInfo *info;

	g_assert(ext);
	
	info = ext->data;
	g_assert(info);

	p = PANEL_WIDGET(info->widget->parent);

	for(panel=0,list=panels;list!=NULL;list=g_list_next(list),panel++)
		if(list->data == p)
			return panel;
	return -1;
}

static CORBA_short
s_panelspot_get_spot_pos(POA_GNOME_PanelSpot *servant,
			 CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
	PanelWidget *panel;
	AppletInfo *info;
	AppletData *ad;

	g_assert(ext);
	
	info = ext->data;
	g_assert(info);
	
	ad = gtk_object_get_data(GTK_OBJECT(info->widget),
				 PANEL_APPLET_DATA);
	if(!ad)
		return -1;
	return ad->pos;
}

static GNOME_Panel_OrientType
s_panelspot_get_parent_orient(POA_GNOME_PanelSpot *servant,
			      CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
	PanelWidget *panel;
	AppletInfo *info;

	g_assert(ext);
	
	info = ext->data;
	g_assert(info);

	panel = PANEL_WIDGET(info->widget->parent);

	g_return_val_if_fail(panel != NULL,ORIENT_UP);

	return get_applet_orient(panel);
}

static void
s_panelspot_register(POA_GNOME_PanelSpot *servant,
		     CORBA_Environment *ev)
{
	PanelWidget *panel;
	Extern *ext = (Extern *)servant;
	AppletInfo *info;

	g_assert(ext);
	
	info = ext->data;
	g_assert(info);

	/*if we should start the next applet*/
	if(goad_id_starting && strcmp(goad_id,goad_id_starting)==0)
		extern_start_next();

	panel = PANEL_WIDGET(info->widget->parent);
	g_return_if_fail(panel!=NULL);

	/*no longer pending*/
	info->type = APPLET_EXTERN;

	orientation_change(info,panel);
	back_change(info,panel);
	send_applet_tooltips_state(ext->applet,
				   global_config.tooltips_enabled);
}

static void
s_panelspot_unregister(POA_GNOME_PanelSpot *servant,
		       CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
	panel_clean_applet(ext->info);
}

static void
s_panelspot_abort_load(POA_GNOME_PanelSpot *servant,
		       CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
	AppletInfo *info;

	g_return_if_fail(ext != NULL);

	info = ext->data;
	g_return_if_fail(info != NULL);
	
	if(goad_id_starting && strcmp(ext->goad_id,goad_id_starting)==0)
		extern_start_next();

	/*only reserved spots can be canceled, if an applet
	  wants to chance a pending applet it needs to first
	  user reserve spot to obtain id and make it EXTERN_RESERVED*/
	if(info->type != APPLET_EXTERN_RESERVED)
		return;

	panel_clean_applet(info);
}

static void
s_panelspot_show_menu(POA_GNOME_PanelSpot *servant,
		      CORBA_Environment *ev)
{
	GtkWidget *panel;
	Extern *ext = (Extern *)servant;
	AppletInfo *info;

	g_return_if_fail(ext != NULL);

	info = ext->data;
	g_return_if_fail(info != NULL);

	if (!info->menu)
		create_applet_menu(info);

	panel = get_panel_parent(info->widget);
	if(IS_SNAPPED_WIDGET(panel)) {
		SNAPPED_WIDGET(panel)->autohide_inhibit = TRUE;
		snapped_widget_queue_pop_down(SNAPPED_WIDGET(panel));
	}

	gtk_menu_popup(GTK_MENU(info->menu), NULL, NULL, applet_menu_position,
		       info, 3, GDK_CURRENT_TIME);
}


static void
s_panelspot_drag_start(POA_GNOME_PanelSpot *servant,
		       CORBA_Environment *ev)
{
	PanelWidget *panel;
	Extern *ext = (Extern *)servant;
	AppletInfo *info;

	g_return_if_fail(ext != NULL);

	info = ext->data;
	g_return_if_fail(info != NULL);

	panel = PANEL_WIDGET(info->widget->parent);

	g_return_if_fail(panel!=NULL);

	/*panel_widget_applet_drag_start(panel,info->widget);
	panel_widget_applet_drag_end(panel);*/
	panel_widget_applet_drag_start(panel,info->widget);
	panel_widget_applet_move_use_idle(panel);
}

static void
s_panelspot_drag_stop(POA_GNOME_PanelSpot *servant,
		      CORBA_Environment *ev)
{
	PanelWidget *panel;
	Extern *ext = (Extern *)servant;
	AppletInfo *info;

	g_return_if_fail(ext != NULL);

	info = ext->data;
	g_return_if_fail(info != NULL);

	panel = PANEL_WIDGET(info->widget->parent);

	g_return_if_fail(panel!=NULL);

	panel_widget_applet_drag_end(panel);
}

static void
s_panelspot_add_callback(POA_GNOME_PanelSpot *servant,
			 CORBA_char *callback_name,
			 CORBA_char *stock_item,
			 CORBA_char *menuitem_text,
			 CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
	AppletInfo *info;

	g_return_if_fail(ext != NULL);

	info = ext->data;
	g_return_if_fail(info != NULL);
	applet_add_callback(info, callback_name, stock_item, menuitem_text);
}

static void
s_panelspot_remove_callback(POA_GNOME_PanelSpot *servant,
			    CORBA_char *callback_name,
			    CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
	AppletInfo *info;

	g_return_if_fail(ext != NULL);

	info = ext->data;
	g_return_if_fail(info != NULL);
	applet_remove_callback(info, callback_name);
}

static void
s_panelspot_sync_config(POA_GNOME_PanelSpot *servant,
			CORBA_Environment *ev)
{
	Extern *ext = (Extern *)servant;
	AppletInfo *info;

	g_return_if_fail(ext != NULL);

	info = ext->data;
	g_return_if_fail(info != NULL);
	if(g_list_find(applets_to_sync, info)==NULL)
		applets_to_sync = g_list_prepend(applets_to_sync,info);
	panel_config_sync();
}

void
panel_corba_clean_up(void)
{
  CORBA_Object ns = gnome_name_service_get();
  goad_server_unregister(ns, "gnome_panel", "server", &ev);
  CORBA_Object_release(ns, &ev);
  CORBA_ORB_shutdown(orb, CORBA_FALSE, &ev);
}

void
panel_corba_gtk_init(CORBA_ORB panel_orb)
{
  PortableServer_ObjectId objid = {0, sizeof("Panel"), "Panel" };
  GNOME_Panel acc;
  char hostname [4096];
  char *name;
  CORBA_Object ns;

  CORBA_exception_init(&ev);

  orb = panel_orb;

  POA_GNOME_Panel__init(&servant, &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  thepoa = (PortableServer_POA)
    CORBA_ORB_resolve_initial_references(orb, "RootPOA", &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  PortableServer_POAManager_activate(PortableServer_POA__get_the_POAManager(thepoa, &ev), &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  PortableServer_POA_activate_object_with_id(thepoa, &objid, &servant, &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  acc = PortableServer_POA_servant_to_reference(thepoa, &servant, &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  ns = gnome_name_service_get();
  goad_server_register(ns, acc, "gnome_panel", "server", &ev);
  CORBA_Object_release(ns, &ev);

  if(goad_server_activation_id()) {
    CORBA_char *ior;
    ior = CORBA_ORB_object_to_string(orb, acc, &ev);
    printf("%s\n", ior); fflush(stdout);
    CORBA_free(ior);
  }

  CORBA_Object_release(acc, &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

  ORBit_custom_run_setup(orb, &ev);
  g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);
}
