/*
 * Copyright © 2006 Ryan Lortie
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Library General 
 * Public License as published by the Free Software Foundation.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * Authors:
 *     Ryan Lortie <desrt@desrt.ca>
 */

#include "gnome-applet-orientation.h"

#include "gnome-applet-layout-box.h"

G_DEFINE_TYPE (GnomeAppletLayoutBox, gnome_applet_layout_box, GTK_TYPE_ALIGNMENT);

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_APPLET_LAYOUT_BOX, \
   GnomeAppletLayoutBoxPrivate))

typedef enum
{
  LAYOUT_FLAT,
  LAYOUT_STACKED,
  LAYOUT_SIDEWAYS
} LayoutType;

typedef enum
{
  IMAGE_LEFT,
  IMAGE_TOP
} ImagePosition;

typedef struct
{
  GtkWidget *image;
  GnomeAppletLabel *label;
  GnomeAppletOrientation orientation;

  ImagePosition image_position;
  LayoutType layout;

  GtkTable *table;
} GnomeAppletLayoutBoxPrivate;

static void
gnome_applet_layout_box_move_image (GnomeAppletLayoutBox *box,
                                    ImagePosition position)
{
  GnomeAppletLayoutBoxPrivate *priv = GET_PRIVATE (box);
  int x, y;

  if (!priv->image || priv->image_position == position)
    return;

  if (position == IMAGE_TOP)
    x = 1, y = 0;
  else
    x = 0, y = 1;

  g_object_ref (priv->image);
  gtk_container_remove (GTK_CONTAINER (priv->table), priv->image);
  gtk_table_attach (priv->table, priv->image,
                    x, x + 1, y, y + 1,
                    0, 0, 0, 0);
  g_object_unref (priv->image);

  priv->image_position = position;
}

static LayoutType
gnome_applet_layout_box_decide_layout (GnomeAppletLayoutBox *box,
                                       GtkAllocation *allocation)
{
  GnomeAppletLayoutBoxPrivate *priv = GET_PRIVATE (box);

  if (gnome_applet_orientation_is_horizontal (priv->orientation))
  {
    int image_height = 0, text_height = 0;

    if (priv->image)
      image_height  = GTK_WIDGET (priv->image)->requisition.height;

    text_height = gnome_applet_label_get_thickness (priv->label);

    if (allocation->height > image_height + text_height)
      return LAYOUT_STACKED;

    return LAYOUT_FLAT;
  }
  else
  {
    int image_width = 0, text_width = 0;

    if (priv->image)
      image_width = GTK_WIDGET (priv->image)->requisition.width;

    text_width = gnome_applet_label_get_length (priv->label);

    if (allocation->width > image_width + text_width)
      return LAYOUT_FLAT;

    if (allocation->width > text_width)
      return LAYOUT_STACKED;

    return LAYOUT_SIDEWAYS;
  }
}

static void
gnome_applet_layout_box_configure (GnomeAppletLayoutBox *box,
                                   GtkAllocation *allocation)
{
  GnomeAppletLayoutBoxPrivate *priv = GET_PRIVATE (box);
  LayoutType layout;

  if (priv->label == NULL)
    return;

  layout = gnome_applet_layout_box_decide_layout (box, allocation);

  if (layout == priv->layout)
    return;

  switch (layout)
  {
    case LAYOUT_FLAT:
      gnome_applet_layout_box_move_image (box, IMAGE_LEFT);
      gnome_applet_label_set_sideways (priv->label, FALSE);
      break;

    case LAYOUT_STACKED:
      gnome_applet_layout_box_move_image (box, IMAGE_TOP);
      gnome_applet_label_set_sideways (priv->label, FALSE);
      break;

    case LAYOUT_SIDEWAYS:
      gnome_applet_layout_box_move_image (box, IMAGE_TOP);
      gnome_applet_label_set_sideways (priv->label, TRUE);
      break;
  }

  priv->layout = layout;
}

  
static void
gnome_applet_layout_box_size_allocate (GtkWidget *widget,
                                       GtkAllocation *allocation)
{
  GnomeAppletLayoutBox *box = GNOME_APPLET_LAYOUT_BOX (widget);
  gnome_applet_layout_box_configure (box, allocation);
  GTK_WIDGET_CLASS (gnome_applet_layout_box_parent_class)->size_allocate (widget, allocation);
}

static void
gnome_applet_layout_box_class_init (GnomeAppletLayoutBoxClass *class)
{
 // GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *wc = GTK_WIDGET_CLASS (class);

  wc->size_allocate = gnome_applet_layout_box_size_allocate;

  g_type_class_add_private (class, sizeof (GnomeAppletLayoutBoxPrivate));
}

static void
gnome_applet_layout_box_init (GnomeAppletLayoutBox *box)
{
  GnomeAppletLayoutBoxPrivate *priv = GET_PRIVATE (box);
  GtkWidget *table;

  table = gtk_table_new (2, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (box), table);
  priv->table = GTK_TABLE (table);
  gtk_widget_show (table);

  priv->orientation = GNOME_APPLET_ORIENTATION_TOP;
  priv->layout = LAYOUT_FLAT;

  gtk_alignment_set (GTK_ALIGNMENT (box), 0.5, 0.5, 0., 0.);
}

GnomeAppletLayoutBox *
gnome_applet_layout_box_new (void)
{
  return g_object_new (GNOME_TYPE_APPLET_LAYOUT_BOX, NULL);
}

void
gnome_applet_layout_box_set_orientation (GnomeAppletLayoutBox *box,
                                         GnomeAppletOrientation orientation)
{
  GnomeAppletLayoutBoxPrivate *priv = GET_PRIVATE (box);

  if (priv->orientation == orientation)
    return;

  priv->orientation = orientation;

  gnome_applet_layout_box_configure (box, &GTK_WIDGET (box)->allocation);
}

GtkImage *
gnome_applet_layout_box_request_image (GnomeAppletLayoutBox *box)
{
  GnomeAppletLayoutBoxPrivate *priv = GET_PRIVATE (box);

  if (!priv->image)
  {
    priv->image = gtk_image_new ();
    gtk_table_attach (priv->table, priv->image, 0, 1, 1, 2, 0, 0, 0, 0);
    gtk_widget_show (priv->image);
    priv->image_position = IMAGE_LEFT;
  }

  return GTK_IMAGE (priv->image);
}

GnomeAppletLabel *
gnome_applet_layout_box_request_label (GnomeAppletLayoutBox *box)
{
  GnomeAppletLayoutBoxPrivate *priv = GET_PRIVATE (box);

  if (!priv->label)
  {
    GtkWidget *label = gnome_applet_label_new (NULL);
    gtk_table_attach (priv->table, label, 1, 2, 1, 2, 0, 0, 0, 0);
    gtk_widget_show (label);

    priv->label = GNOME_APPLET_LABEL (label);
  }

  return priv->label;
}
