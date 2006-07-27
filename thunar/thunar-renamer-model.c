/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-file-monitor.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-renamer-model.h>



#define THUNAR_RENAMER_MODEL_ITEM(item) ((ThunarRenamerModelItem *) (item))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CAN_RENAME,
  PROP_FROZEN,
  PROP_MODE,
  PROP_RENAMER,
};



typedef struct _ThunarRenamerModelItem ThunarRenamerModelItem;



static void                    thunar_renamer_model_class_init          (ThunarRenamerModelClass *klass);
static void                    thunar_renamer_model_tree_model_init     (GtkTreeModelIface       *iface);
static void                    thunar_renamer_model_init                (ThunarRenamerModel      *renamer_model);
static void                    thunar_renamer_model_finalize            (GObject                 *object);
static void                    thunar_renamer_model_get_property        (GObject                 *object,
                                                                         guint                    prop_id,
                                                                         GValue                  *value,
                                                                         GParamSpec              *pspec);
static void                    thunar_renamer_model_set_property        (GObject                 *object,
                                                                         guint                    prop_id,
                                                                         const GValue            *value,
                                                                         GParamSpec              *pspec);
static GtkTreeModelFlags       thunar_renamer_model_get_flags           (GtkTreeModel            *tree_model);
static gint                    thunar_renamer_model_get_n_columns       (GtkTreeModel            *tree_model);
static GType                   thunar_renamer_model_get_column_type     (GtkTreeModel            *tree_model,
                                                                         gint                     column);
static gboolean                thunar_renamer_model_get_iter            (GtkTreeModel            *tree_model,
                                                                         GtkTreeIter             *iter,
                                                                         GtkTreePath             *path);
static GtkTreePath            *thunar_renamer_model_get_path            (GtkTreeModel            *tree_model,
                                                                         GtkTreeIter             *iter);
static void                    thunar_renamer_model_get_value           (GtkTreeModel            *tree_model,
                                                                         GtkTreeIter             *iter,
                                                                         gint                     column,
                                                                         GValue                  *value);
static gboolean                thunar_renamer_model_iter_next           (GtkTreeModel            *tree_model,
                                                                         GtkTreeIter             *iter);
static gboolean                thunar_renamer_model_iter_children       (GtkTreeModel            *tree_model,
                                                                         GtkTreeIter             *iter,
                                                                         GtkTreeIter             *parent);
static gboolean                thunar_renamer_model_iter_has_child      (GtkTreeModel            *tree_model,
                                                                         GtkTreeIter             *iter);
static gboolean                thunar_renamer_model_iter_n_children     (GtkTreeModel            *tree_model,
                                                                         GtkTreeIter             *iter);
static gboolean                thunar_renamer_model_iter_nth_child      (GtkTreeModel            *tree_model,
                                                                         GtkTreeIter             *iter,
                                                                         GtkTreeIter             *parent,
                                                                         gint                     n);
static gboolean                thunar_renamer_model_iter_parent         (GtkTreeModel            *tree_model,
                                                                         GtkTreeIter             *iter,
                                                                         GtkTreeIter             *child);
static void                    thunar_renamer_model_file_changed        (ThunarRenamerModel      *renamer_model,
                                                                         ThunarFile              *file,
                                                                         ThunarFileMonitor       *file_monitor);
static void                    thunar_renamer_model_file_destroyed      (ThunarRenamerModel      *renamer_model,
                                                                         ThunarFile              *file,
                                                                         ThunarFileMonitor       *file_monitor);
static void                    thunar_renamer_model_invalidate_all      (ThunarRenamerModel      *renamer_model);
static void                    thunar_renamer_model_invalidate_item     (ThunarRenamerModel      *renamer_model,
                                                                         ThunarRenamerModelItem  *item);
static gboolean                thunar_renamer_model_conflict_item       (ThunarRenamerModel      *renamer_model,
                                                                         ThunarRenamerModelItem  *item);
static gchar                  *thunar_renamer_model_process_item        (ThunarRenamerModel      *renamer_model,
                                                                         ThunarRenamerModelItem  *item,
                                                                         guint                    index);
static gboolean                thunar_renamer_model_update_idle         (gpointer                 user_data);
static void                    thunar_renamer_model_update_idle_destroy (gpointer                 user_data);
static ThunarRenamerModelItem *thunar_renamer_model_item_new            (ThunarFile              *file) G_GNUC_MALLOC;
static void                    thunar_renamer_model_item_free           (ThunarRenamerModelItem  *item);



struct _ThunarRenamerModelClass
{
  GObjectClass __parent__;
};

struct _ThunarRenamerModel
{
  GObject            __parent__;
  ThunarRenamerMode  mode;
  ThunarFileMonitor *file_monitor;
  ThunarxRenamer    *renamer;
  GList             *items;
  guint              stamp;

  /* TRUE if the model is currently frozen */
  gboolean           frozen;

  /* the idle source used to update the model */
  gint               update_idle_id;
};

struct _ThunarRenamerModelItem
{
  ThunarVfsInfo *info;
  ThunarFile    *file;
  gchar         *name;
  guint          changed : 1;  /* if the file changed */
  guint          conflict : 1; /* if the item conflicts with another item */
  guint          dirty : 1;    /* if the item must be updated */
};



static GObjectClass *thunar_renamer_model_parent_class;



GType
thunar_renamer_model_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarRenamerModelClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_renamer_model_class_init,
        NULL,
        NULL,
        sizeof (ThunarRenamerModel),
        0,
        (GInstanceInitFunc) thunar_renamer_model_init,
        NULL,
      };

      static const GInterfaceInfo tree_model_info =
      {
        (GInterfaceInitFunc) thunar_renamer_model_tree_model_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarRenamerModel"), &info, 0);
      g_type_add_interface_static (type, GTK_TYPE_TREE_MODEL, &tree_model_info);
    }

  return type;
}



static void
thunar_renamer_model_class_init (ThunarRenamerModelClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_renamer_model_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_renamer_model_finalize;
  gobject_class->get_property = thunar_renamer_model_get_property;
  gobject_class->set_property = thunar_renamer_model_set_property;

  /**
   * ThunarRenamerModel:can-rename:
   *
   * Whether the contents of the #ThunarRenamerModel
   * can be renamed. This is %TRUE if atleast one file
   * is in the model and no conflict is present.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CAN_RENAME,
                                   g_param_spec_boolean ("can-rename",
                                                         "can-rename",
                                                         "can-rename",
                                                         FALSE,
                                                         EXO_PARAM_READABLE));

  /**
   * ThunarRenamerModel:frozen:
   *
   * If a #ThunarRenamerModel is frozen, no updates will
   * be processed for the model and the "can-rename"
   * property will always be %FALSE.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FROZEN,
                                   g_param_spec_boolean ("frozen",
                                                         "frozen",
                                                         "frozen",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarRenamerModel:mode:
   *
   * The #ThunarRenamerMode used by this
   * #ThunarRenamerModel.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode", "mode", "mode",
                                                      THUNAR_TYPE_RENAMER_MODE,
                                                      THUNAR_RENAMER_MODE_NAME,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarRenamerModel:renamer:
   *
   * The #ThunarxRenamer that should be used by this
   * #ThunarRenamerModel.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_RENAMER,
                                   g_param_spec_object ("renamer",
                                                        "renamer",
                                                        "renamer",
                                                        THUNARX_TYPE_RENAMER,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_renamer_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags        = thunar_renamer_model_get_flags;
  iface->get_n_columns    = thunar_renamer_model_get_n_columns;
  iface->get_column_type  = thunar_renamer_model_get_column_type;
  iface->get_iter         = thunar_renamer_model_get_iter;
  iface->get_path         = thunar_renamer_model_get_path;
  iface->get_value        = thunar_renamer_model_get_value;
  iface->iter_next        = thunar_renamer_model_iter_next;
  iface->iter_children    = thunar_renamer_model_iter_children;
  iface->iter_has_child   = thunar_renamer_model_iter_has_child;
  iface->iter_n_children  = thunar_renamer_model_iter_n_children;
  iface->iter_nth_child   = thunar_renamer_model_iter_nth_child;
  iface->iter_parent      = thunar_renamer_model_iter_parent;
}



static void
thunar_renamer_model_init (ThunarRenamerModel *renamer_model)
{
  renamer_model->stamp = g_random_int ();
  renamer_model->update_idle_id = -1;

  /* connect to the file monitor */
  renamer_model->file_monitor = thunar_file_monitor_get_default ();
  g_signal_connect_swapped (G_OBJECT (renamer_model->file_monitor), "file-changed",
                            G_CALLBACK (thunar_renamer_model_file_changed), renamer_model);
  g_signal_connect_swapped (G_OBJECT (renamer_model->file_monitor), "file-destroyed",
                            G_CALLBACK (thunar_renamer_model_file_destroyed), renamer_model);
}



static void
thunar_renamer_model_finalize (GObject *object)
{
  ThunarRenamerModel *renamer_model = THUNAR_RENAMER_MODEL (object);

  /* reset the renamer property (must be first!) */
  thunar_renamer_model_set_renamer (renamer_model, NULL);

  /* release all items */
  g_list_foreach (renamer_model->items, (GFunc) thunar_renamer_model_item_free, NULL);
  g_list_free (renamer_model->items);

  /* disconnect from the file monitor */
  g_signal_handlers_disconnect_by_func (G_OBJECT (renamer_model->file_monitor), thunar_renamer_model_file_destroyed, renamer_model);
  g_signal_handlers_disconnect_by_func (G_OBJECT (renamer_model->file_monitor), thunar_renamer_model_file_changed, renamer_model);
  g_object_unref (G_OBJECT (renamer_model->file_monitor));

  /* be sure to cancel any pending update idle source (must be last!) */
  if (G_UNLIKELY (renamer_model->update_idle_id >= 0))
    g_source_remove (renamer_model->update_idle_id);

  (*G_OBJECT_CLASS (thunar_renamer_model_parent_class)->finalize) (object);
}



static void
thunar_renamer_model_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarRenamerModel *renamer_model = THUNAR_RENAMER_MODEL (object);

  switch (prop_id)
    {
    case PROP_CAN_RENAME:
      g_value_set_boolean (value, thunar_renamer_model_get_can_rename (renamer_model));
      break;

    case PROP_FROZEN:
      g_value_set_boolean (value, thunar_renamer_model_get_frozen (renamer_model));
      break;

    case PROP_MODE:
      g_value_set_enum (value, thunar_renamer_model_get_mode (renamer_model));
      break;

    case PROP_RENAMER:
      g_value_set_object (value, thunar_renamer_model_get_renamer (renamer_model));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_renamer_model_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarRenamerModel *renamer_model = THUNAR_RENAMER_MODEL (object);

  switch (prop_id)
    {
    case PROP_FROZEN:
      thunar_renamer_model_set_frozen (renamer_model, g_value_get_boolean (value));
      break;

    case PROP_MODE:
      thunar_renamer_model_set_mode (renamer_model, g_value_get_enum (value));
      break;

    case PROP_RENAMER:
      thunar_renamer_model_set_renamer (renamer_model, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static GtkTreeModelFlags
thunar_renamer_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}



static gint
thunar_renamer_model_get_n_columns (GtkTreeModel *tree_model)
{
  return THUNAR_RENAMER_MODEL_N_COLUMNS;
}



static GType
thunar_renamer_model_get_column_type (GtkTreeModel *tree_model,
                                      gint          column)
{
  switch (column)
    {
    case THUNAR_RENAMER_MODEL_COLUMN_CONFLICT:
      return G_TYPE_BOOLEAN;

    case THUNAR_RENAMER_MODEL_COLUMN_FILE:
      return THUNAR_TYPE_FILE;

    case THUNAR_RENAMER_MODEL_COLUMN_NEWNAME:
      return G_TYPE_STRING;

    case THUNAR_RENAMER_MODEL_COLUMN_OLDNAME:
      return G_TYPE_STRING;

    default:
      g_assert_not_reached ();
      return G_TYPE_INVALID;
    }
}



static gboolean
thunar_renamer_model_get_iter (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               GtkTreePath  *path)
{
  ThunarRenamerModel *renamer_model = THUNAR_RENAMER_MODEL (tree_model);
  GList              *lp;

  g_return_val_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  lp = g_list_nth (renamer_model->items, gtk_tree_path_get_indices (path)[0]);
  if (G_LIKELY (lp != NULL))
    {
      iter->stamp = renamer_model->stamp;
      iter->user_data = lp;
      return TRUE;
    }

  return FALSE;
}



static GtkTreePath*
thunar_renamer_model_get_path (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter)
{
  ThunarRenamerModel *renamer_model = THUNAR_RENAMER_MODEL (tree_model);
  gint                index;

  g_return_val_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model), NULL);
  g_return_val_if_fail (iter->stamp == renamer_model->stamp, NULL);

  /* determine the index of the item */
  index = g_list_position (renamer_model->items, iter->user_data);
  if (G_UNLIKELY (index < 0))
    return NULL;

  return gtk_tree_path_new_from_indices (index, -1);
}



static void
thunar_renamer_model_get_value (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter,
                                gint          column,
                                GValue       *value)
{
  ThunarRenamerModelItem *item;
  ThunarRenamerModel     *renamer_model = THUNAR_RENAMER_MODEL (tree_model);

  g_return_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model));
  g_return_if_fail (iter->stamp == renamer_model->stamp);

  item = g_list_nth_data (iter->user_data, 0);

  switch (column)
    {
    case THUNAR_RENAMER_MODEL_COLUMN_CONFLICT:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, item->conflict);
      break;

    case THUNAR_RENAMER_MODEL_COLUMN_FILE:
      g_value_init (value, THUNAR_TYPE_FILE);
      g_value_set_object (value, item->file);
      break;

    case THUNAR_RENAMER_MODEL_COLUMN_NEWNAME:
      g_value_init (value, G_TYPE_STRING);
      if (G_LIKELY (item->name != NULL))
        g_value_set_string (value, item->name);
      else if (item->conflict)
        g_value_set_static_string (value, thunar_file_get_display_name (item->file));
      else
        g_value_set_static_string (value, "");
      break;

    case THUNAR_RENAMER_MODEL_COLUMN_OLDNAME:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, thunar_file_get_display_name (item->file));
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}



static gboolean
thunar_renamer_model_iter_next (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter)
{
  g_return_val_if_fail (THUNAR_IS_RENAMER_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter->stamp == THUNAR_RENAMER_MODEL (tree_model)->stamp, FALSE);

  iter->user_data = g_list_next (iter->user_data);
  return (iter->user_data != NULL);
}



static gboolean
thunar_renamer_model_iter_children (GtkTreeModel *tree_model,
                                    GtkTreeIter  *iter,
                                    GtkTreeIter  *parent)
{
  ThunarRenamerModel *renamer_model = THUNAR_RENAMER_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model), FALSE);

  if (G_LIKELY (parent == NULL && renamer_model->items != NULL))
    {
      iter->stamp = renamer_model->stamp;
      iter->user_data = renamer_model->items;
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_renamer_model_iter_has_child (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter)
{
  return FALSE;
}



static gboolean
thunar_renamer_model_iter_n_children (GtkTreeModel *tree_model,
                                      GtkTreeIter  *iter)
{
  return (iter == NULL) ? g_list_length (THUNAR_RENAMER_MODEL (tree_model)->items) : 0;
}



static gboolean
thunar_renamer_model_iter_nth_child (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter,
                                     GtkTreeIter  *parent,
                                     gint          n)
{
  ThunarRenamerModel *renamer_model = THUNAR_RENAMER_MODEL (tree_model);

  g_return_val_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model), FALSE);

  if (G_LIKELY (parent != NULL))
    {
      iter->stamp = renamer_model->stamp;
      iter->user_data = g_list_nth (renamer_model->items, n);
      return (iter->user_data != NULL);
    }

  return FALSE;
}



static gboolean
thunar_renamer_model_iter_parent (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  GtkTreeIter  *child)
{
  return FALSE;
}



static void
thunar_renamer_model_file_changed (ThunarRenamerModel *renamer_model,
                                   ThunarFile         *file,
                                   ThunarFileMonitor  *file_monitor)
{
  ThunarRenamerModelItem *item;
  GtkTreePath            *path;
  GtkTreeIter             iter;
  GList                  *lp;

  g_return_if_fail (THUNAR_IS_FILE (file));
  g_return_if_fail (THUNAR_IS_FILE_MONITOR (file_monitor));
  g_return_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model));
  g_return_if_fail (renamer_model->file_monitor == file_monitor);

  /* check if we have that file */
  for (lp = renamer_model->items; lp != NULL; lp = lp->next)
    if (THUNAR_RENAMER_MODEL_ITEM (lp->data)->file == file)
      {
        /* check if the file changed on disk */
        item = THUNAR_RENAMER_MODEL_ITEM (lp->data);
        if (!thunar_vfs_info_matches (item->info, thunar_file_get_info (file)))
          {
            /* connect to the new info */
            thunar_vfs_info_unref (item->info);
            item->info = thunar_vfs_info_ref (thunar_file_get_info (file));

            /* check if we're frozen */
            if (G_LIKELY (!renamer_model->frozen))
              {
                /* the file changed */
                THUNAR_RENAMER_MODEL_ITEM (lp->data)->changed = TRUE;

                /* invalidate the item */
                thunar_renamer_model_invalidate_item (renamer_model, lp->data);
                break;
              }
          }

        /* determine the iter for the item */
        iter.stamp = renamer_model->stamp;
        iter.user_data = lp;

        /* emit "row-changed" to display up2date file name */
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (renamer_model), &iter);
        gtk_tree_model_row_changed (GTK_TREE_MODEL (renamer_model), path, &iter);
        gtk_tree_path_free (path);
        break;
      }
}



static void
thunar_renamer_model_file_destroyed (ThunarRenamerModel *renamer_model,
                                     ThunarFile         *file,
                                     ThunarFileMonitor  *file_monitor)
{
  GtkTreePath *path;
  GList       *lp;
  gint         index;

  g_return_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model));
  g_return_if_fail (renamer_model->file_monitor == file_monitor);
  g_return_if_fail (THUNAR_IS_FILE_MONITOR (file_monitor));
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* check if we have that file */
  for (lp = renamer_model->items; lp != NULL; lp = lp->next)
    if (THUNAR_RENAMER_MODEL_ITEM (lp->data)->file == file)
      {
        /* determine the index of the item */
        index = g_list_position (renamer_model->items, lp);

        /* free the item data */
        thunar_renamer_model_item_free (lp->data);

        /* drop the item from the list */
        renamer_model->items = g_list_delete_link (renamer_model->items, lp);

        /* tell the view that the item is gone */
        path = gtk_tree_path_new_from_indices (index, -1);
        gtk_tree_model_row_deleted (GTK_TREE_MODEL (renamer_model), path);
        gtk_tree_path_free (path);

        /* invalidate all other items */
        thunar_renamer_model_invalidate_all (renamer_model);
        break;
      }
}



static void
thunar_renamer_model_invalidate_all (ThunarRenamerModel *renamer_model)
{
  GList *lp;

  /* invalidate all items in the model */
  for (lp = renamer_model->items; lp != NULL; lp = lp->next)
    thunar_renamer_model_invalidate_item (renamer_model, lp->data);
}



static void
thunar_renamer_model_invalidate_item (ThunarRenamerModel     *renamer_model,
                                      ThunarRenamerModelItem *item)
{
  /* mark the item as dirty */
  item->dirty = TRUE;

  /* check if the update idle source is already running and not frozen */
  if (G_UNLIKELY (renamer_model->update_idle_id < 0 && !renamer_model->frozen))
    {
      /* schedule the update idle source */
      renamer_model->update_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_renamer_model_update_idle,
                                                       renamer_model, thunar_renamer_model_update_idle_destroy);

      /* notify listeners that we're updating */
      g_object_notify (G_OBJECT (renamer_model), "can-rename");
    }
}



static gboolean
trm_same_directory (ThunarFile *a,
                    ThunarFile *b)
{
  ThunarVfsPath *parent_a;
  ThunarVfsPath *parent_b;

  /* determine the parent paths for both files */
  parent_a = thunar_vfs_path_get_parent (thunar_file_get_path (a));
  parent_b = thunar_vfs_path_get_parent (thunar_file_get_path (b));

  /* check if both files have the same parent */
  return (parent_a != NULL && parent_b != NULL && thunar_vfs_path_equal (parent_a, parent_b));
}



static gboolean
thunar_renamer_model_conflict_item (ThunarRenamerModel     *renamer_model,
                                    ThunarRenamerModelItem *item)
{
  ThunarRenamerModelItem *oitem;
  GtkTreePath            *path;
  GtkTreeIter             iter;
  gboolean                conflict = FALSE;
  GList                  *lp;

  for (lp = renamer_model->items; lp != NULL; lp = lp->next)
    {
      /* check if this item is dirty or the item we're about to check */
      oitem = THUNAR_RENAMER_MODEL_ITEM (lp->data);
      if (G_UNLIKELY (oitem == item || oitem->dirty))
        continue;

      /* check if this other item conflicts with the item in question (can only conflict if in same directory) */
      if (trm_same_directory (item->file, oitem->file)
          && ((item->name != NULL && oitem->name == NULL && strcmp (item->name, thunar_file_get_display_name (oitem->file)) == 0)
           || (item->name == NULL && oitem->name != NULL && strcmp (thunar_file_get_display_name (item->file), oitem->name) == 0)
           || (item->name != NULL && oitem->name != NULL && strcmp (item->name, oitem->name) == 0)))
        {
          /* check if the other item is already in conflict state */
          if (G_LIKELY (!oitem->conflict))
            {
              /* set to conflict state */
              oitem->conflict = TRUE;

              /* determine iter for other item */
              iter.stamp = renamer_model->stamp;
              iter.user_data = lp;

              /* emit "row-changed" for the other item */
              path = gtk_tree_model_get_path (GTK_TREE_MODEL (renamer_model), &iter);
              gtk_tree_model_row_changed (GTK_TREE_MODEL (renamer_model), path, &iter);
              gtk_tree_path_free (path);
            }

          /* this item conflicts */
          conflict = TRUE;
        }
    }

  return conflict;
}



static gchar*
thunar_renamer_model_process_item (ThunarRenamerModel     *renamer_model,
                                   ThunarRenamerModelItem *item,
                                   guint                   index)
{
  ThunarRenamerMode mode;
  const gchar      *display_name;
  const gchar      *dot;
  gchar            *name = NULL;
  gchar            *prefix;
  gchar            *suffix;
  gchar            *text;

  /* no new name if no renamer is set */
  if (G_UNLIKELY (renamer_model->renamer == NULL))
    return NULL;

  /* determine the current display name of the file */
  display_name = thunar_file_get_display_name (item->file);

  /* determine the last dot in the filename */
  dot = strrchr (display_name, '.');
  if (G_UNLIKELY (dot == display_name || (dot != NULL && dot[1] == '\0')))
    dot = NULL;

  /* if we don't have a dot, then no "Suffix only" rename can take place */
  if (G_LIKELY (dot != NULL || renamer_model->mode != THUNAR_RENAMER_MODE_SUFFIX))
    {
      /* now, for "Name only", we need a dot, otherwise treat everything as name */
      if (renamer_model->mode == THUNAR_RENAMER_MODE_NAME && dot == NULL)
        mode = THUNAR_RENAMER_MODE_BOTH;
      else
        mode = renamer_model->mode;

      /* determine the new name according to the mode */
      switch (mode)
        {
        case THUNAR_RENAMER_MODE_NAME:
          /* determine the name part of the display name */
          text = g_strndup (display_name, (dot - display_name));

          /* determine the new name */
          prefix = thunarx_renamer_process (renamer_model->renamer, THUNARX_FILE_INFO (item->file), text, index);

          /* determine the new full name */
          name = g_strconcat (prefix, dot, NULL);

          /* release the temporary strings */
          g_free (prefix);
          g_free (text);
          break;

        case THUNAR_RENAMER_MODE_SUFFIX:
          /* determine the new suffix */
          suffix = thunarx_renamer_process (renamer_model->renamer, THUNARX_FILE_INFO (item->file), dot + 1, index);

          /* generate the new file name */
          name = g_new (gchar, (dot - display_name) + 1 + strlen (suffix) + 1);
          memcpy (name, display_name, (dot - display_name) + 1);
          memcpy (name + (dot - display_name) + 1, suffix, strlen (suffix) + 1);

          /* release the suffix */
          g_free (suffix);
          break;

        case THUNAR_RENAMER_MODE_BOTH:
          /* determine the new full name */
          name = thunarx_renamer_process (renamer_model->renamer, THUNARX_FILE_INFO (item->file), display_name, index);
          break;

        default:
          g_assert_not_reached ();
          break;
        }
    }

  /* check if the new name is equal to the old one */
  if (exo_str_is_equal (name, display_name))
    {
      /* just return NULL then */
      g_free (name);
      name = NULL;
    }

  return name;
}



static gboolean
thunar_renamer_model_update_idle (gpointer user_data)
{
  ThunarRenamerModelItem *item;
  ThunarRenamerModel     *renamer_model = THUNAR_RENAMER_MODEL (user_data);
  GtkTreePath            *path;
  GtkTreeIter             iter;
  gboolean                changed = FALSE;
  gboolean                conflict;
  guint                   index;
  gchar                  *name;
  GList                  *lp;

  GDK_THREADS_ENTER ();

  /* don't do anything if the model is frozen */
  if (G_LIKELY (!renamer_model->frozen))
    {
      /* process the first dirty item */
      for (index = 0, lp = renamer_model->items; !changed && lp != NULL; ++index, lp = lp->next)
        {
          /* check if this item is dirty */
          item = THUNAR_RENAMER_MODEL_ITEM (lp->data);
          if (G_LIKELY (!item->dirty))
            continue;

          /* check if the file changed */
          changed = item->changed;

          /* mark as valid, since we're updating right now */
          item->changed = FALSE;
          item->dirty = FALSE;

          /* determine the new name for the item */
          name = thunar_renamer_model_process_item (renamer_model, item, index);
          if (!exo_str_is_equal (item->name, name))
            {
              /* apply new name */
              g_free (item->name);
              item->name = name;

              /* the item changed */
              changed = TRUE;
            }
          else
            {
              /* release temporary name */
              g_free (name);
            }

          /* check if this item conflicts with any other item */
          conflict = thunar_renamer_model_conflict_item (renamer_model, item);
          if (item->conflict != conflict)
            {
              /* apply the new state */
              item->conflict = conflict;

              /* the item changed */
              changed = TRUE;
            }

          /* check if the item changed */
          if (G_LIKELY (changed))
            {
              /* generate the iter for the item */
              iter.stamp = renamer_model->stamp;
              iter.user_data = lp;

              /* emit "row-changed" for this item */
              path = gtk_tree_path_new_from_indices (index, -1);
              gtk_tree_model_row_changed (GTK_TREE_MODEL (renamer_model), path, &iter);
              gtk_tree_path_free (path);
            }
        }
    }

  GDK_THREADS_LEAVE ();

  /* keep the idle source as long as any item changed */
  return changed;
}



static void
thunar_renamer_model_update_idle_destroy (gpointer user_data)
{
  /* reset the update idle id... */
  THUNAR_RENAMER_MODEL (user_data)->update_idle_id = -1;

  /* ...and notify listeners */
  g_object_notify (G_OBJECT (user_data), "can-rename");
}



static ThunarRenamerModelItem*
thunar_renamer_model_item_new (ThunarFile *file)
{
  ThunarRenamerModelItem *item;

  item = _thunar_slice_new0 (ThunarRenamerModelItem);
  item->info = thunar_vfs_info_ref (thunar_file_get_info (file));
  item->file = g_object_ref (G_OBJECT (file));
  item->dirty = TRUE;

  return item;
}



static void
thunar_renamer_model_item_free (ThunarRenamerModelItem *item)
{
  g_object_unref (G_OBJECT (item->file));
  thunar_vfs_info_unref (item->info);
  g_free (item->name);
  _thunar_slice_free (ThunarRenamerModelItem, item);
}



/**
 * thunar_renamer_model_new:
 *
 * Allocates a new #ThunarRenamerModel instance.
 *
 * Return value: the newly allocated #ThunarRenamerModel.
 **/
ThunarRenamerModel*
thunar_renamer_model_new (void)
{
  return g_object_new (THUNAR_TYPE_RENAMER_MODEL, NULL);
}



/**
 * thunar_renamer_model_get_can_rename:
 * @renamer_model : a #ThunarRenamerModel.
 *
 * Returns %TRUE if the @renamer_model contents
 * can be renamed, i.e. if no conflict exist and
 * atleast one file is present.
 *
 * This method will always return %FALSE if the
 * @renamer_model is frozen. See thunar_renamer_model_set_frozen()
 * and thunar_renamer_model_get_frozen().
 *
 * Return value: %TRUE if bulk rename can be performed.
 **/
gboolean
thunar_renamer_model_get_can_rename (ThunarRenamerModel *renamer_model)
{
  gboolean can_rename = FALSE;
  GList   *lp;

  g_return_val_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model), FALSE);

  if (G_LIKELY (renamer_model->renamer != NULL && !renamer_model->frozen && renamer_model->update_idle_id < 0))
    {
      /* check if atleast one item has a new name and no conflicts exist */
      for (lp = renamer_model->items; lp != NULL; lp = lp->next)
        {
          /* check if we have a conflict here */
          if (G_UNLIKELY (THUNAR_RENAMER_MODEL_ITEM (lp->data)->conflict))
            return FALSE;

          /* check if the item has a new name */
          if (THUNAR_RENAMER_MODEL_ITEM (lp->data)->name != NULL)
            can_rename = TRUE;
        }
    }

  return can_rename;
}



/**
 * thunar_renamer_model_get_frozen:
 * @renamer_model : a #ThunarRenamerModel.
 *
 * Returns %TRUE if the @renamer_model<!---->s state is
 * frozen, that is, no updates will be processed on the
 * @renamer_model.
 *
 * Return value: %TRUE if @renamer_model is frozen.
 **/
gboolean
thunar_renamer_model_get_frozen (ThunarRenamerModel *renamer_model)
{
  g_return_val_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model), FALSE);
  return renamer_model->frozen;
}



/**
 * thunar_renamer_model_set_frozen:
 * @renamer_model : a #ThunarRenamerModel.
 * @frozen        : %TRUE to freeze the current @renamer_model<!---->s state.
 *
 * If @frozen is %TRUE the current state of the @renamer_model will be frozen
 * and no updates will be processed until you call this method again with a
 * value of %FALSE for @frozen.
 **/
void
thunar_renamer_model_set_frozen (ThunarRenamerModel *renamer_model,
                                 gboolean            frozen)
{
  g_return_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model));

  /* normalize the value */
  frozen = !!frozen;

  /* check if we have a new value */
  if (G_LIKELY (renamer_model->frozen != frozen))
    {
      /* apply the new setting */
      renamer_model->frozen = frozen;

      /* change state appropriately */
      if (G_LIKELY (frozen))
        {
          /* cancel any pending update idle source */
          if (G_UNLIKELY (renamer_model->update_idle_id >= 0))
            g_source_remove (renamer_model->update_idle_id);
        }
      else
        {
          /* thaw the model, updating all items */
          thunar_renamer_model_invalidate_all (renamer_model);
        }

      /* notify listeners */
      g_object_freeze_notify (G_OBJECT (renamer_model));
      g_object_notify (G_OBJECT (renamer_model), "can-rename");
      g_object_notify (G_OBJECT (renamer_model), "frozen");
      g_object_thaw_notify (G_OBJECT (renamer_model));
    }
}



/**
 * thunar_renamer_model_get_mode:
 * @renamer_model : a #ThunarRenamerModel.
 *
 * Returns the #ThunarRenamerMode currently set
 * for @renamer_model.
 *
 * Return value: the mode for @renamer_model.
 **/
ThunarRenamerMode
thunar_renamer_model_get_mode (ThunarRenamerModel *renamer_model)
{
  g_return_val_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model), THUNAR_RENAMER_MODE_NAME);
  return renamer_model->mode;
}



/**
 * thunar_renamer_model_set_mode:
 * @renamer_model : a #ThunarRenamerModel.
 * @mode              : a #ThunarRenamerMode.
 *
 * Sets the rename operation mode of @renamer_model
 * to @mode and updates all items.
 **/
void
thunar_renamer_model_set_mode (ThunarRenamerModel *renamer_model,
                               ThunarRenamerMode   mode)
{
  g_return_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model));

  /* check if we're already in the requested mode */
  if (G_UNLIKELY (renamer_model->mode == mode))
    return;

  /* apply the new mode */
  renamer_model->mode = mode;

  /* invalidate all items */
  thunar_renamer_model_invalidate_all (renamer_model);

  /* notify listeners */
  g_object_notify (G_OBJECT (renamer_model), "mode");
}



/**
 * thunar_renamer_model_get_renamer:
 * @renamer_model : a #ThunarRenamerModel.
 *
 * Returns the #ThunarxRenamer currently used by
 * the specified @renamer_model.
 *
 * Return value: the current renamer for @renamer_model.
 **/
ThunarxRenamer*
thunar_renamer_model_get_renamer (ThunarRenamerModel *renamer_model)
{
  g_return_val_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model), NULL);
  return renamer_model->renamer;
}



/**
 * thunar_renamer_model_set_renamer:
 * @renamer_model : a #ThunarRenamerModel.
 * @renamer           : a #ThunarxRenamer or %NULL.
 *
 * Sets the renamer for @renamer_model to the
 * specified @renamer.
 **/
void
thunar_renamer_model_set_renamer (ThunarRenamerModel *renamer_model,
                                  ThunarxRenamer     *renamer)
{
  g_return_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model));
  g_return_if_fail (renamer == NULL || THUNARX_IS_RENAMER (renamer));

  /* verify that we do not already use the renamer */
  if (G_UNLIKELY (renamer_model->renamer == renamer))
    return;

  /* disconnect from the previous renamer */
  if (renamer_model->renamer != NULL)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (renamer_model->renamer), thunar_renamer_model_invalidate_all, renamer_model);
      g_object_unref (G_OBJECT (renamer_model->renamer));
    }

  /* activate the new renamer */
  renamer_model->renamer = renamer;

  /* connect to the new renamer */
  if (G_LIKELY (renamer != NULL))
    {
      g_signal_connect_swapped (G_OBJECT (renamer), "changed", G_CALLBACK (thunar_renamer_model_invalidate_all), renamer_model);
      g_object_ref (G_OBJECT (renamer));
    }

  /* invalidate all items */
  thunar_renamer_model_invalidate_all (renamer_model);

  /* notify listeners */
  g_object_notify (G_OBJECT (renamer_model), "renamer");
}



/**
 * thunar_renamer_model_append:
 * @renamer_model : a #ThunarRenamerModel.
 * @file              : the #ThunarFile to add to @renamer_model.
 *
 * Adds the @file to the @renamer_model.
 **/
void
thunar_renamer_model_append (ThunarRenamerModel *renamer_model,
                             ThunarFile         *file)
{
  ThunarRenamerModelItem *item;
  GtkTreePath            *path;
  GtkTreeIter             iter;
  GList                  *lp;

  g_return_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model));
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* check if we already have that file */
  for (lp = renamer_model->items; lp != NULL; lp = lp->next)
    if (THUNAR_RENAMER_MODEL_ITEM (lp->data)->file == file)
      return;

  /* allocate a new item for the file */
  item = thunar_renamer_model_item_new (file);

  /* append the item to the model */
  renamer_model->items = g_list_append (renamer_model->items, item);

  /* determine the iterator for the new item */
  iter.stamp = renamer_model->stamp;
  iter.user_data = g_list_last (renamer_model->items);

  /* emit the "row-inserted" signal */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (renamer_model), &iter);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (renamer_model), path, &iter);
  gtk_tree_path_free (path);

  /* invalidate the newly added item */
  thunar_renamer_model_invalidate_item (renamer_model, item);
}



/**
 * thunar_renamer_model_clear:
 * @renamer_model : a #ThunarRenamerModel.
 *
 * Clears the @renamer_model by removing all files
 * from it.
 **/
void
thunar_renamer_model_clear (ThunarRenamerModel *renamer_model)
{
  g_return_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model));

  /* grab an additional reference on the model */
  g_object_ref (G_OBJECT (renamer_model));

  /* freeze notifications */
  g_object_freeze_notify (G_OBJECT (renamer_model));

  /* delete all items from the model */
  while (renamer_model->items != NULL)
    {
      /* just use the "file-destroyed" handler here to drop the first item */
      thunar_renamer_model_file_destroyed (renamer_model, THUNAR_RENAMER_MODEL_ITEM (renamer_model->items->data)->file,
                                           renamer_model->file_monitor);
    }

  /* thaw notifications */
  g_object_thaw_notify (G_OBJECT (renamer_model));

  /* release the additional reference on the model */
  g_object_unref (G_OBJECT (renamer_model));
}



/**
 * thunar_renamer_model_remove:
 * @renamer_model : a #ThunarRenamerModel.
 * @path          : a #GtkTreePath in the @renamer_model.
 *
 * Removes the item identified by the given @path from
 * the @renamer_model.
 **/
void
thunar_renamer_model_remove (ThunarRenamerModel *renamer_model,
                             GtkTreePath        *path)
{
  GList *lp;

  g_return_if_fail (THUNAR_IS_RENAMER_MODEL (renamer_model));
  g_return_if_fail (gtk_tree_path_get_depth (path) == 1);

  /* determine the list item for the path and verify that its valid */
  lp = g_list_nth (renamer_model->items, gtk_tree_path_get_indices (path)[0]);
  if (G_UNLIKELY (lp == NULL))
    return;

  /* free the item data */
  thunar_renamer_model_item_free (lp->data);

  /* drop the item from the list */
  renamer_model->items = g_list_delete_link (renamer_model->items, lp);

  /* tell the view that the item is gone */
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (renamer_model), path);

  /* invalidate all other items */
  thunar_renamer_model_invalidate_all (renamer_model);
}

