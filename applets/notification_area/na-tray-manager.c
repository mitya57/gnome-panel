/* na-tray-manager.c
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Used to be: eggtraymanager.c
 */

#include <config.h>
#include <string.h>
#include <libintl.h>

#include "na-tray-manager.h"

#include <gdkconfig.h>
#include <glib/gi18n.h>
#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#endif
#include <gtk/gtkinvisible.h>
#include <gtk/gtksocket.h>
#include <gtk/gtkwindow.h>

#include "na-marshal.h"

/* Signals */
enum
{
  TRAY_ICON_ADDED,
  TRAY_ICON_REMOVED,
  MESSAGE_SENT,
  MESSAGE_CANCELLED,
  LOST_SELECTION,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ORIENTATION
};

typedef struct
{
  long id, len;
  long remaining_len;
  
  long timeout;
  char *str;
#ifdef GDK_WINDOWING_X11
  Window window;
#endif
} PendingMessage;

static GObjectClass *parent_class = NULL;
static guint manager_signals[LAST_SIGNAL] = { 0 };

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define SYSTEM_TRAY_ORIENTATION_HORZ 0
#define SYSTEM_TRAY_ORIENTATION_VERT 1

#ifdef GDK_WINDOWING_X11
static gboolean na_tray_manager_check_running_xscreen (Screen *xscreen);
#endif

static void na_tray_manager_init (NaTrayManager *manager);
static void na_tray_manager_class_init (NaTrayManagerClass *klass);

static void na_tray_manager_finalize     (GObject      *object);
static void na_tray_manager_set_property (GObject      *object,
					  guint         prop_id,
					  const GValue *value,
					  GParamSpec   *pspec);
static void na_tray_manager_get_property (GObject      *object,
					  guint         prop_id,
					  GValue       *value,
					  GParamSpec   *pspec);

static void na_tray_manager_unmanage (NaTrayManager *manager);

GType
na_tray_manager_get_type (void)
{
  static GType our_type = 0;

  if (our_type == 0)
    {
      static const GTypeInfo our_info =
      {
	sizeof (NaTrayManagerClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) na_tray_manager_class_init,
	NULL, /* class_finalize */
	NULL, /* class_data */
	sizeof (NaTrayManager),
	0,    /* n_preallocs */
	(GInstanceInitFunc) na_tray_manager_init
      };

      our_type = g_type_register_static (G_TYPE_OBJECT, "NaTrayManager", &our_info, 0);
    }

  return our_type;

}

static void
na_tray_manager_init (NaTrayManager *manager)
{
  manager->invisible = NULL;
  manager->socket_table = g_hash_table_new (NULL, NULL);
}

static void
na_tray_manager_class_init (NaTrayManagerClass *klass)
{
  GObjectClass *gobject_class;
  
  parent_class = g_type_class_peek_parent (klass);
  gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = na_tray_manager_finalize;
  gobject_class->set_property = na_tray_manager_set_property;
  gobject_class->get_property = na_tray_manager_get_property;

  g_object_class_install_property (gobject_class,
				   PROP_ORIENTATION,
				   g_param_spec_enum ("orientation",
						      _("Orientation"),
						      _("The orientation of the tray."),
						      GTK_TYPE_ORIENTATION,
						      GTK_ORIENTATION_HORIZONTAL,
						      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  
  manager_signals[TRAY_ICON_ADDED] =
    g_signal_new ("tray_icon_added",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, tray_icon_added),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_SOCKET);

  manager_signals[TRAY_ICON_REMOVED] =
    g_signal_new ("tray_icon_removed",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, tray_icon_removed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_SOCKET);
  manager_signals[MESSAGE_SENT] =
    g_signal_new ("message_sent",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, message_sent),
		  NULL, NULL,
		  _na_marshal_VOID__OBJECT_STRING_LONG_LONG,
		  G_TYPE_NONE, 4,
		  GTK_TYPE_SOCKET,
		  G_TYPE_STRING,
		  G_TYPE_LONG,
		  G_TYPE_LONG);
  manager_signals[MESSAGE_CANCELLED] =
    g_signal_new ("message_cancelled",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, message_cancelled),
		  NULL, NULL,
		  _na_marshal_VOID__OBJECT_LONG,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_SOCKET,
		  G_TYPE_LONG);
  manager_signals[LOST_SELECTION] =
    g_signal_new ("lost_selection",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NaTrayManagerClass, lost_selection),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

#if defined (GDK_WINDOWING_X11)
  /* Nothing */
#elif defined (GDK_WINDOWING_WIN32)
  g_warning ("Port NaTrayManager to Win32");
#else
  g_warning ("Port NaTrayManager to this GTK+ backend");
#endif
}

static void
na_tray_manager_finalize (GObject *object)
{
  NaTrayManager *manager;
  
  manager = NA_TRAY_MANAGER (object);

  na_tray_manager_unmanage (manager);

  g_list_free (manager->messages);
  g_hash_table_destroy (manager->socket_table);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
na_tray_manager_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  NaTrayManager *manager = NA_TRAY_MANAGER (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      na_tray_manager_set_orientation (manager, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
na_tray_manager_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  NaTrayManager *manager = NA_TRAY_MANAGER (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, manager->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

NaTrayManager *
na_tray_manager_new (void)
{
  NaTrayManager *manager;

  manager = g_object_new (NA_TYPE_TRAY_MANAGER, NULL);

  return manager;
}

#ifdef GDK_WINDOWING_X11

static gboolean
na_tray_manager_plug_removed (GtkSocket       *socket,
			      NaTrayManager   *manager)
{
  Window *window;

  window = g_object_get_data (G_OBJECT (socket), "na-tray-child-window");

  g_hash_table_remove (manager->socket_table, GINT_TO_POINTER (*window));
  g_object_set_data (G_OBJECT (socket), "na-tray-child-window",
		     NULL);
  
  g_signal_emit (manager, manager_signals[TRAY_ICON_REMOVED], 0, socket);

  /* This destroys the socket. */
  return FALSE;
}

static void
na_tray_manager_handle_dock_request (NaTrayManager       *manager,
				     XClientMessageEvent *xevent)
{
  GtkWidget *socket;
  Window *window;
  GtkRequisition req;

  if (g_hash_table_lookup (manager->socket_table, GINT_TO_POINTER (xevent->data.l[2])))
    {
      /* We already got this notification earlier, ignore this one */
      return;
    }
  
  socket = gtk_socket_new ();
  
  /* We need to set the child window here
   * so that the client can call _get functions
   * in the signal handler
   */
  window = g_new (Window, 1);
  *window = xevent->data.l[2];
      
  g_object_set_data_full (G_OBJECT (socket),
			  "na-tray-child-window",
			  window, g_free);
  g_signal_emit (manager, manager_signals[TRAY_ICON_ADDED], 0,
		 socket);

  /* Add the socket only if it's been attached */
  if (GTK_IS_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (socket))))
    {
      g_signal_connect (socket, "plug_removed",
			G_CALLBACK (na_tray_manager_plug_removed), manager);
      
      gtk_socket_add_id (GTK_SOCKET (socket), *window);

      g_hash_table_insert (manager->socket_table, GINT_TO_POINTER (*window), socket);

      /*
       * Make sure the icons have a meaningfull size ...
       */ 
      req.width = req.height = 1;
      gtk_widget_size_request (socket, &req);
      /*
      if ((req.width < 16) || (req.height < 16))
      {
          gint nw = MAX (24, req.width);
          gint nh = MAX (24, req.height);
          g_warning (_("tray icon has requested a size of (%i x %i), resizing to (%i x %i)"), 
                      req.width, req.height, nw, nh);
          gtk_widget_set_size_request(icon, nw,  nh);
      }
      */
      gtk_widget_show(socket);
    }
  else
    gtk_widget_destroy (socket);
}

static void
pending_message_free (PendingMessage *message)
{
  g_free (message->str);
  g_free (message);
}

static void
na_tray_manager_handle_message_data (NaTrayManager       *manager,
				     XClientMessageEvent *xevent)
{
  GList *p;
  int len;
  
  /* Try to see if we can find the
   * pending message in the list
   */
  for (p = manager->messages; p; p = p->next)
    {
      PendingMessage *msg = p->data;

      if (xevent->window == msg->window)
	{
	  /* Append the message */
	  len = MIN (msg->remaining_len, 20);

	  memcpy ((msg->str + msg->len - msg->remaining_len),
		  &xevent->data, len);
	  msg->remaining_len -= len;

	  if (msg->remaining_len == 0)
	    {
	      GtkSocket *socket;

	      socket = g_hash_table_lookup (manager->socket_table, GINT_TO_POINTER (msg->window));

	      if (socket)
		{
		  g_signal_emit (manager, manager_signals[MESSAGE_SENT], 0,
				 socket, msg->str, msg->id, msg->timeout);
		}
	      manager->messages = g_list_remove_link (manager->messages,
						      p);
	      
	      pending_message_free (msg);
	    }

	  return;
	}
    }
}

static void
na_tray_manager_handle_begin_message (NaTrayManager       *manager,
				      XClientMessageEvent *xevent)
{
  GList *p;
  PendingMessage *msg;

  /* Check if the same message is
   * already in the queue and remove it if so
   */
  for (p = manager->messages; p; p = p->next)
    {
      PendingMessage *msg = p->data;

      if (xevent->window == msg->window &&
	  xevent->data.l[4] == msg->id)
	{
	  /* Hmm, we found it, now remove it */
	  pending_message_free (msg);
	  manager->messages = g_list_remove_link (manager->messages, p);
	  break;
	}
    }

  /* Now add the new message to the queue */
  msg = g_new0 (PendingMessage, 1);
  msg->window = xevent->window;
  msg->timeout = xevent->data.l[2];
  msg->len = xevent->data.l[3];
  msg->id = xevent->data.l[4];
  msg->remaining_len = msg->len;
  msg->str = g_malloc (msg->len + 1);
  msg->str[msg->len] = '\0';
  manager->messages = g_list_prepend (manager->messages, msg);
}

static void
na_tray_manager_handle_cancel_message (NaTrayManager       *manager,
				       XClientMessageEvent *xevent)
{
  GtkSocket *socket;
  
  socket = g_hash_table_lookup (manager->socket_table, GINT_TO_POINTER (xevent->window));
  
  if (socket)
    {
      g_signal_emit (manager, manager_signals[MESSAGE_CANCELLED], 0,
		     socket, xevent->data.l[2]);
    }
}

static GdkFilterReturn
na_tray_manager_handle_event (NaTrayManager       *manager,
			      XClientMessageEvent *xevent)
{
  switch (xevent->data.l[1])
    {
    case SYSTEM_TRAY_REQUEST_DOCK:
      na_tray_manager_handle_dock_request (manager, xevent);
      return GDK_FILTER_REMOVE;

    case SYSTEM_TRAY_BEGIN_MESSAGE:
      na_tray_manager_handle_begin_message (manager, xevent);
      return GDK_FILTER_REMOVE;

    case SYSTEM_TRAY_CANCEL_MESSAGE:
      na_tray_manager_handle_cancel_message (manager, xevent);
      return GDK_FILTER_REMOVE;
    default:
      break;
    }

  return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
na_tray_manager_window_filter (GdkXEvent *xev, GdkEvent *event, gpointer data)
{
  XEvent *xevent = (GdkXEvent *)xev;
  NaTrayManager *manager = data;

  if (xevent->type == ClientMessage)
    {
      if (xevent->xclient.message_type == manager->opcode_atom)
	{
	  return na_tray_manager_handle_event (manager, (XClientMessageEvent *)xevent);
	}
      else if (xevent->xclient.message_type == manager->message_data_atom)
	{
	  na_tray_manager_handle_message_data (manager, (XClientMessageEvent *)xevent);
	  return GDK_FILTER_REMOVE;
	}
    }
  else if (xevent->type == SelectionClear)
    {
      g_signal_emit (manager, manager_signals[LOST_SELECTION], 0);
      na_tray_manager_unmanage (manager);
    }
  return GDK_FILTER_CONTINUE;
}

#endif  

static void
na_tray_manager_unmanage (NaTrayManager *manager)
{
#ifdef GDK_WINDOWING_X11
  Display *display;
  guint32 timestamp;
  GtkWidget *invisible;

  if (manager->invisible == NULL)
    return;

  invisible = manager->invisible;
  g_assert (GTK_IS_INVISIBLE (invisible));
  g_assert (GTK_WIDGET_REALIZED (invisible));
  g_assert (GDK_IS_WINDOW (invisible->window));
  
  display = GDK_WINDOW_XDISPLAY (invisible->window);
  
  if (XGetSelectionOwner (display, manager->selection_atom) ==
      GDK_WINDOW_XWINDOW (invisible->window))
    {
      timestamp = gdk_x11_get_server_time (invisible->window);      
      XSetSelectionOwner (display, manager->selection_atom, None, timestamp);
    }

  gdk_window_remove_filter (invisible->window, na_tray_manager_window_filter, manager);  

  manager->invisible = NULL; /* prior to destroy for reentrancy paranoia */
  gtk_widget_destroy (invisible);
  g_object_unref (G_OBJECT (invisible));
#endif
}

static void
na_tray_manager_set_orientation_property (NaTrayManager *manager)
{
#ifdef GDK_WINDOWING_X11
  gulong data[1];

  if (!manager->invisible || !manager->invisible->window)
    return;

  g_assert (manager->orientation_atom != None);

  data[0] = manager->orientation == GTK_ORIENTATION_HORIZONTAL ?
		SYSTEM_TRAY_ORIENTATION_HORZ :
		SYSTEM_TRAY_ORIENTATION_VERT;

  XChangeProperty (GDK_WINDOW_XDISPLAY (manager->invisible->window),
		   GDK_WINDOW_XWINDOW (manager->invisible->window),
		   manager->orientation_atom,
		   XA_CARDINAL, 32,
		   PropModeReplace,
		   (guchar *) &data, 1);
#endif
}

#ifdef GDK_WINDOWING_X11

static gboolean
na_tray_manager_manage_xscreen (NaTrayManager *manager, Screen *xscreen)
{
  GtkWidget *invisible;
  char *selection_atom_name;
  guint32 timestamp;
  GdkScreen *screen;
  
  g_return_val_if_fail (NA_IS_TRAY_MANAGER (manager), FALSE);
  g_return_val_if_fail (manager->screen == NULL, FALSE);

  /* If there's already a manager running on the screen
   * we can't create another one.
   */
#if 0
  if (na_tray_manager_check_running_xscreen (xscreen))
    return FALSE;
#endif
  screen = gdk_display_get_screen (gdk_x11_lookup_xdisplay (DisplayOfScreen (xscreen)),
				   XScreenNumberOfScreen (xscreen));
  
  invisible = gtk_invisible_new_for_screen (screen);
  gtk_widget_realize (invisible);
  
  gtk_widget_add_events (invisible, GDK_PROPERTY_CHANGE_MASK | GDK_STRUCTURE_MASK);

  selection_atom_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
					 XScreenNumberOfScreen (xscreen));
  manager->selection_atom = XInternAtom (DisplayOfScreen (xscreen), selection_atom_name, False);

  g_free (selection_atom_name);

  manager->orientation_atom = XInternAtom (DisplayOfScreen (xscreen),
					   "_NET_SYSTEM_TRAY_ORIENTATION",
					   FALSE);
  na_tray_manager_set_orientation_property (manager);
  
  timestamp = gdk_x11_get_server_time (invisible->window);
  XSetSelectionOwner (DisplayOfScreen (xscreen), manager->selection_atom,
		      GDK_WINDOW_XWINDOW (invisible->window), timestamp);

  /* Check if we were could set the selection owner successfully */
  if (XGetSelectionOwner (DisplayOfScreen (xscreen), manager->selection_atom) ==
      GDK_WINDOW_XWINDOW (invisible->window))
    {
      XClientMessageEvent xev;

      xev.type = ClientMessage;
      xev.window = RootWindowOfScreen (xscreen);
      xev.message_type = XInternAtom (DisplayOfScreen (xscreen), "MANAGER", False);

      xev.format = 32;
      xev.data.l[0] = timestamp;
      xev.data.l[1] = manager->selection_atom;
      xev.data.l[2] = GDK_WINDOW_XWINDOW (invisible->window);
      xev.data.l[3] = 0;	/* manager specific data */
      xev.data.l[4] = 0;	/* manager specific data */

      XSendEvent (DisplayOfScreen (xscreen),
		  RootWindowOfScreen (xscreen),
		  False, StructureNotifyMask, (XEvent *)&xev);

      manager->invisible = invisible;
      g_object_ref (G_OBJECT (manager->invisible));
      
      manager->opcode_atom = XInternAtom (DisplayOfScreen (xscreen),
					  "_NET_SYSTEM_TRAY_OPCODE",
					  False);

      manager->message_data_atom = XInternAtom (DisplayOfScreen (xscreen),
						"_NET_SYSTEM_TRAY_MESSAGE_DATA",
						False);

      /* Add a window filter */
      gdk_window_add_filter (invisible->window, na_tray_manager_window_filter, manager);
      return TRUE;
    }
  else
    {
      gtk_widget_destroy (invisible);
 
      return FALSE;
    }
}

#endif

gboolean
na_tray_manager_manage_screen (NaTrayManager *manager,
			       GdkScreen     *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (manager->screen == NULL, FALSE);

#ifdef GDK_WINDOWING_X11
  return na_tray_manager_manage_xscreen (manager, 
					 GDK_SCREEN_XSCREEN (screen));
#else
  return FALSE;
#endif
}

#ifdef GDK_WINDOWING_X11

static gboolean
na_tray_manager_check_running_xscreen (Screen *xscreen)
{
  Atom selection_atom;
  char *selection_atom_name;

  selection_atom_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
					 XScreenNumberOfScreen (xscreen));
  selection_atom = XInternAtom (DisplayOfScreen (xscreen), selection_atom_name, False);
  g_free (selection_atom_name);

  if (XGetSelectionOwner (DisplayOfScreen (xscreen), selection_atom) != None)
    return TRUE;
  else
    return FALSE;
}

#endif

gboolean
na_tray_manager_check_running (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

#ifdef GDK_WINDOWING_X11
  return na_tray_manager_check_running_xscreen (GDK_SCREEN_XSCREEN (screen));
#else
  return FALSE;
#endif
}

char *
na_tray_manager_get_child_title (NaTrayManager      *manager,
				 NaTrayManagerChild *child)
{
  char *retval = NULL;
#ifdef GDK_WINDOWING_X11
  Window *child_window;
  Atom utf8_string, atom, type;
  int result;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *val;

  g_return_val_if_fail (NA_IS_TRAY_MANAGER (manager), NULL);
  g_return_val_if_fail (GTK_IS_SOCKET (child), NULL);
  
  child_window = g_object_get_data (G_OBJECT (child),
				    "na-tray-child-window");

  utf8_string = XInternAtom (GDK_DISPLAY (), "UTF8_STRING", False);
  atom = XInternAtom (GDK_DISPLAY (), "_NET_WM_NAME", False);

  gdk_error_trap_push ();

  result = XGetWindowProperty (GDK_DISPLAY (),
			       *child_window,
			       atom,
			       0, G_MAXLONG,
			       False, utf8_string,
			       &type, &format, &nitems,
			       &bytes_after, (guchar **)&val);
  
  if (gdk_error_trap_pop () || result != Success)
    return NULL;

  if (type != utf8_string ||
      format != 8 ||
      nitems == 0)
    {
      if (val)
	XFree (val);
      return NULL;
    }

  if (!g_utf8_validate (val, nitems, NULL))
    {
      XFree (val);
      return NULL;
    }

  retval = g_strndup (val, nitems);

  XFree (val);
#endif
  return retval;
}

void
na_tray_manager_set_orientation (NaTrayManager  *manager,
				 GtkOrientation  orientation)
{
  g_return_if_fail (NA_IS_TRAY_MANAGER (manager));

  if (manager->orientation != orientation)
    {
      manager->orientation = orientation;

      na_tray_manager_set_orientation_property (manager);

      g_object_notify (G_OBJECT (manager), "orientation");
    }
}

GtkOrientation
na_tray_manager_get_orientation (NaTrayManager *manager)
{
  g_return_val_if_fail (NA_IS_TRAY_MANAGER (manager), GTK_ORIENTATION_HORIZONTAL);

  return manager->orientation;
}