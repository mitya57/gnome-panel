#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cairo.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <gtk/gtk.h>
#include <math.h>

#include "clock.h"
#include "clock-map.h"
#include "clock-sunpos.h"
#include "clock-marshallers.h"

G_DEFINE_TYPE (ClockMap, clock_map, GTK_TYPE_WIDGET)

enum {
	NEED_LOCATIONS,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct {
        struct tm last_refresh;

        gint width;
        gint height;

        GdkPixbuf *stock_map_pixbuf;
        GdkPixbuf *location_marker_pixbuf[3];

        GdkPixbuf *location_map_pixbuf;

        /* The shadow itself */
        GdkPixbuf *shadow_pixbuf;

        /* The map with the shadow composited onto it */
        GdkPixbuf *shadow_map_pixbuf;

        /* The rotated shadow_map_pixbuf, ready for display */
        GdkPixbuf *rotated_map_pixbuf;
} ClockMapPrivate;

static void clock_map_finalize (GObject *);
static void clock_map_size_request (GtkWidget *this,
                                        GtkRequisition *requisition);
static void clock_map_size_allocate (GtkWidget *this,
					 GtkAllocation *allocation);
static gboolean clock_map_expose (GtkWidget *this,
				      GdkEventExpose *expose);

static void clock_map_place_locations (ClockMap *this);
static void clock_map_render_shadow (ClockMap *this);
static void clock_map_rotate (ClockMap *this);
static void clock_map_display (ClockMap *this);

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CLOCK_MAP_TYPE, ClockMapPrivate))

ClockMap *
clock_map_new (void)
{
        ClockMap *this;
        ClockMapPrivate *priv;

        this = g_object_new (CLOCK_MAP_TYPE, NULL);
        priv = PRIVATE (this);

        priv->location_marker_pixbuf[0] = gdk_pixbuf_new_from_file
                (ICONDIR "/clock-map-location-marker.png", NULL);
        priv->location_marker_pixbuf[1] = gdk_pixbuf_new_from_file
                (ICONDIR "/clock-map-location-hilight.png", NULL);
        priv->location_marker_pixbuf[2] = gdk_pixbuf_new_from_file
                (ICONDIR "/clock-map-location-current.png", NULL);

        clock_map_refresh (this);

        return this;
}

static void
clock_map_class_init (ClockMapClass *this_class)
{
        GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (g_obj_class);

        g_obj_class->finalize = clock_map_finalize;

        /* GtkWidget signals */
        widget_class->size_request = clock_map_size_request;
        widget_class->size_allocate = clock_map_size_allocate;
	widget_class->expose_event = clock_map_expose;

        g_type_class_add_private (this_class, sizeof (ClockMapPrivate));

	/**
	 * ClockMap::need-locations
	 *
	 * The map widget emits this signal when it needs to know which
	 * locations to display.
	 *
	 * Returns: the handler should return a (GList *) of (ClockLocation *).
	 * The map widget will not modify this list, so the caller should keep
	 * it alive.
	 */
	signals[NEED_LOCATIONS] = g_signal_new ("need-locations",
						G_TYPE_FROM_CLASS (g_obj_class),
						G_SIGNAL_RUN_LAST,
						G_STRUCT_OFFSET (ClockMapClass, need_locations),
						NULL,
						NULL,
						_clock_marshal_POINTER__VOID,
						G_TYPE_POINTER, 0);
}

static void
clock_map_init (ClockMap *this)
{
        ClockMapPrivate *priv = PRIVATE (this);

	GTK_WIDGET_SET_FLAGS (this, GTK_NO_WINDOW);

        priv->stock_map_pixbuf = NULL;
        priv->location_marker_pixbuf[0] = NULL;
        priv->location_marker_pixbuf[1] = NULL;
        priv->location_marker_pixbuf[2] = NULL;
}

static void
clock_map_finalize (GObject *g_obj)
{
        ClockMapPrivate *priv = PRIVATE (g_obj);
	int i;

        if (priv->stock_map_pixbuf) {
                gdk_pixbuf_unref (priv->stock_map_pixbuf);
                priv->stock_map_pixbuf = NULL;
        }

	for (i = 0; i < 3; i++) {
        	if (priv->location_marker_pixbuf[i]) {
                	gdk_pixbuf_unref (priv->location_marker_pixbuf[i]);
                	priv->location_marker_pixbuf[i] = NULL;
        	}
	}

        if (priv->location_map_pixbuf) {
                gdk_pixbuf_unref (priv->location_map_pixbuf);
                priv->location_map_pixbuf = NULL;
        }

        if (priv->shadow_pixbuf) {
                gdk_pixbuf_unref (priv->shadow_pixbuf);
                priv->shadow_pixbuf = NULL;
        }

        if (priv->shadow_map_pixbuf) {
                gdk_pixbuf_unref (priv->shadow_map_pixbuf);
                priv->shadow_map_pixbuf = NULL;
        }

        if (priv->rotated_map_pixbuf) {
                gdk_pixbuf_unref (priv->rotated_map_pixbuf);
                priv->rotated_map_pixbuf = NULL;
        }

        G_OBJECT_CLASS (clock_map_parent_class)->finalize (g_obj);
}

void
clock_map_refresh (ClockMap *this)
{
        ClockMapPrivate *priv = PRIVATE (this);
	GtkWidget *widget = GTK_WIDGET (this);
        GtkWidget *parent = gtk_widget_get_parent (widget);
	GtkRequisition req;
        gint width;
        gint height;

	gtk_widget_size_request (widget, &req);
        width = req.width;
        height = req.height;

        if (parent) {
		if (widget->allocation.width != 1) {
			width = widget->allocation.width;
			height = widget->allocation.height;
		}

                if (priv->stock_map_pixbuf) {
                        gdk_pixbuf_unref (priv->stock_map_pixbuf);
                        priv->stock_map_pixbuf = NULL;
                }
        }

        if (!priv->stock_map_pixbuf) {
                GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale
                        (ICONDIR "/clock-map.png",
                         width, height, FALSE, NULL);

                priv->width = gdk_pixbuf_get_width (pixbuf);
                priv->height = gdk_pixbuf_get_height (pixbuf);

                priv->stock_map_pixbuf = pixbuf;
        }

        clock_map_place_locations (this);

        clock_map_render_shadow (this);
        clock_map_rotate (this);

        clock_map_display (this);
}

static gboolean
clock_map_expose (GtkWidget *this, GdkEventExpose *event)
{
        ClockMapPrivate *priv = PRIVATE (this);
	GtkAllocation allocation = this->allocation;
	GdkRectangle region;
        cairo_t *cr;

	if (!priv->rotated_map_pixbuf)
		clock_map_refresh (CLOCK_MAP (this));

        cr = gdk_cairo_create (this->window);

	region.x = allocation.x;
	region.y = allocation.y;
	region.width = gdk_pixbuf_get_width (priv->rotated_map_pixbuf);
	region.height = gdk_pixbuf_get_height (priv->rotated_map_pixbuf);

	gdk_rectangle_intersect (&region, &(event->area), &region);
	gdk_draw_pixbuf (this->window,
			 this->style->black_gc,
			 priv->rotated_map_pixbuf,
			 region.x - allocation.x,
			 region.y - allocation.y,
			 region.x,
			 region.y,
			 region.width,
			 region.height,
			 GDK_RGB_DITHER_NORMAL,
			 0, 0);

        /* draw a simple outline */
        cairo_rectangle (
                cr,
                region.x + 0.5, region.y + 0.5,
                region.width - 1, region.height - 1);

        cairo_set_source_rgb (
                cr,
                this->style->mid [GTK_STATE_ACTIVE].red   / 65535.0,
                this->style->mid [GTK_STATE_ACTIVE].green / 65535.0,
                this->style->mid [GTK_STATE_ACTIVE].blue  / 65535.0);

        cairo_set_line_width (cr, 1.0);
        cairo_stroke (cr);

        cairo_destroy (cr);

	return FALSE;
}

static void
clock_map_size_request (GtkWidget *this, GtkRequisition *requisition)
{
        requisition->width = 250;
        requisition->height = 125;
}

static void
clock_map_size_allocate (GtkWidget *this, GtkAllocation *allocation)
{
        ClockMapPrivate *priv = PRIVATE (this);

	if (GTK_WIDGET_CLASS (clock_map_parent_class)->size_allocate)
		GTK_WIDGET_CLASS (clock_map_parent_class)->size_allocate (this, allocation);

	if (!priv->stock_map_pixbuf
	    || priv->width != this->allocation.width
	    || priv->height != this->allocation.height)
                clock_map_refresh (CLOCK_MAP (this));
}

static void
clock_map_mark (ClockMap *this, gfloat latitude, gfloat longitude, gint mark)
{
        ClockMapPrivate *priv = PRIVATE (this);
        GdkPixbuf *marker = priv->location_marker_pixbuf[mark];
        GdkPixbuf *partial = NULL;

        int x, y;
        int width, height;
        int marker_width, marker_height;
        int dest_x, dest_y, dest_width, dest_height;

        width = gdk_pixbuf_get_width (priv->location_map_pixbuf);
        height = gdk_pixbuf_get_height (priv->location_map_pixbuf);

        x = (width / 2.0 + (width / 2.0) * longitude / 180.0);
        y = (height / 2.0 - (height / 2.0) * latitude / 90.0);

        marker_width = gdk_pixbuf_get_width (marker);
        marker_height = gdk_pixbuf_get_height (marker);

        dest_x = x - marker_width / 2;
        dest_y = y - marker_height / 2;
        dest_width = marker_width;
        dest_height = marker_height;

        /* create a small partial pixbuf if the mark is too close to
           the north or south pole */
        if (dest_y < 0) {
                partial = gdk_pixbuf_new_subpixbuf
                        (marker, 0, dest_y + marker_height,
                         marker_width, -dest_y);

                dest_y = 0.0;
                marker_height = gdk_pixbuf_get_height (partial);
        } else if (dest_y + dest_height > height) {
                partial = gdk_pixbuf_new_subpixbuf
                        (marker, 0, 0, marker_width, height - dest_y);
                marker_height = gdk_pixbuf_get_height (partial);
        }

        if (partial) {
                marker = partial;
        }

        /* handle the cases where the marker needs to be placed across
           the 180 degree longitude line */
        if (dest_x < 0) {
                /* split into our two pixbufs for the left and right edges */
                GdkPixbuf *lhs = NULL;
                GdkPixbuf *rhs = NULL;

                lhs = gdk_pixbuf_new_subpixbuf
                        (marker, -dest_x, 0, marker_width + dest_x, marker_height);

                gdk_pixbuf_composite (lhs, priv->location_map_pixbuf,
                                      0, dest_y,
                                      gdk_pixbuf_get_width (lhs),
                                      gdk_pixbuf_get_height (lhs),
                                      0, dest_y,
                                      1.0, 1.0, GDK_INTERP_NEAREST, 0xFF);

                rhs = gdk_pixbuf_new_subpixbuf
                        (marker, 0, 0, -dest_x, marker_height);

                gdk_pixbuf_composite (rhs, priv->location_map_pixbuf,
                                      width - gdk_pixbuf_get_width (rhs) - 1,
                                      dest_y,
                                      gdk_pixbuf_get_width (rhs),
                                      gdk_pixbuf_get_height (rhs),
                                      width - gdk_pixbuf_get_width (rhs) - 1,
                                      dest_y,
                                      1.0, 1.0, GDK_INTERP_NEAREST, 0xFF);

                gdk_pixbuf_unref (lhs);
                gdk_pixbuf_unref (rhs);
        } else if (dest_x + dest_width > width) {
                /* split into our two pixbufs for the left and right edges */
                GdkPixbuf *lhs = NULL;
                GdkPixbuf *rhs = NULL;

                lhs = gdk_pixbuf_new_subpixbuf
                        (marker, width - dest_x, 0, marker_width - width + dest_x, marker_height);

                gdk_pixbuf_composite (lhs, priv->location_map_pixbuf,
                                      0, dest_y,
                                      gdk_pixbuf_get_width (lhs),
                                      gdk_pixbuf_get_height (lhs),
                                      0, dest_y,
                                      1.0, 1.0, GDK_INTERP_NEAREST, 0xFF);

                rhs = gdk_pixbuf_new_subpixbuf
                        (marker, 0, 0, width - dest_x, marker_height);

                gdk_pixbuf_composite (rhs, priv->location_map_pixbuf,
                                      width - gdk_pixbuf_get_width (rhs) - 1,
                                      dest_y,
                                      gdk_pixbuf_get_width (rhs),
                                      gdk_pixbuf_get_height (rhs),
                                      width - gdk_pixbuf_get_width (rhs) - 1,
                                      dest_y,
                                      1.0, 1.0, GDK_INTERP_NEAREST, 0xFF);

                gdk_pixbuf_unref (lhs);
                gdk_pixbuf_unref (rhs);
        } else {
                gdk_pixbuf_composite (marker, priv->location_map_pixbuf,
                                      dest_x, dest_y,
                                      gdk_pixbuf_get_width (marker),
                                      gdk_pixbuf_get_height (marker),
                                      dest_x, dest_y,
                                      1.0, 1.0, GDK_INTERP_NEAREST, 0xFF);
        }

        if (partial != NULL) {
                gdk_pixbuf_unref (partial);
        }
}

static void
clock_map_place_location (ClockMap *this, ClockLocation *loc, gboolean hilight)
{
        gfloat latitude, longitude;
	gint marker;

        clock_location_get_coords (loc, &latitude, &longitude);

	if (hilight)
		marker = 1;
	else if (clock_location_is_current (loc))
		marker = 2;
	else
		marker = 0;

        clock_map_mark (this, latitude, longitude, marker);
}

static void
clock_map_place_locations (ClockMap *this)
{
        ClockMapPrivate *priv = PRIVATE (this);
        GList *locs;
        ClockLocation *loc;

        if (priv->location_map_pixbuf) {
                gdk_pixbuf_unref (priv->location_map_pixbuf);
                priv->location_map_pixbuf = NULL;
        }

        priv->location_map_pixbuf = gdk_pixbuf_copy (priv->stock_map_pixbuf);

	locs = NULL;
	g_signal_emit (this, signals[NEED_LOCATIONS], 0, &locs);

        while (locs) {
                loc = CLOCK_LOCATION (locs->data);

                clock_map_place_location (this, loc, FALSE);

                locs = locs->next;
        }

#if 0
        /* map_mark test suite for the edge cases */

        /* points around longitude 180 */
        clock_map_mark (this, 0.0, 180.0);
        clock_map_mark (this, -15.0, -178.0);
        clock_map_mark (this, -30.0, -176.0);
        clock_map_mark (this, 15.0, 178.0);
        clock_map_mark (this, 30.0, 176.0);

        clock_map_mark (this, 90.0, 180.0);
        clock_map_mark (this, -90.0, 180.0);

        /* north pole & friends */
        clock_map_mark (this, 90.0, 0.0);
        clock_map_mark (this, 88.0, -15.0);
        clock_map_mark (this, 92.0, 15.0);

        /* south pole & friends */
        clock_map_mark (this, -90.0, 0.0);
        clock_map_mark (this, -88.0, -15.0);
        clock_map_mark (this, -92.0, 15.0);
#endif
}

static void
clock_map_compute_vector (gdouble lat, gdouble lon, gdouble *vec)
{
        gdouble lat_rad, lon_rad;
        lat_rad = lat * (M_PI/180.0);
        lon_rad = lon * (M_PI/180.0);

        vec[0] = sin(lon_rad) * cos(lat_rad);
        vec[1] = sin(lat_rad);
        vec[2] = cos(lon_rad) * cos(lat_rad);
}

static guchar
clock_map_is_sunlit (gdouble pos_lat, gdouble pos_long,
                         gdouble sun_lat, gdouble sun_long)
{
        gdouble pos_vec[3];
        gdouble sun_vec[3];
        gdouble dot;

        /* twilight */
        gdouble epsilon = 0.01;

        clock_map_compute_vector (pos_lat, pos_long, pos_vec);
        clock_map_compute_vector (sun_lat, sun_long, sun_vec);

        /* compute the dot product of the two */
        dot = pos_vec[0]*sun_vec[0] + pos_vec[1]*sun_vec[1]
                + pos_vec[2]*sun_vec[2];

        if (dot > epsilon) {
                return 0x00;
        }

        if (dot < -epsilon) {
                return 0xFF;
        }

        return (guchar)(-128 * ((dot / epsilon) - 1));
}

static void
clock_map_render_shadow_pixbuf (GdkPixbuf *pixbuf)
{
        int x, y;
        int height, width;
        int n_channels, rowstride;
        guchar *pixels, *p;
        gdouble sun_lat, sun_lon;
        time_t now = time (NULL);

        n_channels = gdk_pixbuf_get_n_channels (pixbuf);
        rowstride = gdk_pixbuf_get_rowstride (pixbuf);
        pixels = gdk_pixbuf_get_pixels (pixbuf);

        width = gdk_pixbuf_get_width (pixbuf);
        height = gdk_pixbuf_get_height (pixbuf);

        sun_position (now, &sun_lat, &sun_lon);

        for (y = 0; y < height; y++) {
                gdouble lat = (height / 2.0 - y) / (height / 2.0) * 90.0;

                for (x = 0; x < width; x++) {
                        guchar shade;
                        gdouble lon =
                                (x - width / 2.0) / (width / 2.0) * 180.0;

                        shade = clock_map_is_sunlit (lat, lon,
                                                         sun_lat, sun_lon);

                        p = pixels + y * rowstride + x * n_channels;
                        p[3] = shade;
                }
        }
}

static void
clock_map_render_shadow (ClockMap *this)
{
        ClockMapPrivate *priv = PRIVATE (this);

        if (priv->shadow_pixbuf) {
                gdk_pixbuf_unref (priv->shadow_pixbuf);
        }

        priv->shadow_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
                                              priv->width, priv->height);

        /* Initialize to all shadow */
        gdk_pixbuf_fill (priv->shadow_pixbuf, 0x6d9ccdff);

        clock_map_render_shadow_pixbuf (priv->shadow_pixbuf);

        if (priv->shadow_map_pixbuf) {
                gdk_pixbuf_unref (priv->shadow_map_pixbuf);
        }

        priv->shadow_map_pixbuf = gdk_pixbuf_copy (priv->location_map_pixbuf);

        gdk_pixbuf_composite (priv->shadow_pixbuf, priv->shadow_map_pixbuf,
                              0, 0, priv->width, priv->height,
                              0, 0, 1, 1, GDK_INTERP_NEAREST, 0x66);
}

static void
clock_map_rotate (ClockMap *this)
{
        ClockMapPrivate *priv = PRIVATE (this);

        int rot_center, map_center;
        gfloat latitude, longitude;

        map_center = priv->width / 2;

#ifdef TESTER
	priv->rotated_map_pixbuf = gdk_pixbuf_copy (priv->shadow_map_pixbuf);

#else

        if (priv->rotated_map_pixbuf) {
                gdk_pixbuf_unref (priv->rotated_map_pixbuf);
        }

        priv->rotated_map_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
                                                   priv->width, priv->height);

        latitude = 0.0;
        longitude = 0.0;

        rot_center = (priv->width / 2.0 + (priv->width / 2.0) * longitude / 180.0);

        if (rot_center < map_center) {
                /* copy the part after the image center */
                gdk_pixbuf_copy_area (priv->shadow_map_pixbuf,
                                      rot_center, 0,
                                      map_center, priv->height,
                                      priv->rotated_map_pixbuf, map_center, 0);

                /* copy the part before the image center */
                gdk_pixbuf_copy_area (priv->shadow_map_pixbuf,
                                      rot_center + map_center, 0,
                                      priv->width - (rot_center + map_center),
                                      priv->height,
                                      priv->rotated_map_pixbuf, 0, 0);

                gdk_pixbuf_copy_area (priv->shadow_map_pixbuf,
                                      0, 0, rot_center, priv->height,
                                      priv->rotated_map_pixbuf,
                                      map_center - rot_center, 0);
        } else {
                /* copy the part after the image center */
                gdk_pixbuf_copy_area (priv->shadow_map_pixbuf,
                                      rot_center - map_center, 0,
                                      map_center, priv->height,
                                      priv->rotated_map_pixbuf, 0, 0);

                /* copy the part before the image center */
                gdk_pixbuf_copy_area (priv->shadow_map_pixbuf,
                                      rot_center, 0, priv->width - rot_center,
                                      priv->height,
                                      priv->rotated_map_pixbuf, map_center, 0);

                gdk_pixbuf_copy_area (priv->shadow_map_pixbuf,
                                      0, 0, rot_center - map_center,
                                      priv->height,
                                      priv->rotated_map_pixbuf,
                                      map_center + priv->width -  rot_center, 0);
        }
#endif
}

static void
clock_map_display (ClockMap *this)
{
	gtk_widget_queue_draw (GTK_WIDGET (this));
}

typedef struct {
       ClockMap *map;
       ClockLocation *location;
       int count;
} BlinkData;

static gboolean
highlight (gpointer user_data)
{
       BlinkData *data = user_data;

       if (data->count == 6) {
	       g_object_unref (data->location);
               g_free (data);
               return FALSE;
       }

       if (data->count % 2 == 0)
                 clock_map_place_location (data->map, data->location, TRUE);
       else
                 clock_map_place_locations (data->map);
       clock_map_render_shadow (data->map);
       clock_map_rotate (data->map);
       clock_map_display (data->map);

       data->count++;

       return TRUE;
}

void
clock_map_blink_location (ClockMap *this, ClockLocation *loc)
{
       BlinkData *data;

       g_return_if_fail (IS_CLOCK_MAP (this));
       g_return_if_fail (IS_CLOCK_LOCATION (loc));

       data = g_new0 (BlinkData, 1);
       data->map = this;
       data->location = g_object_ref (loc);

       highlight (data);

       g_timeout_add (300, highlight, data);
}

static gboolean
clock_map_needs_refresh (ClockMap *this)
{
        ClockMapPrivate *priv = PRIVATE (this);
        gboolean refresh = FALSE;

        struct tm now;
        time_t now_t;
        time (&now_t);

        gmtime_r (&now_t, &now);

        /* refresh once per minute */
        if (now.tm_year > priv->last_refresh.tm_year
            || now.tm_mon > priv->last_refresh.tm_mon
            || now.tm_mday > priv->last_refresh.tm_mday
            || now.tm_hour > priv->last_refresh.tm_hour
            || now.tm_min > priv->last_refresh.tm_min) {
                refresh = TRUE;
        }

        gmtime_r (&now_t, &priv->last_refresh);

        return refresh;
}

void
clock_map_update_time (ClockMap *this)
{
        ClockMapPrivate *priv;

	g_return_if_fail (IS_CLOCK_MAP (this));

	priv = PRIVATE (this);

        if (!clock_map_needs_refresh (this)) {
                return;
        }

        clock_map_render_shadow (this);
        clock_map_rotate (this);
        clock_map_display (this);
}