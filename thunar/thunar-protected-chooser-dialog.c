/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-abstract-dialog.h>
#include <thunar/thunar-application.h>
#include <thunar/thunar-protected-chooser-dialog.h>
#include <thunar/thunar-protected-chooser-model.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-private.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_INFO,
};



static void        thunar_protected_chooser_dialog_dispose             (GObject                  *object);
static void        thunar_protected_chooser_dialog_get_property        (GObject                  *object,
                                                                        guint                     prop_id,
                                                                        GValue                   *value,
                                                                        GParamSpec               *pspec);
static void        thunar_protected_chooser_dialog_set_property        (GObject                  *object,
                                                                        guint                     prop_id,
                                                                        const GValue             *value,
                                                                        GParamSpec               *pspec);
static void        thunar_protected_chooser_dialog_realize             (GtkWidget                *widget);
static void        thunar_protected_chooser_dialog_response            (GtkDialog                *widget,
                                                                        gint                      response);
static gboolean    thunar_protected_chooser_dialog_selection_func      (GtkTreeSelection         *selection,
                                                                        GtkTreeModel             *model,
                                                                        GtkTreePath              *path,
                                                                        gboolean                  path_currently_selected,
                                                                        gpointer                  user_data);
static gboolean    thunar_protected_chooser_dialog_context_menu        (ThunarProtectedChooserDialog      *dialog,
                                                                        guint                               button,
                                                                        guint32                             time);
static void        thunar_protected_chooser_dialog_update_accept       (ThunarProtectedChooserDialog      *dialog);
static void        thunar_protected_chooser_dialog_update_header       (ThunarProtectedChooserDialog      *dialog);
static void        thunar_protected_chooser_dialog_browse_clicked      (GtkWidget                          *button,
                                                                        ThunarProtectedChooserDialog      *dialog);
static gboolean    thunar_protected_chooser_dialog_button_press_event  (GtkWidget                          *tree_view,
                                                                        GdkEventButton                     *event,
                                                                        ThunarProtectedChooserDialog      *dialog);
static void        thunar_protected_chooser_dialog_notify_expanded     (GtkExpander                        *expander,
                                                                        GParamSpec                         *pspec,
                                                                        ThunarProtectedChooserDialog      *dialog);
static void        thunar_protected_chooser_dialog_expand              (ThunarProtectedChooserDialog      *dialog);
static gboolean    thunar_protected_chooser_dialog_popup_menu          (GtkWidget                          *tree_view,
                                                                        ThunarProtectedChooserDialog      *dialog);
static void        thunar_protected_chooser_dialog_row_activated       (GtkTreeView                        *treeview,
                                                                        GtkTreePath                        *path,
                                                                        GtkTreeViewColumn                  *column,
                                                                        ThunarProtectedChooserDialog      *dialog);
static void        thunar_protected_chooser_dialog_selection_changed   (GtkTreeSelection                  *selection,
                                                                        ThunarProtectedChooserDialog      *dialog);
static void        thunar_protected_chooser_dialog_set_current_info    (ThunarProtectedChooserDialog *dialog,
                                                                        GAppInfo                     *info);


struct _ThunarProtectedChooserDialogClass
{
  ThunarAbstractDialogClass __parent__;
};

struct _ThunarProtectedChooserDialog
{
  ThunarAbstractDialog __parent__;

  GAppInfo    *current_info;

  GtkWidget   *header_image;
  GtkWidget   *header_label;
  GtkWidget   *tree_view;
  GtkWidget   *custom_expander;
  GtkWidget   *custom_entry;
  GtkWidget   *custom_button;
  GtkWidget   *cancel_button;
  GtkWidget   *accept_button;
};



G_DEFINE_TYPE (ThunarProtectedChooserDialog, thunar_protected_chooser_dialog, THUNAR_TYPE_ABSTRACT_DIALOG)



static void
thunar_protected_chooser_dialog_class_init (ThunarProtectedChooserDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_protected_chooser_dialog_dispose;
  gobject_class->get_property = thunar_protected_chooser_dialog_get_property;
  gobject_class->set_property = thunar_protected_chooser_dialog_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_protected_chooser_dialog_realize;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_protected_chooser_dialog_response;

  /**
   * ThunarProtectedChooserDialog::current_info:
   *
   * A placeholder for the #GAppInfo currently chosen.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_INFO,
                                   g_param_spec_object ("current-info", "current-info", "current-info",
                                                        G_TYPE_APP_INFO,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_protected_chooser_dialog_init (ThunarProtectedChooserDialog *dialog)
{
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;
  GtkWidget         *header;
  GtkWidget         *hbox;
  GtkWidget         *vbox;
  GtkWidget         *box;
  GtkWidget         *swin;

  dialog->current_info = NULL;

  /* setup basic window properties */
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Select Protected File Handlers"));

  /* create the main widget box */
  vbox = g_object_new (GTK_TYPE_VBOX, "border-width", 6, "spacing", 12, NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  /* create the header box */
  header = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, FALSE, 0);
  gtk_widget_show (header);

  /* create the header image */
  dialog->header_image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (header), dialog->header_image, FALSE, FALSE, 0);
  gtk_widget_show (dialog->header_image);

  /* create the header label */
  dialog->header_label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (dialog->header_label), 0.0f, 0.5f);
  gtk_label_set_line_wrap (GTK_LABEL (dialog->header_label), TRUE);
  gtk_widget_set_size_request (dialog->header_label, 350, -1);
  gtk_box_pack_start (GTK_BOX (header), dialog->header_label, FALSE, FALSE, 0);
  gtk_widget_show (dialog->header_label);

  /* create the view box */
  box = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);
  gtk_widget_show (box);

  /* create the scrolled window for the tree view */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (swin, -1, 270);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (box), swin, TRUE, TRUE, 0);
  gtk_widget_show (swin);

  /* create the tree view */
  dialog->tree_view = g_object_new (GTK_TYPE_TREE_VIEW, "headers-visible", FALSE, NULL);
  g_signal_connect (G_OBJECT (dialog->tree_view), "button-press-event", G_CALLBACK (thunar_protected_chooser_dialog_button_press_event), dialog);
  g_signal_connect (G_OBJECT (dialog->tree_view), "popup-menu", G_CALLBACK (thunar_protected_chooser_dialog_popup_menu), dialog);
  g_signal_connect (G_OBJECT (dialog->tree_view), "row-activated", G_CALLBACK (thunar_protected_chooser_dialog_row_activated), dialog);
  gtk_container_add (GTK_CONTAINER (swin), dialog->tree_view);
  gtk_widget_show (dialog->tree_view);

  /* append the tree view column */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN, "expand", TRUE, NULL);
  renderer = g_object_new (EXO_TYPE_CELL_RENDERER_ICON, "follow-state", FALSE, "size", 24, NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "gicon", THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_ICON,
                                       NULL);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "style", THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_STYLE,
                                       "text", THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_NAME,
                                       "weight", THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_WEIGHT,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->tree_view), column);

  /* don't show the expanders */
  g_object_set (G_OBJECT (dialog->tree_view),
                "level-indentation", 24,
                "show-expanders", FALSE,
                NULL);

  /* create the "Custom command" expand */
  dialog->custom_expander = gtk_expander_new_with_mnemonic (_("Use a _custom command:"));
  gtk_widget_set_tooltip_text (dialog->custom_expander, _("Use a custom command for an application that is not "
                                                          "available from the above application list."));
  exo_binding_new_with_negation (G_OBJECT (dialog->custom_expander), "expanded", G_OBJECT (dialog->tree_view), "sensitive");
  g_signal_connect (G_OBJECT (dialog->custom_expander), "notify::expanded", G_CALLBACK (thunar_protected_chooser_dialog_notify_expanded), dialog);
  gtk_box_pack_start (GTK_BOX (box), dialog->custom_expander, FALSE, FALSE, 0);
  gtk_widget_show (dialog->custom_expander);

  /* create the "Custom command" box */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (dialog->custom_expander), hbox);
  gtk_widget_show (hbox);

  /* create the "Custom command" entry */
  dialog->custom_entry = g_object_new (GTK_TYPE_ENTRY, "activates-default", TRUE, NULL);
  g_signal_connect_swapped (G_OBJECT (dialog->custom_entry), "changed", G_CALLBACK (thunar_protected_chooser_dialog_update_accept), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->custom_entry, TRUE, TRUE, 0);
  gtk_widget_show (dialog->custom_entry);

  /* create the "Custom command" button */
  dialog->custom_button = gtk_button_new_with_mnemonic (_("_Browse..."));
  g_signal_connect (G_OBJECT (dialog->custom_button), "clicked", G_CALLBACK (thunar_protected_chooser_dialog_browse_clicked), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->custom_button, FALSE, FALSE, 0);
  gtk_widget_show (dialog->custom_button);

  /* add the "Cancel" button */
  dialog->cancel_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

  /* add the "Ok" button */
  dialog->accept_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, FALSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  /* update the "Ok" button and the custom entry whenever the tree selection changes */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
  gtk_tree_selection_set_select_function (selection, thunar_protected_chooser_dialog_selection_func, dialog, NULL);
  g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (thunar_protected_chooser_dialog_selection_changed), dialog);
}



static void
thunar_protected_chooser_dialog_dispose (GObject *object)
{
  ThunarProtectedChooserDialog *dialog = THUNAR_PROTECTED_CHOOSER_DIALOG (object);

  /* drop the reference on the current info */
  if (G_LIKELY (dialog->current_info != NULL))
    g_object_unref (dialog->current_info);

  (*G_OBJECT_CLASS (thunar_protected_chooser_dialog_parent_class)->dispose) (object);
}



static void
thunar_protected_chooser_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ThunarProtectedChooserDialog *dialog = THUNAR_PROTECTED_CHOOSER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CURRENT_INFO:
      g_value_set_object (value, thunar_protected_chooser_dialog_get_current_info (dialog));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_protected_chooser_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ThunarProtectedChooserDialog *dialog = THUNAR_PROTECTED_CHOOSER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CURRENT_INFO:
      thunar_protected_chooser_dialog_set_current_info (dialog, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



GAppInfo*
thunar_protected_chooser_dialog_get_current_info (ThunarProtectedChooserDialog *dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_PROTECTED_CHOOSER_DIALOG (dialog), NULL);

  if (dialog->current_info)
      return g_object_ref (dialog->current_info);
  else
      return NULL;
}



static void
thunar_protected_chooser_dialog_set_current_info (ThunarProtectedChooserDialog *dialog, GAppInfo *app_info)
{
  GAppInfo *old_info = dialog->current_info;
  dialog->current_info = app_info ? g_object_ref (app_info) : NULL;
  if (old_info)
    {
      g_object_unref (old_info);
    }
}



static void
thunar_protected_chooser_dialog_realize (GtkWidget *widget)
{
  ThunarProtectedChooserDialog *dialog = THUNAR_PROTECTED_CHOOSER_DIALOG (widget);

  /* let the GtkWindow class realize the dialog */
  (*GTK_WIDGET_CLASS (thunar_protected_chooser_dialog_parent_class)->realize) (widget);

  /* update the dialog header */
  thunar_protected_chooser_dialog_update_header (dialog);
}



static void
thunar_protected_chooser_dialog_response (GtkDialog *widget,
                                gint       response)
{
  ThunarProtectedChooserDialog *dialog = THUNAR_PROTECTED_CHOOSER_DIALOG (widget);
  GtkTreeSelection    *selection;
  GtkTreeModel        *model;
  GtkTreeIter          iter;
  const gchar         *exec;
  GAppInfo            *app_info = NULL;
  GError              *error = NULL;
  gchar               *path;
  gchar               *name;
  gchar               *s;

  /* no special processing for non-accept responses */
  if (G_UNLIKELY (response != GTK_RESPONSE_ACCEPT))
    return;

  /* determine the application that was chosen by the user */
  if (!gtk_expander_get_expanded (GTK_EXPANDER (dialog->custom_expander)))
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        gtk_tree_model_get (model, &iter, THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_APPLICATION, &app_info, -1);
    }
  else
    {
      /* determine the command line for the custom command */
      exec = gtk_entry_get_text (GTK_ENTRY (dialog->custom_entry));

      /* determine the path for the custom command */
      path = g_strdup (exec);
      s = strchr (path, ' ');
      if (G_UNLIKELY (s != NULL))
        *s = '\0';

      /* determine the name from the path of the custom command */
      name = g_path_get_basename (path);

      /* try to add an application for the custom command */
      app_info = g_app_info_create_from_commandline (exec, name, G_APP_INFO_CREATE_NONE, &error);

      /* verify the application */
      if (G_UNLIKELY (app_info == NULL))
        {
          /* display an error to the user */
          thunar_dialogs_show_error (GTK_WIDGET (dialog), error, _("Failed to add new application \"%s\""), name);

          /* release the error */
          g_error_free (error);
        }

      /* cleanup */
      g_free (path);
      g_free (name);
    }

  /* verify that we have a valid application */#
  if (G_UNLIKELY (app_info == NULL))
    return;

  /* store and cleanup */
  thunar_protected_chooser_dialog_set_current_info (dialog, app_info);
  g_object_unref (app_info);
}



static gboolean
thunar_protected_chooser_dialog_selection_func (GtkTreeSelection *selection,
                                      GtkTreeModel     *model,
                                      GtkTreePath      *path,
                                      gboolean          path_currently_selected,
                                      gpointer          user_data)
{
  GtkTreeIter iter;
  gboolean    permitted = TRUE;
  GValue      value = { 0, };

  /* we can always change the selection if the path is already selected */
  if (G_UNLIKELY (!path_currently_selected))
    {
      /* check if there's an application for the path */
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get_value (model, &iter, THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_APPLICATION, &value);
      permitted = (g_value_get_object (&value) != NULL);
      g_value_unset (&value);
    }

  return permitted;
}



static gboolean
thunar_protected_chooser_dialog_context_menu (ThunarProtectedChooserDialog *dialog,
                                    guint                button,
                                    guint32              timestamp)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GtkWidget        *menu;
  GAppInfo         *app_info;

  _thunar_return_val_if_fail (THUNAR_IS_PROTECTED_CHOOSER_DIALOG (dialog), FALSE);

  /* determine the selected row */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return FALSE;

  /* determine the app info for the row */
  gtk_tree_model_get (model, &iter, THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_APPLICATION, &app_info, -1);
  if (G_UNLIKELY (app_info == NULL))
    return FALSE;

  /* prepare the popup menu */
  menu = gtk_menu_new ();

  /* run the menu on the dialog's screen (takes over the floating of menu) */
  thunar_gtk_menu_run (GTK_MENU (menu), GTK_WIDGET (dialog), NULL, NULL, button, timestamp);

  /* clean up */
  g_object_unref (app_info);

  return TRUE;
}



static void
thunar_protected_chooser_dialog_update_accept (ThunarProtectedChooserDialog *dialog)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  const gchar      *text;
  gboolean          sensitive = FALSE;
  GValue            value = { 0, };

  _thunar_return_if_fail (THUNAR_IS_PROTECTED_CHOOSER_DIALOG (dialog));

  if (gtk_expander_get_expanded (GTK_EXPANDER (dialog->custom_expander)))
    {
      /* check if the user entered a valid custom command */
      text = gtk_entry_get_text (GTK_ENTRY (dialog->custom_entry));
      sensitive = (text != NULL && *text != '\0');
    }
  else
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
          /* check if the selected row refers to a valid application */
          gtk_tree_model_get_value (model, &iter, THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_APPLICATION, &value);
          sensitive = (g_value_get_object (&value) != NULL);
          g_value_unset (&value);
        }
    }

  /* update the "Ok" button sensitivity */
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, sensitive);
}



static void
thunar_protected_chooser_dialog_update_header (ThunarProtectedChooserDialog *dialog)
{
  GError      *error = NULL;
  GIcon       *icon;
  gchar       *text;

  _thunar_return_if_fail (THUNAR_IS_PROTECTED_CHOOSER_DIALOG (dialog));
  _thunar_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (dialog)));

  gtk_image_clear (GTK_IMAGE (dialog->header_image));
  gtk_label_set_text (GTK_LABEL (dialog->header_label), NULL);

  /* use a generic icon... */
  icon = g_icon_new_for_string ("firejail-protect", &error);
  if (G_UNLIKELY (error))
    {
      /* release the error, this isn't important enough to complain about */
      g_error_free (error);
    }
  else
    {
      gtk_image_set_from_gicon (GTK_IMAGE (dialog->header_image), icon, GTK_ICON_SIZE_DIALOG);
      g_object_unref (icon);
    }

  /* update the header label */
  text = g_strdup_printf (_("Open the selected protected files with:"));
  gtk_label_set_markup (GTK_LABEL (dialog->header_label), text);
  g_free (text);

  /* update the "Browse..." tooltip */
  thunar_gtk_widget_set_tooltip (dialog->custom_button,
                                 _("Browse the file system to select an "
                                   "application to open the selected protected files."));
}



static void
thunar_protected_chooser_dialog_browse_clicked (GtkWidget           *button,
                                      ThunarProtectedChooserDialog *dialog)
{
  GtkFileFilter *filter;
  GtkWidget     *chooser;
  gchar         *filename;
  gchar         *s;

  chooser = gtk_file_chooser_dialog_new (_("Select an Application"),
                                         GTK_WINDOW (dialog),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);

  /* add file chooser filters */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Executable Files"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-executable");
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Perl Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Python Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Ruby Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Shell Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  /* use the bindir as default folder */
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), BINDIR);

  /* setup the currently selected file */
  filename = gtk_editable_get_chars (GTK_EDITABLE (dialog->custom_entry), 0, -1);
  if (G_LIKELY (filename != NULL))
    {
      /* use only the first argument */
      s = strchr (filename, ' ');
      if (G_UNLIKELY (s != NULL))
        *s = '\0';

      /* check if we have a file name */
      if (G_LIKELY (*filename != '\0'))
        {
          /* check if the filename is not an absolute path */
          if (G_LIKELY (!g_path_is_absolute (filename)))
            {
              /* try to lookup the filename in $PATH */
              s = g_find_program_in_path (filename);
              if (G_LIKELY (s != NULL))
                {
                  /* use the absolute path instead */
                  g_free (filename);
                  filename = s;
                }
            }

          /* check if we have an absolute path now */
          if (G_LIKELY (g_path_is_absolute (filename)))
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), filename);
        }

      /* release the filename */
      g_free (filename);
    }

  /* run the chooser dialog */
  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
      gtk_entry_set_text (GTK_ENTRY (dialog->custom_entry), filename);
      g_free (filename);
    }

  gtk_widget_destroy (chooser);
}



static gboolean
thunar_protected_chooser_dialog_button_press_event (GtkWidget           *tree_view,
                                          GdkEventButton      *event,
                                          ThunarProtectedChooserDialog *dialog)
{
  GtkTreeSelection *selection;
  GtkTreePath      *path;

  _thunar_return_val_if_fail (THUNAR_IS_PROTECTED_CHOOSER_DIALOG (dialog), FALSE);
  _thunar_return_val_if_fail (dialog->tree_view == tree_view, FALSE);
  _thunar_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  /* check if we should popup the context menu */
  if (G_LIKELY (event->button == 3 && event->type == GDK_BUTTON_PRESS))
    {
      /* determine the path for the clicked row */
      if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, &path, NULL, NULL, NULL))
        {
          /* be sure to select exactly this row... */
          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
          gtk_tree_selection_unselect_all (selection);
          gtk_tree_selection_select_path (selection, path);
          gtk_tree_path_free (path);

          /* ...and popup the context menu */
          return thunar_protected_chooser_dialog_context_menu (dialog, event->button, event->time);
        }
    }

  return FALSE;
}



static void
thunar_protected_chooser_dialog_notify_expanded (GtkExpander         *expander,
                                       GParamSpec          *pspec,
                                       ThunarProtectedChooserDialog *dialog)
{
  GtkTreeSelection *selection;

  _thunar_return_if_fail (GTK_IS_EXPANDER (expander));
  _thunar_return_if_fail (THUNAR_IS_PROTECTED_CHOOSER_DIALOG (dialog));

  /* clear the application selection whenever the expander
   * is expanded to avoid confusion for the user.
   */
  if (gtk_expander_get_expanded (expander))
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
      gtk_tree_selection_unselect_all (selection);
    }

  /* update the sensitivity of the "Ok" button */
  thunar_protected_chooser_dialog_update_accept (dialog);
}



static void
thunar_protected_chooser_dialog_expand (ThunarProtectedChooserDialog *dialog)
{
  GtkTreeModel *model;
  GtkTreePath  *path;
  GtkTreeIter   iter;

  _thunar_return_if_fail (THUNAR_IS_PROTECTED_CHOOSER_DIALOG (dialog));

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->tree_view));

  /* expand the first tree view row (the recommended applications) */
  if (G_LIKELY (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter)))
    {
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
      gtk_tree_view_expand_to_path (GTK_TREE_VIEW (dialog->tree_view), path);
      gtk_tree_path_free (path);
    }

  /* expand the second tree view row (the other applications) */
  if (G_LIKELY (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter)))
    {
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
      gtk_tree_view_expand_to_path (GTK_TREE_VIEW (dialog->tree_view), path);
      gtk_tree_path_free (path);
    }

  /* reset the cursor */
  if (G_LIKELY (gtk_widget_get_realized (GTK_WIDGET (dialog))))
    gdk_window_set_cursor (GTK_WIDGET (dialog)->window, NULL);

  /* grab focus to the tree view widget */
  if (G_LIKELY (gtk_widget_get_realized (dialog->tree_view)))
    gtk_widget_grab_focus (dialog->tree_view);
}



static gboolean
thunar_protected_chooser_dialog_popup_menu (GtkWidget           *tree_view,
                                  ThunarProtectedChooserDialog *dialog)
{
  _thunar_return_val_if_fail (THUNAR_IS_PROTECTED_CHOOSER_DIALOG (dialog), FALSE);
  _thunar_return_val_if_fail (dialog->tree_view == tree_view, FALSE);
  _thunar_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);

  /* popup the context menu */
  return thunar_protected_chooser_dialog_context_menu (dialog, 0, gtk_get_current_event_time ());
}



static void
thunar_protected_chooser_dialog_row_activated (GtkTreeView         *treeview,
                                     GtkTreePath         *path,
                                     GtkTreeViewColumn   *column,
                                     ThunarProtectedChooserDialog *dialog)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GValue        value = { 0, };

  _thunar_return_if_fail (THUNAR_IS_PROTECTED_CHOOSER_DIALOG (dialog));
  _thunar_return_if_fail (GTK_IS_TREE_VIEW (treeview));

  /* determine the current chooser model */
  model = gtk_tree_view_get_model (treeview);
  if (G_UNLIKELY (model == NULL))
    return;

  /* determine the application for the tree path */
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get_value (model, &iter, THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_APPLICATION, &value);

  /* check if the row refers to a valid application */
  if (G_LIKELY (g_value_get_object (&value) != NULL))
    {
      /* emit the accept dialog response */
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    }
  else if (gtk_tree_view_row_expanded (treeview, path))
    {
      /* collapse the path that were double clicked */
      gtk_tree_view_collapse_row (treeview, path);
    }
  else
    {
      /* expand the path that were double clicked */
      gtk_tree_view_expand_to_path (treeview, path);
    }

  /* cleanup */
  g_value_unset (&value);
}



static void
thunar_protected_chooser_dialog_selection_changed (GtkTreeSelection    *selection,
                                         ThunarProtectedChooserDialog *dialog)
{
  GAppInfo     *app_info;
  GtkTreeModel *model;
  const gchar  *exec;
  GtkTreeIter   iter;

  /* determine the iterator for the selected row */
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* determine the app info for the selected row */
      gtk_tree_model_get (model, &iter, THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_APPLICATION, &app_info, -1);

      /* remember it */
//      thunar_protected_chooser_dialog_set_current_info (dialog, app_info);

      /* determine the command for the app info */
      exec = g_app_info_get_executable (app_info);
      if (G_LIKELY (exec != NULL && g_utf8_validate (exec, -1, NULL)))
        {
          /* setup the command as default for the custom command box */
          gtk_entry_set_text (GTK_ENTRY (dialog->custom_entry), exec);
        }

      /* cleanup */
      g_object_unref (app_info);
    }

  /* update the sensitivity of the "Ok" button */
  thunar_protected_chooser_dialog_update_accept (dialog);
}



void
thunar_protected_chooser_dialog_set_model (ThunarProtectedChooserDialog *dialog,
                                           ThunarProtectedChooserModel  *model)
{
  _thunar_return_if_fail (THUNAR_IS_PROTECTED_CHOOSER_DIALOG (dialog));
  _thunar_return_if_fail (THUNAR_IS_PROTECTED_CHOOSER_MODEL (model));

  gtk_tree_view_set_model (GTK_TREE_VIEW (THUNAR_PROTECTED_CHOOSER_DIALOG (dialog)->tree_view), GTK_TREE_MODEL (model));
  thunar_protected_chooser_dialog_expand (THUNAR_PROTECTED_CHOOSER_DIALOG (dialog));
}



/**
 * thunar_show_protected_chooser_dialog:
 * @parent : the #GtkWidget or the #GdkScreen on which to open the
 *           dialog. May also be %NULL in which case the default
 *           #GdkScreen will be used.
 *
 * Convenience function to display a #ThunarProtectedChooserDialog with the
 * given parameters.
 *
 * If @parent is a #GtkWidget the chooser dialog will be opened as
 * modal dialog above the @parent. Else if @parent is a screen (if
 * @parent is %NULL the default screen is used), the dialog won't
 * be modal and it will simply popup on the specified screen.
 **/
void
thunar_show_protected_chooser_dialog (gpointer    parent)
{
  ThunarApplication *application;
  GdkScreen         *screen;
  GtkWidget         *dialog;
  GtkWidget         *window = NULL;
  ThunarProtectedChooserModel *model;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));

  /* determine the screen for the dialog */
  if (G_UNLIKELY (parent == NULL))
    {
      /* just use the default screen, no toplevel window */
      screen = gdk_screen_get_default ();
    }
  else if (GTK_IS_WIDGET (parent))
    {
      /* use the screen for the widget and the toplevel window */
      screen = gtk_widget_get_screen (parent);
      window = gtk_widget_get_toplevel (parent);
    }
  else
    {
      /* parent is a screen, no toplevel window */
      screen = GDK_SCREEN (parent);
    }

  /* display the chooser dialog */
  dialog = g_object_new (THUNAR_TYPE_PROTECTED_CHOOSER_DIALOG,
                         "screen", screen,
                         NULL);

  /* fill up the model */
  model = thunar_protected_chooser_model_new ("*");
  thunar_protected_chooser_dialog_set_model (THUNAR_PROTECTED_CHOOSER_DIALOG (dialog), model);
  g_object_unref (G_OBJECT (model));

  /* update the header */
  if (gtk_widget_get_realized (dialog))
    thunar_protected_chooser_dialog_update_header (THUNAR_PROTECTED_CHOOSER_DIALOG (dialog));

  /* check if we have a toplevel window */
  if (G_LIKELY (window != NULL && gtk_widget_get_toplevel (window)))
    {
      /* dialog is transient for toplevel window and modal */
      gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
    }

  /* destroy the dialog after a user interaction */
  g_signal_connect_after (G_OBJECT (dialog), "response", G_CALLBACK (gtk_widget_destroy), NULL);

  /* let the application handle the dialog */
  application = thunar_application_get ();
  thunar_application_take_window (application, GTK_WINDOW (dialog));
  g_object_unref (G_OBJECT (application));

  /* display the dialog */
  gtk_widget_show (dialog);
}




