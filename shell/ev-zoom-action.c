/* ev-zoom-action.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2012 Carlos Garcia Campos  <carlosgc@gnome.org>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ev-zoom-action.h"

#include <glib/gi18n.h>

enum {
        ACTIVATED,
        LAST_SIGNAL
};

enum
{
        PROP_0,

        PROP_DOCUMENT_MODEL,
        PROP_MENU
};

enum {
        ZOOM_MODES_SECTION,
        ZOOM_FREE_SECTION
};

static const struct {
        const gchar *name;
        float        level;
} zoom_levels[] = {
        { N_("50%"), 0.5 },
        { N_("70%"), 0.7071067811 },
        { N_("85%"), 0.8408964152 },
        { N_("100%"), 1.0 },
        { N_("125%"), 1.1892071149 },
        { N_("150%"), 1.4142135623 },
        { N_("175%"), 1.6817928304 },
        { N_("200%"), 2.0 },
        { N_("300%"), 2.8284271247 },
        { N_("400%"), 4.0 },
        { N_("800%"), 8.0 },
        { N_("1600%"), 16.0 },
        { N_("3200%"), 32.0 },
        { N_("6400%"), 64.0 }
};

struct _EvZoomActionPrivate {
        GtkWidget       *entry;

        EvDocumentModel *model;
        GMenu           *menu;

        GMenuModel      *zoom_free_section;
        GtkWidget       *popup;
        gboolean         popup_shown;
};

G_DEFINE_TYPE (EvZoomAction, ev_zoom_action, GTK_TYPE_BOX)

static guint signals[LAST_SIGNAL] = { 0 };

#define EPSILON 0.000001

static void
ev_zoom_action_set_zoom_level (EvZoomAction *zoom_action,
                               float         zoom)
{
        gchar *zoom_str;
        float  zoom_perc;
        guint  i;

        for (i = 3; i < G_N_ELEMENTS (zoom_levels); i++) {
                if (ABS (zoom - zoom_levels[i].level) < EPSILON) {
                        gtk_entry_set_text (GTK_ENTRY (zoom_action->priv->entry),
                                            zoom_levels[i].name);
                        return;
                }
        }

        zoom_perc = zoom * 100.;
        if (ABS ((gint)zoom_perc - zoom_perc) < 0.001)
                zoom_str = g_strdup_printf ("%d%%", (gint)zoom_perc);
        else
                zoom_str = g_strdup_printf ("%.2f%%", zoom_perc);
        gtk_entry_set_text (GTK_ENTRY (zoom_action->priv->entry), zoom_str);
        g_free (zoom_str);
}

static void
ev_zoom_action_update_zoom_level (EvZoomAction *zoom_action)
{
        float      zoom = ev_document_model_get_scale (zoom_action->priv->model);
        GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (zoom_action));

        zoom *= 72.0 / ev_document_misc_get_screen_dpi (screen);
        ev_zoom_action_set_zoom_level (zoom_action, zoom);
}

static void
zoom_changed_cb (EvDocumentModel *model,
                 GParamSpec      *pspec,
                 EvZoomAction    *zoom_action)
{
        ev_zoom_action_update_zoom_level (zoom_action);
}

static void
document_changed_cb (EvDocumentModel *model,
                     GParamSpec      *pspec,
                     EvZoomAction    *zoom_action)
{
        EvDocument *document = ev_document_model_get_document (model);

        if (!document) {
                gtk_widget_set_sensitive (GTK_WIDGET (zoom_action), FALSE);
                return;
        }
        gtk_widget_set_sensitive (GTK_WIDGET (zoom_action), ev_document_get_n_pages (document) > 0);

        ev_zoom_action_update_zoom_level (zoom_action);
}

static void
ev_zoom_action_populate_free_zoom_section (EvZoomAction *zoom_action)
{
        gdouble max_scale;
        guint   i;

        max_scale = ev_document_model_get_max_scale (zoom_action->priv->model);

        for (i = 0; i < G_N_ELEMENTS (zoom_levels); i++) {
                GMenuItem *item;

                if (zoom_levels[i].level > max_scale)
                        break;

                item = g_menu_item_new (zoom_levels[i].name, NULL);
                g_menu_item_set_action_and_target (item, "win.zoom",
                                                   "d", zoom_levels[i].level);
                g_menu_append_item (G_MENU (zoom_action->priv->zoom_free_section), item);
                g_object_unref (item);
        }
}

static void
max_zoom_changed_cb (EvDocumentModel *model,
                     GParamSpec      *pspec,
                     EvZoomAction    *zoom_action)
{
        g_menu_remove_all (G_MENU (zoom_action->priv->zoom_free_section));
        g_clear_pointer (&zoom_action->priv->popup, (GDestroyNotify)gtk_widget_destroy);
        ev_zoom_action_populate_free_zoom_section (zoom_action);
}

static void
entry_activated_cb (GtkEntry     *entry,
                    EvZoomAction *zoom_action)
{
        GdkScreen   *screen;
        double       zoom_perc;
        float        zoom;
        const gchar *text = gtk_entry_get_text (entry);
        gchar       *end_ptr = NULL;

        if (!text || text[0] == '\0') {
                ev_zoom_action_update_zoom_level (zoom_action);
                g_signal_emit (zoom_action, signals[ACTIVATED], 0, NULL);
                return;
        }

        zoom_perc = g_strtod (text, &end_ptr);
        if (end_ptr && end_ptr[0] != '\0' && end_ptr[0] != '%') {
                ev_zoom_action_update_zoom_level (zoom_action);
                g_signal_emit (zoom_action, signals[ACTIVATED], 0, NULL);
                return;
        }

        screen = gtk_widget_get_screen (GTK_WIDGET (zoom_action));
        zoom = zoom_perc / 100.;
        ev_document_model_set_sizing_mode (zoom_action->priv->model, EV_SIZING_FREE);
        ev_document_model_set_scale (zoom_action->priv->model,
                                     zoom * ev_document_misc_get_screen_dpi (screen) / 72.0);
        g_signal_emit (zoom_action, signals[ACTIVATED], 0, NULL);
}

static gboolean
focus_out_cb (EvZoomAction *zoom_action)
{
        ev_zoom_action_update_zoom_level (zoom_action);

        return FALSE;
}

static void
popup_menu_show_cb (GtkWidget    *widget,
                    EvZoomAction *zoom_action)
{
        zoom_action->priv->popup_shown = TRUE;
}

static void
popup_menu_hide_cb (GtkWidget    *widget,
                    EvZoomAction *zoom_action)
{
        zoom_action->priv->popup_shown = FALSE;
}

static void
popup_menu_detached (EvZoomAction *zoom_action,
                     GtkWidget    *popup)
{
        GtkWidget *toplevel;

        if (zoom_action->priv->popup != popup)
                return;

        toplevel = gtk_widget_get_toplevel (zoom_action->priv->popup);
        g_signal_handlers_disconnect_by_func (toplevel,
                                              popup_menu_show_cb,
                                              zoom_action);
        g_signal_handlers_disconnect_by_func (toplevel,
                                              popup_menu_hide_cb,
                                              zoom_action);

        zoom_action->priv->popup = NULL;
}

static GtkWidget *
get_popup (EvZoomAction *zoom_action)
{
        GtkWidget *toplevel;

        if (zoom_action->priv->popup)
                return zoom_action->priv->popup;

        zoom_action->priv->popup = gtk_menu_new_from_model (G_MENU_MODEL (zoom_action->priv->menu));
        gtk_menu_attach_to_widget (GTK_MENU (zoom_action->priv->popup),
                                   GTK_WIDGET (zoom_action),
                                   (GtkMenuDetachFunc)popup_menu_detached);
        toplevel = gtk_widget_get_toplevel (zoom_action->priv->popup);
        g_signal_connect (toplevel, "show",
                          G_CALLBACK (popup_menu_show_cb),
                          zoom_action);
        g_signal_connect (toplevel, "hide",
                          G_CALLBACK (popup_menu_hide_cb),
                          zoom_action);

        return zoom_action->priv->popup;
}


static void
menu_position_below (GtkMenu  *menu,
                     gint     *x,
                     gint     *y,
                     gint     *push_in,
                     gpointer  user_data)
{
        EvZoomAction  *zoom_action;
        GtkWidget     *widget;
        GtkAllocation  child_allocation;
        GtkRequisition req;
        GdkScreen     *screen;
        gint           monitor_num;
        GdkRectangle   monitor;
        gint           sx = 0, sy = 0;

        zoom_action = EV_ZOOM_ACTION (user_data);
        widget = GTK_WIDGET (zoom_action);

        gtk_widget_get_allocation (zoom_action->priv->entry, &child_allocation);

        if (!gtk_widget_get_has_window (zoom_action->priv->entry)) {
                sx += child_allocation.x;
                sy += child_allocation.y;
        }

        gdk_window_get_root_coords (gtk_widget_get_window (zoom_action->priv->entry),
                                    sx, sy, &sx, &sy);

        gtk_widget_get_preferred_size (GTK_WIDGET (menu), &req, NULL);

        if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
                *x = sx;
        else
                *x = sx + child_allocation.width - req.width;
        *y = sy;

        screen = gtk_widget_get_screen (widget);
        monitor_num = gdk_screen_get_monitor_at_window (screen,
                                                        gtk_widget_get_window (widget));
        gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);

        if (*x < monitor.x)
                *x = monitor.x;
        else if (*x + req.width > monitor.x + monitor.width)
                *x = monitor.x + monitor.width - req.width;

        if (monitor.y + monitor.height - *y - child_allocation.height >= req.height)
                *y += child_allocation.height;
        else if (*y - monitor.y >= req.height)
                *y -= req.height;
        else if (monitor.y + monitor.height - *y - child_allocation.height > *y - monitor.y)
                *y += child_allocation.height;
        else
                *y -= req.height;

        *push_in = FALSE;
}

static void
entry_icon_press_callback (GtkEntry            *entry,
                           GtkEntryIconPosition icon_pos,
                           GdkEventButton      *event,
                           EvZoomAction        *zoom_action)
{
        GtkWidget *menu;

        if (event->button != GDK_BUTTON_PRIMARY)
                return;

        menu = get_popup (zoom_action);
        gtk_widget_set_size_request (menu,
                                     gtk_widget_get_allocated_width (GTK_WIDGET (zoom_action)),
                                     -1);

        gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                        menu_position_below, zoom_action,
                        event->button, event->time);
}

static void
ev_zoom_action_finalize (GObject *object)
{
        EvZoomAction *zoom_action = EV_ZOOM_ACTION (object);

        if (zoom_action->priv->model) {
                g_object_remove_weak_pointer (G_OBJECT (zoom_action->priv->model),
                                              (gpointer)&zoom_action->priv->model);
        }

        g_clear_object (&zoom_action->priv->zoom_free_section);

        G_OBJECT_CLASS (ev_zoom_action_parent_class)->finalize (object);
}

static void
ev_zoom_action_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
        EvZoomAction *zoom_action = EV_ZOOM_ACTION (object);

        switch (prop_id) {
        case PROP_DOCUMENT_MODEL:
                zoom_action->priv->model = g_value_get_object (value);
                break;
        case PROP_MENU:
                zoom_action->priv->menu = g_value_get_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
setup_initial_entry_size (EvZoomAction *zoom_action)
{
        GMenuModel *menu;
        guint       i;
        gint        n_items;
        gint        width = 0;

        menu = g_menu_model_get_item_link (G_MENU_MODEL (zoom_action->priv->menu),
                                           ZOOM_MODES_SECTION, G_MENU_LINK_SECTION);

        n_items = g_menu_model_get_n_items (menu);
        for (i = 0; i < n_items; i++) {
                GVariant *value;
                gint      length;

                value = g_menu_model_get_item_attribute_value (menu, i, G_MENU_ATTRIBUTE_LABEL, NULL);
                length = g_utf8_strlen (g_variant_get_string (value, NULL), -1);
                if (length > width)
                        width = length;
                g_variant_unref (value);
        }
        g_object_unref (menu);

        /* Count the toggle size as one character more */
        gtk_entry_set_width_chars (GTK_ENTRY (zoom_action->priv->entry), width + 1);
}

static void
ev_zoom_action_constructed (GObject *object)
{
        EvZoomAction *zoom_action = EV_ZOOM_ACTION (object);

        G_OBJECT_CLASS (ev_zoom_action_parent_class)->constructed (object);

        zoom_action->priv->zoom_free_section =
                g_menu_model_get_item_link (G_MENU_MODEL (zoom_action->priv->menu),
                                            ZOOM_FREE_SECTION, G_MENU_LINK_SECTION);
        ev_zoom_action_populate_free_zoom_section (zoom_action);

        g_object_add_weak_pointer (G_OBJECT (zoom_action->priv->model),
                                   (gpointer)&zoom_action->priv->model);
        if (ev_document_model_get_document (zoom_action->priv->model)) {
                ev_zoom_action_update_zoom_level (zoom_action);
        } else {
                ev_zoom_action_set_zoom_level (zoom_action, 1.);
                gtk_widget_set_sensitive (GTK_WIDGET (zoom_action), FALSE);
        }

        g_signal_connect (zoom_action->priv->model, "notify::document",
                          G_CALLBACK (document_changed_cb),
                          zoom_action);
        g_signal_connect (zoom_action->priv->model, "notify::scale",
                          G_CALLBACK (zoom_changed_cb),
                          zoom_action);
        g_signal_connect (zoom_action->priv->model, "notify::max-scale",
                          G_CALLBACK (max_zoom_changed_cb),
                          zoom_action);

        setup_initial_entry_size (zoom_action);
}

static void
ev_zoom_action_class_init (EvZoomActionClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);

        object_class->finalize = ev_zoom_action_finalize;
        object_class->constructed = ev_zoom_action_constructed;
        object_class->set_property = ev_zoom_action_set_property;

        g_object_class_install_property (object_class,
                                         PROP_DOCUMENT_MODEL,
                                         g_param_spec_object ("document-model",
                                                              "DocumentModel",
                                                              "The document model",
                                                              EV_TYPE_DOCUMENT_MODEL,
                                                              G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));

        g_object_class_install_property (object_class,
                                         PROP_MENU,
                                         g_param_spec_object ("menu",
                                                              "Menu",
                                                              "The zoom popup menu",
                                                              G_TYPE_MENU,
                                                              G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));

        signals[ACTIVATED] =
                g_signal_new ("activated",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        g_type_class_add_private (object_class, sizeof (EvZoomActionPrivate));
}

static void
ev_zoom_action_init (EvZoomAction *zoom_action)
{
        EvZoomActionPrivate *priv;

        zoom_action->priv = G_TYPE_INSTANCE_GET_PRIVATE (zoom_action, EV_TYPE_ZOOM_ACTION, EvZoomActionPrivate);
        priv = zoom_action->priv;

        gtk_orientable_set_orientation (GTK_ORIENTABLE (zoom_action), GTK_ORIENTATION_VERTICAL);

        priv->entry = gtk_entry_new ();
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->entry),
                                           GTK_ENTRY_ICON_SECONDARY,
                                           "go-down-symbolic");
        gtk_box_pack_start (GTK_BOX (zoom_action), priv->entry, TRUE, FALSE, 0);
        gtk_widget_show (priv->entry);

        g_signal_connect (priv->entry, "icon-press",
                          G_CALLBACK (entry_icon_press_callback),
                          zoom_action);
        g_signal_connect (priv->entry, "activate",
                          G_CALLBACK (entry_activated_cb),
                          zoom_action);
        g_signal_connect_swapped (priv->entry, "focus-out-event",
                                  G_CALLBACK (focus_out_cb),
                                  zoom_action);
}

GtkWidget *
ev_zoom_action_new (EvDocumentModel *model,
                    GMenu           *menu)
{
        g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), NULL);
        g_return_val_if_fail (G_IS_MENU (menu), NULL);

        return GTK_WIDGET (g_object_new (EV_TYPE_ZOOM_ACTION,
                                         "document-model", model,
                                         "menu", menu,
                                         NULL));
}

gboolean
ev_zoom_action_get_popup_shown (EvZoomAction *action)
{
        g_return_val_if_fail (EV_IS_ZOOM_ACTION (action), FALSE);

        return action->priv->popup_shown;
}
