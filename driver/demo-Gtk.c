/* demo-Gtk.c --- implements the interactive demo-mode and options dialogs.
 * xscreensaver, Copyright (c) 1993-2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_GTK /* whole file */

#include <xscreensaver-intl.h>

#include <stdlib.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

# ifdef __GNUC__
#  define STFU __extension__  /* ignore gcc -pendantic warnings in next sexp */
# else
#  define STFU /* */
# endif


#ifdef ENABLE_NLS
# include <locale.h>
#endif /* ENABLE_NLS */

#ifndef VMS
# include <pwd.h>		/* for getpwuid() */
#else /* VMS */
# include "vms-pwd.h"
#endif /* VMS */

#ifdef HAVE_UNAME
# include <sys/utsname.h>	/* for uname() */
#endif /* HAVE_UNAME */

#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>


#include <signal.h>
#include <errno.h>
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif


#include <X11/Xproto.h>		/* for CARD32 */
#include <X11/Xatom.h>		/* for XA_INTEGER */
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

/* We don't actually use any widget internals, but these are included
   so that gdb will have debug info for the widgets... */
#include <X11/IntrinsicP.h>
#include <X11/ShellP.h>

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Error.h>
# else /* VMS */
#  include <Xmu/Error.h>
# endif
#else
# include "xmu.h"
#endif

#ifdef HAVE_XINERAMA
# include <X11/extensions/Xinerama.h>
#endif /* HAVE_XINERAMA */

#include <gtk/gtk.h>

#ifdef HAVE_CRAPPLET
# include <gnome.h>
# include <capplet-widget.h>
#endif

#include <gdk/gdkx.h>

#ifdef HAVE_GTK2
# include <glade/glade-xml.h>
# include <gmodule.h>
#else  /* !HAVE_GTK2 */
# define G_MODULE_EXPORT /**/
#endif /* !HAVE_GTK2 */

#if defined(DEFAULT_ICONDIR) && !defined(GLADE_DIR)
# define GLADE_DIR DEFAULT_ICONDIR
#endif
#if !defined(DEFAULT_ICONDIR) && defined(GLADE_DIR)
# define DEFAULT_ICONDIR GLADE_DIR
#endif

#ifndef HAVE_XML
 /* Kludge: this is defined in demo-Gtk-conf.c when HAVE_XML.
    It is unused otherwise, so in that case, stub it out. */
 static const char *hack_configuration_path = 0;
#endif



#include "version.h"
#include "prefs.h"
#include "resources.h"		/* for parse_time() */
#include "visual.h"		/* for has_writable_cells() */
#include "remote.h"		/* for xscreensaver_command() */
#include "usleep.h"

#include "logo-50.xpm"
#include "logo-180.xpm"

#undef dgettext  /* else these are defined twice... */
#undef dcgettext

#include "demo-Gtk-widgets.h"
#include "demo-Gtk-support.h"
#include "demo-Gtk-conf.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_GTK2
enum {
  COL_ENABLED,
  COL_NAME,
  COL_LAST
};
#endif /* HAVE_GTK2 */

/* from exec.c */
extern void exec_command (const char *shell, const char *command, int nice);
extern int on_path_p (const char *program);

static void hack_subproc_environment (Window preview_window_id, Bool debug_p);

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


/* You might think that to read an array of 32-bit quantities out of a
   server-side property, you would pass an array of 32-bit data quantities
   into XGetWindowProperty().  You would be wrong.  You have to use an array
   of longs, even if long is 64 bits (using 32 of each 64.)
 */
typedef long PROP32;

char *progname = 0;
char *progclass = "XScreenSaver";
XrmDatabase db;

/* The order of the items in the mode menu. */
static int mode_menu_order[] = {
  DONT_BLANK, BLANK_ONLY, ONE_HACK, RANDOM_HACKS, RANDOM_HACKS_SAME };


typedef struct {

  char *short_version;		/* version number of this xscreensaver build */

  GtkWidget *toplevel_widget;	/* the main window */
  GtkWidget *base_widget;	/* root of our hierarchy (for name lookups) */
  GtkWidget *popup_widget;	/* the "Settings" dialog */
  conf_data *cdata;		/* private data for per-hack configuration */

#ifdef HAVE_GTK2
  GladeXML *glade_ui;           /* Glade UI file */
#endif /* HAVE_GTK2 */

  Bool debug_p;			/* whether to print diagnostics */
  Bool initializing_p;		/* flag for breaking recursion loops */
  Bool saving_p;		/* flag for breaking recursion loops */

  char *desired_preview_cmd;	/* subprocess we intend to run */
  char *running_preview_cmd;	/* subprocess we are currently running */
  pid_t running_preview_pid;	/* pid of forked subproc (might be dead) */
  Bool running_preview_error_p;	/* whether the pid died abnormally */

  Bool preview_suppressed_p;	/* flag meaning "don't launch subproc" */
  int subproc_timer_id;		/* timer to delay subproc launch */
  int subproc_check_timer_id;	/* timer to check whether it started up */
  int subproc_check_countdown;  /* how many more checks left */

  int *list_elt_to_hack_number;	/* table for sorting the hack list */
  int *hack_number_to_list_elt;	/* the inverse table */
  Bool *hacks_available_p;	/* whether hacks are on $PATH */
  int total_available;		/* how many are on $PATH */
  int list_count;		/* how many items are in the list: this may be
                                   less than p->screenhacks_count, if some are
                                   suppressed. */

  int _selected_list_element;	/* don't use this: call
                                   selected_list_element() instead */

  int nscreens;			/* How many X or Xinerama screens there are */

  saver_preferences prefs;

} state;


/* Total fucking evilness due to the fact that it's rocket science to get
   a closure object of our own down into the various widget callbacks. */
static state *global_state_kludge;

Atom XA_VROOT;
Atom XA_SCREENSAVER, XA_SCREENSAVER_RESPONSE, XA_SCREENSAVER_VERSION;
Atom XA_SCREENSAVER_ID, XA_SCREENSAVER_STATUS, XA_SELECT, XA_DEMO;
Atom XA_ACTIVATE, XA_BLANK, XA_LOCK, XA_RESTART, XA_EXIT;


static void populate_demo_window (state *, int list_elt);
static void populate_prefs_page (state *);
static void populate_popup_window (state *);

static Bool flush_dialog_changes_and_save (state *);
static Bool flush_popup_changes_and_save (state *);

static int maybe_reload_init_file (state *);
static void await_xscreensaver (state *);
static Bool xscreensaver_running_p (state *);
static void sensitize_menu_items (state *s, Bool force_p);
static void force_dialog_repaint (state *s);

static void schedule_preview (state *, const char *cmd);
static void kill_preview_subproc (state *, Bool reset_p);
static void schedule_preview_check (state *);


/* Prototypes of functions used by the Glade-generated code,
   to avoid warnings.
 */
void exit_menu_cb (GtkMenuItem *, gpointer user_data);
void about_menu_cb (GtkMenuItem *, gpointer user_data);
void doc_menu_cb (GtkMenuItem *, gpointer user_data);
void file_menu_cb (GtkMenuItem *, gpointer user_data);
void activate_menu_cb (GtkMenuItem *, gpointer user_data);
void lock_menu_cb (GtkMenuItem *, gpointer user_data);
void kill_menu_cb (GtkMenuItem *, gpointer user_data);
void restart_menu_cb (GtkWidget *, gpointer user_data);
void run_this_cb (GtkButton *, gpointer user_data);
void manual_cb (GtkButton *, gpointer user_data);
void run_next_cb (GtkButton *, gpointer user_data);
void run_prev_cb (GtkButton *, gpointer user_data);
void pref_changed_cb (GtkWidget *, gpointer user_data);
gboolean pref_changed_event_cb (GtkWidget *, GdkEvent *, gpointer user_data);
void mode_menu_item_cb (GtkWidget *, gpointer user_data);
void switch_page_cb (GtkNotebook *, GtkNotebookPage *, 
                     gint page_num, gpointer user_data);
void browse_image_dir_cb (GtkButton *, gpointer user_data);
void browse_text_file_cb (GtkButton *, gpointer user_data);
void browse_text_program_cb (GtkButton *, gpointer user_data);
void settings_cb (GtkButton *, gpointer user_data);
void settings_adv_cb (GtkButton *, gpointer user_data);
void settings_std_cb (GtkButton *, gpointer user_data);
void settings_switch_page_cb (GtkNotebook *, GtkNotebookPage *,
                              gint page_num, gpointer user_data);
void settings_cancel_cb (GtkButton *, gpointer user_data);
void settings_ok_cb (GtkButton *, gpointer user_data);

static void kill_gnome_screensaver (void);
static void kill_kde_screensaver (void);


/* Some random utility functions
 */

const char *blurb (void);

const char *
blurb (void)
{
  time_t now = time ((time_t *) 0);
  char *ct = (char *) ctime (&now);
  static char buf[255];
  int n = strlen(progname);
  if (n > 100) n = 99;
  strncpy(buf, progname, n);
  buf[n++] = ':';
  buf[n++] = ' ';
  strncpy(buf+n, ct+11, 8);
  strcpy(buf+n+9, ": ");
  return buf;
}


static GtkWidget *
name_to_widget (state *s, const char *name)
{
  GtkWidget *w;
  if (!s) abort();
  if (!name) abort();
  if (!*name) abort();

#ifdef HAVE_GTK2
  if (!s->glade_ui)
    {
      /* First try to load the Glade file from the current directory;
         if there isn't one there, check the installed directory.
       */
# define GLADE_FILE_NAME "xscreensaver-demo.glade2"
      const char * const files[] = { GLADE_FILE_NAME,
                                     GLADE_DIR "/" GLADE_FILE_NAME };
      int i;
      for (i = 0; i < countof (files); i++)
        {
          struct stat st;
          if (!stat (files[i], &st))
            {
              s->glade_ui = glade_xml_new (files[i], NULL, NULL);
              break;
            }
        }
      if (!s->glade_ui)
	{
	  fprintf (stderr,
                   "%s: could not load \"" GLADE_FILE_NAME "\"\n"
                   "\tfrom " GLADE_DIR "/ or current directory.\n",
                   blurb());
          exit (-1);
	}
# undef GLADE_FILE_NAME

      glade_xml_signal_autoconnect (s->glade_ui);
    }

  w = glade_xml_get_widget (s->glade_ui, name);

#else /* !HAVE_GTK2 */

  w = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (s->base_widget),
                                         name);
  if (w) return w;
  w = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (s->popup_widget),
                                         name);
#endif /* HAVE_GTK2 */
  if (w) return w;

  fprintf (stderr, "%s: no widget \"%s\" (wrong Glade file?)\n",
           blurb(), name);
  abort();
}


/* Why this behavior isn't automatic in *either* toolkit, I'll never know.
   Takes a scroller, viewport, or list as an argument.
 */
static void
ensure_selected_item_visible (GtkWidget *widget)
{
#ifdef HAVE_GTK2
  GtkTreePath *path;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GtkTreeModel *model;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
	path = gtk_tree_path_new_first ();
  else
	path = gtk_tree_model_get_path (model, &iter);
  
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (widget), path, NULL, FALSE);

  gtk_tree_path_free (path);

#else /* !HAVE_GTK2 */

  GtkScrolledWindow *scroller = 0;
  GtkViewport *vp = 0;
  GtkList *list_widget = 0;
  GList *slist;
  GList *kids;
  int nkids = 0;
  GtkWidget *selected = 0;
  int list_elt = -1;
  GtkAdjustment *adj;
  gint parent_h, child_y, child_h, children_h, ignore;
  double ratio_t, ratio_b;

  if (GTK_IS_SCROLLED_WINDOW (widget))
    {
      scroller = GTK_SCROLLED_WINDOW (widget);
      vp = GTK_VIEWPORT (GTK_BIN (scroller)->child);
      list_widget = GTK_LIST (GTK_BIN(vp)->child);
    }
  else if (GTK_IS_VIEWPORT (widget))
    {
      vp = GTK_VIEWPORT (widget);
      scroller = GTK_SCROLLED_WINDOW (GTK_WIDGET (vp)->parent);
      list_widget = GTK_LIST (GTK_BIN(vp)->child);
    }
  else if (GTK_IS_LIST (widget))
    {
      list_widget = GTK_LIST (widget);
      vp = GTK_VIEWPORT (GTK_WIDGET (list_widget)->parent);
      scroller = GTK_SCROLLED_WINDOW (GTK_WIDGET (vp)->parent);
    }
  else
    abort();

  slist = list_widget->selection;
  selected = (slist ? GTK_WIDGET (slist->data) : 0);
  if (!selected)
    return;

  list_elt = gtk_list_child_position (list_widget, GTK_WIDGET (selected));

  for (kids = gtk_container_children (GTK_CONTAINER (list_widget));
       kids; kids = kids->next)
    nkids++;

  adj = gtk_scrolled_window_get_vadjustment (scroller);

  gdk_window_get_geometry (GTK_WIDGET(vp)->window,
                           &ignore, &ignore, &ignore, &parent_h, &ignore);
  gdk_window_get_geometry (GTK_WIDGET(selected)->window,
                           &ignore, &child_y, &ignore, &child_h, &ignore);
  children_h = nkids * child_h;

  ratio_t = ((double) child_y) / ((double) children_h);
  ratio_b = ((double) child_y + child_h) / ((double) children_h);

  if (adj->upper == 0.0)  /* no items in list */
    return;

  if (ratio_t < (adj->value / adj->upper) ||
      ratio_b > ((adj->value + adj->page_size) / adj->upper))
    {
      double target;
      int slop = parent_h * 0.75; /* how much to overshoot by */

      if (ratio_t < (adj->value / adj->upper))
        {
          double ratio_w = ((double) parent_h) / ((double) children_h);
          double ratio_l = (ratio_b - ratio_t);
          target = ((ratio_t - ratio_w + ratio_l) * adj->upper);
          target += slop;
        }
      else /* if (ratio_b > ((adj->value + adj->page_size) / adj->upper))*/
        {
          target = ratio_t * adj->upper;
          target -= slop;
        }

      if (target > adj->upper - adj->page_size)
        target = adj->upper - adj->page_size;
      if (target < 0)
        target = 0;

      gtk_adjustment_set_value (adj, target);
    }
#endif /* !HAVE_GTK2 */
}

static void
warning_dialog_dismiss_cb (GtkWidget *widget, gpointer user_data)
{
  GtkWidget *shell = GTK_WIDGET (user_data);
  while (shell->parent)
    shell = shell->parent;
  gtk_widget_destroy (GTK_WIDGET (shell));
}


void restart_menu_cb (GtkWidget *widget, gpointer user_data);

static void warning_dialog_restart_cb (GtkWidget *widget, gpointer user_data)
{
  restart_menu_cb (widget, user_data);
  warning_dialog_dismiss_cb (widget, user_data);
}

static void warning_dialog_killg_cb (GtkWidget *widget, gpointer user_data)
{
  kill_gnome_screensaver ();
  warning_dialog_dismiss_cb (widget, user_data);
}

static void warning_dialog_killk_cb (GtkWidget *widget, gpointer user_data)
{
  kill_kde_screensaver ();
  warning_dialog_dismiss_cb (widget, user_data);
}

typedef enum { D_NONE, D_LAUNCH, D_GNOME, D_KDE } dialog_button;

static void
warning_dialog (GtkWidget *parent, const char *message,
                dialog_button button_type, int center)
{
  char *msg = strdup (message);
  char *head;

  GtkWidget *dialog = gtk_dialog_new ();
  GtkWidget *label = 0;
  GtkWidget *ok = 0;
  GtkWidget *cancel = 0;
  int i = 0;

  while (parent && !parent->window)
    parent = parent->parent;

  if (!parent ||
      !GTK_WIDGET (parent)->window) /* too early to pop up transient dialogs */
    {
      fprintf (stderr, "%s: too early for dialog?\n", progname);
      return;
    }

  head = msg;
  while (head)
    {
      char name[20];
      char *s = strchr (head, '\n');
      if (s) *s = 0;

      sprintf (name, "label%d", i++);

      {
        label = gtk_label_new (head);
#ifdef HAVE_GTK2
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
#endif /* HAVE_GTK2 */

#ifndef HAVE_GTK2
        if (i == 1)
          {
            GTK_WIDGET (label)->style =
              gtk_style_copy (GTK_WIDGET (label)->style);
            GTK_WIDGET (label)->style->font =
              gdk_font_load (get_string_resource("warning_dialog.headingFont",
                                                 "Dialog.Font"));
            gtk_widget_set_style (GTK_WIDGET (label),
                                  GTK_WIDGET (label)->style);
          }
#endif /* !HAVE_GTK2 */
        if (center <= 0)
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                            label, TRUE, TRUE, 0);
        gtk_widget_show (label);
      }

      if (s)
	head = s+1;
      else
	head = 0;

      center--;
    }

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  label = gtk_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->action_area),
                      label, TRUE, TRUE, 0);

#ifdef HAVE_GTK2
  if (button_type != D_NONE)
    {
      cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
      gtk_container_add (GTK_CONTAINER (label), cancel);
    }

  ok = gtk_button_new_from_stock (GTK_STOCK_OK);
  gtk_container_add (GTK_CONTAINER (label), ok);

#else /* !HAVE_GTK2 */

  ok = gtk_button_new_with_label ("OK");
  gtk_container_add (GTK_CONTAINER (label), ok);

  if (button_type != D_NONE)
    {
      cancel = gtk_button_new_with_label ("Cancel");
      gtk_container_add (GTK_CONTAINER (label), cancel);
    }

#endif /* !HAVE_GTK2 */

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
  gtk_window_set_title (GTK_WINDOW (dialog), progclass);
  GTK_WIDGET_SET_FLAGS (ok, GTK_CAN_DEFAULT);
  gtk_widget_show (ok);
  gtk_widget_grab_focus (ok);

  if (cancel)
    {
      GTK_WIDGET_SET_FLAGS (cancel, GTK_CAN_DEFAULT); 
      gtk_widget_show (cancel);
    }
  gtk_widget_show (label);
  gtk_widget_show (dialog);

  if (button_type != D_NONE)
    {
      GtkSignalFunc fn;
      switch (button_type) {
      case D_LAUNCH: fn = GTK_SIGNAL_FUNC (warning_dialog_restart_cb); break;
      case D_GNOME:  fn = GTK_SIGNAL_FUNC (warning_dialog_killg_cb);   break;
      case D_KDE:    fn = GTK_SIGNAL_FUNC (warning_dialog_killk_cb);   break;
      default: abort(); break;
      }
      gtk_signal_connect_object (GTK_OBJECT (ok), "clicked", fn, 
                                 (gpointer) dialog);
      gtk_signal_connect_object (GTK_OBJECT (cancel), "clicked",
                                 GTK_SIGNAL_FUNC (warning_dialog_dismiss_cb),
                                 (gpointer) dialog);
    }
  else
    {
      gtk_signal_connect_object (GTK_OBJECT (ok), "clicked",
                                 GTK_SIGNAL_FUNC (warning_dialog_dismiss_cb),
                                 (gpointer) dialog);
    }

  gdk_window_set_transient_for (GTK_WIDGET (dialog)->window,
                                GTK_WIDGET (parent)->window);

#ifdef HAVE_GTK2
  gtk_window_present (GTK_WINDOW (dialog));
#else  /* !HAVE_GTK2 */
  gdk_window_show (GTK_WIDGET (dialog)->window);
  gdk_window_raise (GTK_WIDGET (dialog)->window);
#endif /* !HAVE_GTK2 */

  free (msg);
}


static void
run_cmd (state *s, Atom command, int arg)
{
  char *err = 0;
  int status;

  flush_dialog_changes_and_save (s);
  status = xscreensaver_command (GDK_DISPLAY(), command, arg, False, &err);

  /* Kludge: ignore the spurious "window unexpectedly deleted" errors... */
  if (status < 0 && err && strstr (err, "unexpectedly deleted"))
    status = 0;

  if (status < 0)
    {
      char buf [255];
      if (err)
        sprintf (buf, "Error:\n\n%s", err);
      else
        strcpy (buf, "Unknown error!");
      warning_dialog (s->toplevel_widget, buf, D_NONE, 100);
    }
  if (err) free (err);

  sensitize_menu_items (s, True);
  force_dialog_repaint (s);
}


static void
run_hack (state *s, int list_elt, Bool report_errors_p)
{
  int hack_number;
  char *err = 0;
  int status;

  if (list_elt < 0) return;
  hack_number = s->list_elt_to_hack_number[list_elt];

  flush_dialog_changes_and_save (s);
  schedule_preview (s, 0);

  status = xscreensaver_command (GDK_DISPLAY(), XA_DEMO, hack_number + 1,
                                 False, &err);

  if (status < 0 && report_errors_p)
    {
      if (xscreensaver_running_p (s))
        {
          /* Kludge: ignore the spurious "window unexpectedly deleted"
             errors... */
          if (err && strstr (err, "unexpectedly deleted"))
            status = 0;

          if (status < 0)
            {
              char buf [255];
              if (err)
                sprintf (buf, "Error:\n\n%s", err);
              else
                strcpy (buf, "Unknown error!");
              warning_dialog (s->toplevel_widget, buf, D_NONE, 100);
            }
        }
      else
        {
          /* The error is that the daemon isn't running;
             offer to restart it.
           */
          const char *d = DisplayString (GDK_DISPLAY());
          char msg [1024];
          sprintf (msg,
                   _("Warning:\n\n"
                     "The XScreenSaver daemon doesn't seem to be running\n"
                     "on display \"%s\".  Launch it now?"),
                   d);
          warning_dialog (s->toplevel_widget, msg, D_LAUNCH, 1);
        }
    }

  if (err) free (err);

  sensitize_menu_items (s, False);
}



/* Button callbacks

   According to Eric Lassauge, this G_MODULE_EXPORT crud is needed to make
   libglade work on Cygwin; apparently all Glade callbacks need this magic
   extra declaration.  I do not pretend to understand.
 */

G_MODULE_EXPORT void
exit_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  flush_dialog_changes_and_save (s);
  kill_preview_subproc (s, False);
  gtk_main_quit ();
}

static gboolean
wm_toplevel_close_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  state *s = (state *) data;
  flush_dialog_changes_and_save (s);
  gtk_main_quit ();
  return TRUE;
}


G_MODULE_EXPORT void
about_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  char msg [2048];
  char *vers = strdup (screensaver_id + 4);
  char *s;
  char copy[1024];
  char *desc = _("For updates, check http://www.jwz.org/xscreensaver/");

  s = strchr (vers, ',');
  *s = 0;
  s += 2;

  /* Ole Laursen <olau@hardworking.dk> says "don't use _() here because
     non-ASCII characters aren't allowed in localizable string keys."
     (I don't want to just use (c) instead of � because that doesn't
     look as good in the plain-old default Latin1 "C" locale.)
   */
#ifdef HAVE_GTK2
  sprintf(copy, ("Copyright \xC2\xA9 1991-2008 %s"), s);
#else  /* !HAVE_GTK2 */
  sprintf(copy, ("Copyright \251 1991-2008 %s"), s);
#endif /* !HAVE_GTK2 */

  sprintf (msg, "%s\n\n%s", copy, desc);

  /* I can't make gnome_about_new() work here -- it starts dying in
     gdk_imlib_get_visual() under gnome_about_new().  If this worked,
     then this might be the thing to do:

     #ifdef HAVE_CRAPPLET
     {
       const gchar *auth[] = { 0 };
       GtkWidget *about = gnome_about_new (progclass, vers, "", auth, desc,
                                           "xscreensaver.xpm");
       gtk_widget_show (about);
     }
     #else / * GTK but not GNOME * /
      ...
   */
  {
    GdkColormap *colormap;
    GdkPixmap *gdkpixmap;
    GdkBitmap *mask;

    GtkWidget *dialog = gtk_dialog_new ();
    GtkWidget *hbox, *icon, *vbox, *label1, *label2, *hb, *ok;
    GtkWidget *parent = GTK_WIDGET (menuitem);
    while (parent->parent)
      parent = parent->parent;

    hbox = gtk_hbox_new (FALSE, 20);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                        hbox, TRUE, TRUE, 0);

    colormap = gtk_widget_get_colormap (parent);
    gdkpixmap =
      gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mask, NULL,
                                             (gchar **) logo_180_xpm);
    icon = gtk_pixmap_new (gdkpixmap, mask);
    gtk_misc_set_padding (GTK_MISC (icon), 10, 10);

    gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

    label1 = gtk_label_new (vers);
    gtk_box_pack_start (GTK_BOX (vbox), label1, TRUE, TRUE, 0);
    gtk_label_set_justify (GTK_LABEL (label1), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label1), 0.0, 0.75);

#ifndef HAVE_GTK2
    GTK_WIDGET (label1)->style = gtk_style_copy (GTK_WIDGET (label1)->style);
    GTK_WIDGET (label1)->style->font =
      gdk_font_load (get_string_resource ("about.headingFont","Dialog.Font"));
    gtk_widget_set_style (GTK_WIDGET (label1), GTK_WIDGET (label1)->style);
#endif /* HAVE_GTK2 */

    label2 = gtk_label_new (msg);
    gtk_box_pack_start (GTK_BOX (vbox), label2, TRUE, TRUE, 0);
    gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label2), 0.0, 0.25);

#ifndef HAVE_GTK2
    GTK_WIDGET (label2)->style = gtk_style_copy (GTK_WIDGET (label2)->style);
    GTK_WIDGET (label2)->style->font =
      gdk_font_load (get_string_resource ("about.bodyFont","Dialog.Font"));
    gtk_widget_set_style (GTK_WIDGET (label2), GTK_WIDGET (label2)->style);
#endif /* HAVE_GTK2 */

    hb = gtk_hbutton_box_new ();

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->action_area),
                        hb, TRUE, TRUE, 0);

#ifdef HAVE_GTK2
    ok = gtk_button_new_from_stock (GTK_STOCK_OK);
#else /* !HAVE_GTK2 */
    ok = gtk_button_new_with_label (_("OK"));
#endif /* !HAVE_GTK2 */
    gtk_container_add (GTK_CONTAINER (hb), ok);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
    gtk_window_set_title (GTK_WINDOW (dialog), progclass);

    gtk_widget_show (hbox);
    gtk_widget_show (icon);
    gtk_widget_show (vbox);
    gtk_widget_show (label1);
    gtk_widget_show (label2);
    gtk_widget_show (hb);
    gtk_widget_show (ok);
    gtk_widget_show (dialog);

    gtk_signal_connect_object (GTK_OBJECT (ok), "clicked",
                               GTK_SIGNAL_FUNC (warning_dialog_dismiss_cb),
                               (gpointer) dialog);
    gdk_window_set_transient_for (GTK_WIDGET (dialog)->window,
                                  GTK_WIDGET (parent)->window);
    gdk_window_show (GTK_WIDGET (dialog)->window);
    gdk_window_raise (GTK_WIDGET (dialog)->window);
  }
}


G_MODULE_EXPORT void
doc_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  saver_preferences *p = &s->prefs;
  char *help_command;

  if (!p->help_url || !*p->help_url)
    {
      warning_dialog (s->toplevel_widget,
                      _("Error:\n\n"
			"No Help URL has been specified.\n"), D_NONE, 100);
      return;
    }

  help_command = (char *) malloc (strlen (p->load_url_command) +
				  (strlen (p->help_url) * 4) + 20);
  strcpy (help_command, "( ");
  sprintf (help_command + strlen(help_command),
           p->load_url_command,
           p->help_url, p->help_url, p->help_url, p->help_url);
  strcat (help_command, " ) &");
  if (system (help_command) < 0)
    fprintf (stderr, "%s: fork error\n", blurb());
  free (help_command);
}


G_MODULE_EXPORT void
file_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  sensitize_menu_items (s, False);
}


G_MODULE_EXPORT void
activate_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  run_cmd (s, XA_ACTIVATE, 0);
}


G_MODULE_EXPORT void
lock_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  run_cmd (s, XA_LOCK, 0);
}


G_MODULE_EXPORT void
kill_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  run_cmd (s, XA_EXIT, 0);
}


G_MODULE_EXPORT void
restart_menu_cb (GtkWidget *widget, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  flush_dialog_changes_and_save (s);
  xscreensaver_command (GDK_DISPLAY(), XA_EXIT, 0, False, NULL);
  sleep (1);
  if (system ("xscreensaver -nosplash &") < 0)
    fprintf (stderr, "%s: fork error\n", blurb());

  await_xscreensaver (s);
}

static Bool
xscreensaver_running_p (state *s)
{
  Display *dpy = GDK_DISPLAY();
  char *rversion = 0;
  server_xscreensaver_version (dpy, &rversion, 0, 0);
  if (!rversion)
    return False;
  free (rversion);
  return True;
}

static void
await_xscreensaver (state *s)
{
  int countdown = 5;
  Bool ok = False;

  while (!ok && (--countdown > 0))
    if (xscreensaver_running_p (s))
      ok = True;
    else
      sleep (1);    /* If it's not there yet, wait a second... */

  sensitize_menu_items (s, True);

  if (! ok)
    {
      /* Timed out, no screensaver running. */

      char buf [1024];
      Bool root_p = (geteuid () == 0);
      
      strcpy (buf, 
              _("Error:\n\n"
		"The xscreensaver daemon did not start up properly.\n"
		"\n"));

      if (root_p)

# ifdef __GNUC__
        __extension__     /* don't warn about "string length is greater than
                             the length ISO C89 compilers are required to
                             support" in the following expression... */
# endif
        strcat (buf, STFU
	  _("You are running as root.  This usually means that xscreensaver\n"
            "was unable to contact your X server because access control is\n"
            "turned on.  Try running this command:\n"
            "\n"
            "                        xhost +localhost\n"
            "\n"
            "and then selecting `File / Restart Daemon'.\n"
            "\n"
            "Note that turning off access control will allow anyone logged\n"
            "on to this machine to access your screen, which might be\n"
            "considered a security problem.  Please read the xscreensaver\n"
            "manual and FAQ for more information.\n"
            "\n"
            "You shouldn't run X as root. Instead, you should log in as a\n"
            "normal user, and `su' as necessary."));
      else
        strcat (buf, _("Please check your $PATH and permissions."));

      warning_dialog (s->toplevel_widget, buf, D_NONE, 1);
    }

  force_dialog_repaint (s);
}


static int
selected_list_element (state *s)
{
  return s->_selected_list_element;
}


static int
demo_write_init_file (state *s, saver_preferences *p)
{
  Display *dpy = GDK_DISPLAY();

#if 0
  /* #### try to figure out why shit keeps getting reordered... */
  if (strcmp (s->prefs.screenhacks[0]->name, "DNA Lounge Slideshow"))
    abort();
#endif

  if (!write_init_file (dpy, p, s->short_version, False))
    {
      if (s->debug_p)
        fprintf (stderr, "%s: wrote %s\n", blurb(), init_file_name());
      return 0;
    }
  else
    {
      const char *f = init_file_name();
      if (!f || !*f)
        warning_dialog (s->toplevel_widget,
                        _("Error:\n\nCouldn't determine init file name!\n"),
                        D_NONE, 100);
      else
        {
          char *b = (char *) malloc (strlen(f) + 1024);
          sprintf (b, _("Error:\n\nCouldn't write %s\n"), f);
          warning_dialog (s->toplevel_widget, b, D_NONE, 100);
          free (b);
        }
      return -1;
    }
}


G_MODULE_EXPORT void
run_this_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  int list_elt = selected_list_element (s);
  if (list_elt < 0) return;
  if (!flush_dialog_changes_and_save (s))
    run_hack (s, list_elt, True);
}


G_MODULE_EXPORT void
manual_cb (GtkButton *button, gpointer user_data)
{
  Display *dpy = GDK_DISPLAY();
  state *s = global_state_kludge;  /* I hate C so much... */
  saver_preferences *p = &s->prefs;
  GtkWidget *list_widget = name_to_widget (s, "list");
  int list_elt = selected_list_element (s);
  int hack_number;
  char *name, *name2, *cmd, *str;
  char *oname = 0;
  if (list_elt < 0) return;
  hack_number = s->list_elt_to_hack_number[list_elt];

  flush_dialog_changes_and_save (s);
  ensure_selected_item_visible (list_widget);

  name = strdup (p->screenhacks[hack_number]->command);
  name2 = name;
  oname = name;
  while (isspace (*name2)) name2++;
  str = name2;
  while (*str && !isspace (*str)) str++;
  *str = 0;
  str = strrchr (name2, '/');
  if (str) name2 = str+1;

  cmd = get_string_resource (dpy, "manualCommand", "ManualCommand");
  if (cmd)
    {
      char *cmd2 = (char *) malloc (strlen (cmd) + (strlen (name2) * 4) + 100);
      strcpy (cmd2, "( ");
      sprintf (cmd2 + strlen (cmd2),
               cmd,
               name2, name2, name2, name2);
      strcat (cmd2, " ) &");
      if (system (cmd2) < 0)
        fprintf (stderr, "%s: fork error\n", blurb());
      free (cmd2);
    }
  else
    {
      warning_dialog (GTK_WIDGET (button),
                      _("Error:\n\nno `manualCommand' resource set."),
                      D_NONE, 100);
    }

  free (oname);
}


static void
force_list_select_item (state *s, GtkWidget *list, int list_elt, Bool scroll_p)
{
  GtkWidget *parent = name_to_widget (s, "scroller");
  Bool was = GTK_WIDGET_IS_SENSITIVE (parent);
#ifdef HAVE_GTK2
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
#endif /* HAVE_GTK2 */

  if (!was) gtk_widget_set_sensitive (parent, True);
#ifdef HAVE_GTK2
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));
  g_assert (model);
  if (gtk_tree_model_iter_nth_child (model, &iter, NULL, list_elt))
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
      gtk_tree_selection_select_iter (selection, &iter);
    }
#else  /* !HAVE_GTK2 */
  gtk_list_select_item (GTK_LIST (list), list_elt);
#endif /* !HAVE_GTK2 */
  if (scroll_p) ensure_selected_item_visible (GTK_WIDGET (list));
  if (!was) gtk_widget_set_sensitive (parent, False);
}


G_MODULE_EXPORT void
run_next_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  /* saver_preferences *p = &s->prefs; */
  Bool ops = s->preview_suppressed_p;

  GtkWidget *list_widget = name_to_widget (s, "list");
  int list_elt = selected_list_element (s);

  if (list_elt < 0)
    list_elt = 0;
  else
    list_elt++;

  if (list_elt >= s->list_count)
    list_elt = 0;

  s->preview_suppressed_p = True;

  flush_dialog_changes_and_save (s);
  force_list_select_item (s, list_widget, list_elt, True);
  populate_demo_window (s, list_elt);
  run_hack (s, list_elt, False);

  s->preview_suppressed_p = ops;
}


G_MODULE_EXPORT void
run_prev_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  /* saver_preferences *p = &s->prefs; */
  Bool ops = s->preview_suppressed_p;

  GtkWidget *list_widget = name_to_widget (s, "list");
  int list_elt = selected_list_element (s);

  if (list_elt < 0)
    list_elt = s->list_count - 1;
  else
    list_elt--;

  if (list_elt < 0)
    list_elt = s->list_count - 1;

  s->preview_suppressed_p = True;

  flush_dialog_changes_and_save (s);
  force_list_select_item (s, list_widget, list_elt, True);
  populate_demo_window (s, list_elt);
  run_hack (s, list_elt, False);

  s->preview_suppressed_p = ops;
}


/* Writes the given settings into prefs.
   Returns true if there was a change, False otherwise.
   command and/or visual may be 0, or enabled_p may be -1, meaning "no change".
 */
static Bool
flush_changes (state *s,
               int list_elt,
               int enabled_p,
               const char *command,
               const char *visual)
{
  saver_preferences *p = &s->prefs;
  Bool changed = False;
  screenhack *hack;
  int hack_number;
  if (list_elt < 0 || list_elt >= s->list_count)
    abort();

  hack_number = s->list_elt_to_hack_number[list_elt];
  hack = p->screenhacks[hack_number];

  if (enabled_p != -1 &&
      enabled_p != hack->enabled_p)
    {
      hack->enabled_p = enabled_p;
      changed = True;
      if (s->debug_p)
        fprintf (stderr, "%s: \"%s\": enabled => %d\n",
                 blurb(), hack->name, enabled_p);
    }

  if (command)
    {
      if (!hack->command || !!strcmp (command, hack->command))
        {
          if (hack->command) free (hack->command);
          hack->command = strdup (command);
          changed = True;
          if (s->debug_p)
            fprintf (stderr, "%s: \"%s\": command => \"%s\"\n",
                     blurb(), hack->name, command);
        }
    }

  if (visual)
    {
      const char *ov = hack->visual;
      if (!ov || !*ov) ov = "any";
      if (!*visual) visual = "any";
      if (!!strcasecmp (visual, ov))
        {
          if (hack->visual) free (hack->visual);
          hack->visual = strdup (visual);
          changed = True;
          if (s->debug_p)
            fprintf (stderr, "%s: \"%s\": visual => \"%s\"\n",
                     blurb(), hack->name, visual);
        }
    }

  return changed;
}


/* Helper for the text fields that contain time specifications:
   this parses the text, and does error checking.
 */
static void 
hack_time_text (state *s, const char *line, Time *store, Bool sec_p)
{
  if (*line)
    {
      int value;
      if (!sec_p || strchr (line, ':'))
        value = parse_time ((char *) line, sec_p, True);
      else
        {
          char c;
          if (sscanf (line, "%d%c", &value, &c) != 1)
            value = -1;
          if (!sec_p)
            value *= 60;
        }

      value *= 1000;	/* Time measures in microseconds */
      if (value < 0)
	{
	  char b[255];
	  sprintf (b,
		   _("Error:\n\n"
		     "Unparsable time format: \"%s\"\n"),
		   line);
	  warning_dialog (s->toplevel_widget, b, D_NONE, 100);
	}
      else
	*store = value;
    }
}


static Bool
directory_p (const char *path)
{
  struct stat st;
  if (!path || !*path)
    return False;
  else if (stat (path, &st))
    return False;
  else if (!S_ISDIR (st.st_mode))
    return False;
  else
    return True;
}

static Bool
file_p (const char *path)
{
  struct stat st;
  if (!path || !*path)
    return False;
  else if (stat (path, &st))
    return False;
  else if (S_ISDIR (st.st_mode))
    return False;
  else
    return True;
}

static char *
normalize_directory (const char *path)
{
  int L;
  char *p2, *s;
  if (!path || !*path) return 0;
  L = strlen (path);
  p2 = (char *) malloc (L + 2);
  strcpy (p2, path);
  if (p2[L-1] == '/')  /* remove trailing slash */
    p2[--L] = 0;

  for (s = p2; s && *s; s++)
    {
      if (*s == '/' &&
          (!strncmp (s, "/../", 4) ||			/* delete "XYZ/../" */
           !strncmp (s, "/..\000", 4)))			/* delete "XYZ/..$" */
        {
          char *s0 = s;
          while (s0 > p2 && s0[-1] != '/')
            s0--;
          if (s0 > p2)
            {
              s0--;
              s += 3;
              strcpy (s0, s);
              s = s0-1;
            }
        }
      else if (*s == '/' && !strncmp (s, "/./", 3))	/* delete "/./" */
        strcpy (s, s+2), s--;
      else if (*s == '/' && !strncmp (s, "/.\000", 3))	/* delete "/.$" */
        *s = 0, s--;
    }

  for (s = p2; s && *s; s++)		/* normalize consecutive slashes */
    while (s[0] == '/' && s[1] == '/')
      strcpy (s, s+1);

  /* and strip trailing whitespace for good measure. */
  L = strlen(p2);
  while (isspace(p2[L-1]))
    p2[--L] = 0;

  return p2;
}


#ifdef HAVE_GTK2

typedef struct {
  state *s;
  int i;
  Bool *changed;
} FlushForeachClosure;

static gboolean
flush_checkbox  (GtkTreeModel *model,
		 GtkTreePath *path,
		 GtkTreeIter *iter,
		 gpointer data)
{
  FlushForeachClosure *closure = data;
  gboolean checked;

  gtk_tree_model_get (model, iter,
		      COL_ENABLED, &checked,
		      -1);

  if (flush_changes (closure->s, closure->i,
		     checked, 0, 0))
    *closure->changed = True;
  
  closure->i++;

  /* don't remove row */
  return FALSE;
}

#endif /* HAVE_GTK2 */

/* Flush out any changes made in the main dialog window (where changes
   take place immediately: clicking on a checkbox causes the init file
   to be written right away.)
 */
static Bool
flush_dialog_changes_and_save (state *s)
{
  saver_preferences *p = &s->prefs;
  saver_preferences P2, *p2 = &P2;
#ifdef HAVE_GTK2
  GtkTreeView *list_widget = GTK_TREE_VIEW (name_to_widget (s, "list"));
  GtkTreeModel *model = gtk_tree_view_get_model (list_widget);
  FlushForeachClosure closure;
#else /* !HAVE_GTK2 */
  GtkList *list_widget = GTK_LIST (name_to_widget (s, "list"));
  GList *kids = gtk_container_children (GTK_CONTAINER (list_widget));
  int i;
#endif /* !HAVE_GTK2 */

  Bool changed = False;
  GtkWidget *w;

  if (s->saving_p) return False;
  s->saving_p = True;

  *p2 = *p;

  /* Flush any checkbox changes in the list down into the prefs struct.
   */
#ifdef HAVE_GTK2
  closure.s = s;
  closure.changed = &changed;
  closure.i = 0;
  gtk_tree_model_foreach (model, flush_checkbox, &closure);

#else /* !HAVE_GTK2 */

  for (i = 0; kids; kids = kids->next, i++)
    {
      GtkWidget *line = GTK_WIDGET (kids->data);
      GtkWidget *line_hbox = GTK_WIDGET (GTK_BIN (line)->child);
      GtkWidget *line_check =
        GTK_WIDGET (gtk_container_children (GTK_CONTAINER (line_hbox))->data);
      Bool checked =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (line_check));

      if (flush_changes (s, i, (checked ? 1 : 0), 0, 0))
        changed = True;
    }
#endif /* ~HAVE_GTK2 */

  /* Flush the non-hack-specific settings down into the prefs struct.
   */

# define SECONDS(FIELD,NAME) \
    w = name_to_widget (s, (NAME)); \
    hack_time_text (s, gtk_entry_get_text (GTK_ENTRY (w)), (FIELD), True)

# define MINUTES(FIELD,NAME) \
    w = name_to_widget (s, (NAME)); \
    hack_time_text (s, gtk_entry_get_text (GTK_ENTRY (w)), (FIELD), False)

# define CHECKBOX(FIELD,NAME) \
    w = name_to_widget (s, (NAME)); \
    (FIELD) = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))

# define PATHNAME(FIELD,NAME) \
    w = name_to_widget (s, (NAME)); \
    (FIELD) = normalize_directory (gtk_entry_get_text (GTK_ENTRY (w)))

# define TEXT(FIELD,NAME) \
    w = name_to_widget (s, (NAME)); \
    (FIELD) = (char *) gtk_entry_get_text (GTK_ENTRY (w))

  MINUTES  (&p2->timeout,         "timeout_spinbutton");
  MINUTES  (&p2->cycle,           "cycle_spinbutton");
  CHECKBOX (p2->lock_p,           "lock_button");
  MINUTES  (&p2->lock_timeout,    "lock_spinbutton");

  CHECKBOX (p2->dpms_enabled_p,  "dpms_button");
  MINUTES  (&p2->dpms_standby,    "dpms_standby_spinbutton");
  MINUTES  (&p2->dpms_suspend,    "dpms_suspend_spinbutton");
  MINUTES  (&p2->dpms_off,        "dpms_off_spinbutton");

  CHECKBOX (p2->grab_desktop_p,   "grab_desk_button");
  CHECKBOX (p2->grab_video_p,     "grab_video_button");
  CHECKBOX (p2->random_image_p,   "grab_image_button");
  PATHNAME (p2->image_directory,  "image_text");

#if 0
  CHECKBOX (p2->verbose_p,        "verbose_button");
  CHECKBOX (p2->capture_stderr_p, "capture_button");
  CHECKBOX (p2->splash_p,         "splash_button");
#endif

  {
    Bool v = False;
    CHECKBOX (v, "text_host_radio");     if (v) p2->tmode = TEXT_DATE;
    CHECKBOX (v, "text_radio");          if (v) p2->tmode = TEXT_LITERAL;
    CHECKBOX (v, "text_file_radio");     if (v) p2->tmode = TEXT_FILE;
    CHECKBOX (v, "text_program_radio");  if (v) p2->tmode = TEXT_PROGRAM;
    CHECKBOX (v, "text_url_radio");      if (v) p2->tmode = TEXT_URL;
    TEXT     (p2->text_literal, "text_entry");
    PATHNAME (p2->text_file,    "text_file_entry");
    PATHNAME (p2->text_program, "text_program_entry");
    PATHNAME (p2->text_program, "text_program_entry");
    TEXT     (p2->text_url,     "text_url_entry");
  }

  CHECKBOX (p2->install_cmap_p,   "install_button");
  CHECKBOX (p2->fade_p,           "fade_button");
  CHECKBOX (p2->unfade_p,         "unfade_button");
  SECONDS  (&p2->fade_seconds,    "fade_spinbutton");

# undef SECONDS
# undef MINUTES
# undef CHECKBOX
# undef PATHNAME
# undef TEXT

  /* Warn if the image directory doesn't exist.
   */
  if (p2->image_directory &&
      *p2->image_directory &&
      !directory_p (p2->image_directory))
    {
      char b[255];
      sprintf (b, "Error:\n\n" "Directory does not exist: \"%s\"\n",
               p2->image_directory);
      warning_dialog (s->toplevel_widget, b, D_NONE, 100);
    }


  /* Map the mode menu to `saver_mode' enum values. */
  {
    GtkOptionMenu *opt = GTK_OPTION_MENU (name_to_widget (s, "mode_menu"));
    GtkMenu *menu = GTK_MENU (gtk_option_menu_get_menu (opt));
    GtkWidget *selected = gtk_menu_get_active (menu);
    GList *kids = gtk_container_children (GTK_CONTAINER (menu));
    int menu_elt = g_list_index (kids, (gpointer) selected);
    if (menu_elt < 0 || menu_elt >= countof(mode_menu_order)) abort();
    p2->mode = mode_menu_order[menu_elt];
  }

  if (p2->mode == ONE_HACK)
    {
      int list_elt = selected_list_element (s);
      p2->selected_hack = (list_elt >= 0
                           ? s->list_elt_to_hack_number[list_elt]
                           : -1);
    }

# define COPY(field, name) \
  if (p->field != p2->field) { \
    changed = True; \
    if (s->debug_p) \
      fprintf (stderr, "%s: %s => %d\n", blurb(), name, (int) p2->field); \
  } \
  p->field = p2->field

  COPY(mode,             "mode");
  COPY(selected_hack,    "selected_hack");

  COPY(timeout,        "timeout");
  COPY(cycle,          "cycle");
  COPY(lock_p,         "lock_p");
  COPY(lock_timeout,   "lock_timeout");

  COPY(dpms_enabled_p, "dpms_enabled_p");
  COPY(dpms_standby,   "dpms_standby");
  COPY(dpms_suspend,   "dpms_suspend");
  COPY(dpms_off,       "dpms_off");

#if 0
  COPY(verbose_p,        "verbose_p");
  COPY(capture_stderr_p, "capture_stderr_p");
  COPY(splash_p,         "splash_p");
#endif

  COPY(tmode,            "tmode");

  COPY(install_cmap_p,   "install_cmap_p");
  COPY(fade_p,           "fade_p");
  COPY(unfade_p,         "unfade_p");
  COPY(fade_seconds,     "fade_seconds");

  COPY(grab_desktop_p, "grab_desktop_p");
  COPY(grab_video_p,   "grab_video_p");
  COPY(random_image_p, "random_image_p");

# undef COPY

# define COPYSTR(FIELD,NAME) \
  if (!p->FIELD || \
      !p2->FIELD || \
      strcmp(p->FIELD, p2->FIELD)) \
    { \
      changed = True; \
      if (s->debug_p) \
        fprintf (stderr, "%s: %s => \"%s\"\n", blurb(), NAME, p2->FIELD); \
    } \
  if (p->FIELD && p->FIELD != p2->FIELD) \
    free (p->FIELD); \
  p->FIELD = p2->FIELD; \
  p2->FIELD = 0

  COPYSTR(image_directory, "image_directory");
  COPYSTR(text_literal,    "text_literal");
  COPYSTR(text_file,       "text_file");
  COPYSTR(text_program,    "text_program");
  COPYSTR(text_url,        "text_url");
# undef COPYSTR

  populate_prefs_page (s);

  if (changed)
    {
      Display *dpy = GDK_DISPLAY();
      Bool enabled_p = (p->dpms_enabled_p && p->mode != DONT_BLANK);
      sync_server_dpms_settings (dpy, enabled_p,
                                 p->dpms_standby / 1000,
                                 p->dpms_suspend / 1000,
                                 p->dpms_off / 1000,
                                 False);

      changed = demo_write_init_file (s, p);
    }

  s->saving_p = False;
  return changed;
}


/* Flush out any changes made in the popup dialog box (where changes
   take place only when the OK button is clicked.)
 */
static Bool
flush_popup_changes_and_save (state *s)
{
  Bool changed = False;
  saver_preferences *p = &s->prefs;
  int list_elt = selected_list_element (s);

  GtkEntry *cmd = GTK_ENTRY (name_to_widget (s, "cmd_text"));
  GtkCombo *vis = GTK_COMBO (name_to_widget (s, "visual_combo"));

  const char *visual = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (vis)->entry));
  const char *command = gtk_entry_get_text (cmd);

  char c;
  unsigned long id;

  if (s->saving_p) return False;
  s->saving_p = True;

  if (list_elt < 0)
    goto DONE;

  if (maybe_reload_init_file (s) != 0)
    {
      changed = True;
      goto DONE;
    }

  /* Sanity-check and canonicalize whatever the user typed into the combo box.
   */
  if      (!strcasecmp (visual, ""))                   visual = "";
  else if (!strcasecmp (visual, "any"))                visual = "";
  else if (!strcasecmp (visual, "default"))            visual = "Default";
  else if (!strcasecmp (visual, "default-n"))          visual = "Default-N";
  else if (!strcasecmp (visual, "default-i"))          visual = "Default-I";
  else if (!strcasecmp (visual, "best"))               visual = "Best";
  else if (!strcasecmp (visual, "mono"))               visual = "Mono";
  else if (!strcasecmp (visual, "monochrome"))         visual = "Mono";
  else if (!strcasecmp (visual, "gray"))               visual = "Gray";
  else if (!strcasecmp (visual, "grey"))               visual = "Gray";
  else if (!strcasecmp (visual, "color"))              visual = "Color";
  else if (!strcasecmp (visual, "gl"))                 visual = "GL";
  else if (!strcasecmp (visual, "staticgray"))         visual = "StaticGray";
  else if (!strcasecmp (visual, "staticcolor"))        visual = "StaticColor";
  else if (!strcasecmp (visual, "truecolor"))          visual = "TrueColor";
  else if (!strcasecmp (visual, "grayscale"))          visual = "GrayScale";
  else if (!strcasecmp (visual, "greyscale"))          visual = "GrayScale";
  else if (!strcasecmp (visual, "pseudocolor"))        visual = "PseudoColor";
  else if (!strcasecmp (visual, "directcolor"))        visual = "DirectColor";
  else if (1 == sscanf (visual, " %lu %c", &id, &c))   ;
  else if (1 == sscanf (visual, " 0x%lx %c", &id, &c)) ;
  else
    {
      gdk_beep ();				  /* unparsable */
      visual = "";
      gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (vis)->entry), _("Any"));
    }

  changed = flush_changes (s, list_elt, -1, command, visual);
  if (changed)
    {
      changed = demo_write_init_file (s, p);

      /* Do this to re-launch the hack if (and only if) the command line
         has changed. */
      populate_demo_window (s, selected_list_element (s));
    }

 DONE:
  s->saving_p = False;
  return changed;
}


G_MODULE_EXPORT void
pref_changed_cb (GtkWidget *widget, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  if (! s->initializing_p)
    {
      s->initializing_p = True;
      flush_dialog_changes_and_save (s);
      s->initializing_p = False;
    }
}

G_MODULE_EXPORT gboolean
pref_changed_event_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  pref_changed_cb (widget, user_data);
  return FALSE;
}

/* Callback on menu items in the "mode" options menu.
 */
G_MODULE_EXPORT void
mode_menu_item_cb (GtkWidget *widget, gpointer user_data)
{
  state *s = (state *) user_data;
  saver_preferences *p = &s->prefs;
  GtkWidget *list = name_to_widget (s, "list");
  int list_elt;

  GList *menu_items = gtk_container_children (GTK_CONTAINER (widget->parent));
  int menu_index = 0;
  saver_mode new_mode;

  while (menu_items)
    {
      if (menu_items->data == widget)
        break;
      menu_index++;
      menu_items = menu_items->next;
    }
  if (!menu_items) abort();

  new_mode = mode_menu_order[menu_index];

  /* Keep the same list element displayed as before; except if we're
     switching *to* "one screensaver" mode from any other mode, set
     "the one" to be that which is currently selected.
   */
  list_elt = selected_list_element (s);
  if (new_mode == ONE_HACK)
    p->selected_hack = s->list_elt_to_hack_number[list_elt];

  {
    saver_mode old_mode = p->mode;
    p->mode = new_mode;
    populate_demo_window (s, list_elt);
    force_list_select_item (s, list, list_elt, True);
    p->mode = old_mode;  /* put it back, so the init file gets written */
  }

  pref_changed_cb (widget, user_data);
}


G_MODULE_EXPORT void
switch_page_cb (GtkNotebook *notebook, GtkNotebookPage *page,
                gint page_num, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  pref_changed_cb (GTK_WIDGET (notebook), user_data);

  /* If we're switching to page 0, schedule the current hack to be run.
     Otherwise, schedule it to stop. */
  if (page_num == 0)
    populate_demo_window (s, selected_list_element (s));
  else
    schedule_preview (s, 0);
}

#ifdef HAVE_GTK2
static void
list_activated_cb (GtkTreeView       *list,
		   GtkTreePath       *path,
		   GtkTreeViewColumn *column,
		   gpointer           data)
{
  state *s = data;
  char *str;
  int list_elt;

  g_return_if_fail (!gdk_pointer_is_grabbed ());

  str = gtk_tree_path_to_string (path);
  list_elt = strtol (str, NULL, 10);
  g_free (str);

  if (list_elt >= 0)
    run_hack (s, list_elt, True);
}

static void
list_select_changed_cb (GtkTreeSelection *selection, gpointer data)
{
  state *s = (state *)data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;
  char *str;
  int list_elt;
 
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  path = gtk_tree_model_get_path (model, &iter);
  str = gtk_tree_path_to_string (path);
  list_elt = strtol (str, NULL, 10);

  gtk_tree_path_free (path);
  g_free (str);

  populate_demo_window (s, list_elt);
  flush_dialog_changes_and_save (s);

  /* Re-populate the Settings window any time a new item is selected
     in the list, in case both windows are currently visible.
   */
  populate_popup_window (s);
}

#else /* !HAVE_GTK2 */

static time_t last_doubleclick_time = 0;   /* FMH!  This is to suppress the
                                              list_select_cb that comes in
                                              *after* we've double-clicked.
                                            */

static gint
list_doubleclick_cb (GtkWidget *button, GdkEventButton *event,
                     gpointer data)
{
  state *s = (state *) data;
  if (event->type == GDK_2BUTTON_PRESS)
    {
      GtkList *list = GTK_LIST (name_to_widget (s, "list"));
      int list_elt = gtk_list_child_position (list, GTK_WIDGET (button));

      last_doubleclick_time = time ((time_t *) 0);

      if (list_elt >= 0)
        run_hack (s, list_elt, True);
    }

  return FALSE;
}


static void
list_select_cb (GtkList *list, GtkWidget *child, gpointer data)
{
  state *s = (state *) data;
  time_t now = time ((time_t *) 0);

  if (now >= last_doubleclick_time + 2)
    {
      int list_elt = gtk_list_child_position (list, GTK_WIDGET (child));
      populate_demo_window (s, list_elt);
      flush_dialog_changes_and_save (s);
    }
}

static void
list_unselect_cb (GtkList *list, GtkWidget *child, gpointer data)
{
  state *s = (state *) data;
  populate_demo_window (s, -1);
  flush_dialog_changes_and_save (s);
}

#endif /* !HAVE_GTK2 */


/* Called when the checkboxes that are in the left column of the
   scrolling list are clicked.  This both populates the right pane
   (just as clicking on the label (really, listitem) does) and
   also syncs this checkbox with  the right pane Enabled checkbox.
 */
static void
list_checkbox_cb (
#ifdef HAVE_GTK2
		  GtkCellRendererToggle *toggle,
		  gchar                 *path_string,
#else  /* !HAVE_GTK2 */
		  GtkWidget *cb,
#endif /* !HAVE_GTK2 */
		  gpointer               data)
{
  state *s = (state *) data;

#ifdef HAVE_GTK2
  GtkScrolledWindow *scroller =
    GTK_SCROLLED_WINDOW (name_to_widget (s, "scroller"));
  GtkTreeView *list = GTK_TREE_VIEW (name_to_widget (s, "list"));
  GtkTreeModel *model = gtk_tree_view_get_model (list);
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  gboolean active;
#else /* !HAVE_GTK2 */
  GtkWidget *line_hbox = GTK_WIDGET (cb)->parent;
  GtkWidget *line = GTK_WIDGET (line_hbox)->parent;

  GtkList *list = GTK_LIST (GTK_WIDGET (line)->parent);
  GtkViewport *vp = GTK_VIEWPORT (GTK_WIDGET (list)->parent);
  GtkScrolledWindow *scroller = GTK_SCROLLED_WINDOW (GTK_WIDGET (vp)->parent);
#endif /* !HAVE_GTK2 */
  GtkAdjustment *adj;
  double scroll_top;

  int list_elt;

#ifdef HAVE_GTK2
  if (!gtk_tree_model_get_iter (model, &iter, path))
    {
      g_warning ("bad path: %s", path_string);
      return;
    }
  gtk_tree_path_free (path);

  gtk_tree_model_get (model, &iter,
		      COL_ENABLED, &active,
		      -1);

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      COL_ENABLED, !active,
		      -1);

  list_elt = strtol (path_string, NULL, 10);  
#else  /* !HAVE_GTK2 */
  list_elt = gtk_list_child_position (list, line);
#endif /* !HAVE_GTK2 */

  /* remember previous scroll position of the top of the list */
  adj = gtk_scrolled_window_get_vadjustment (scroller);
  scroll_top = adj->value;

  flush_dialog_changes_and_save (s);
  force_list_select_item (s, GTK_WIDGET (list), list_elt, False);
  populate_demo_window (s, list_elt);
  
  /* restore the previous scroll position of the top of the list.
     this is weak, but I don't really know why it's moving... */
  gtk_adjustment_set_value (adj, scroll_top);
}


typedef struct {
  state *state;
  GtkFileSelection *widget;
} file_selection_data;



static void
store_image_directory (GtkWidget *button, gpointer user_data)
{
  file_selection_data *fsd = (file_selection_data *) user_data;
  state *s = fsd->state;
  GtkFileSelection *selector = fsd->widget;
  GtkWidget *top = s->toplevel_widget;
  saver_preferences *p = &s->prefs;
  const char *path = gtk_file_selection_get_filename (selector);

  if (p->image_directory && !strcmp(p->image_directory, path))
    return;  /* no change */

  if (!directory_p (path))
    {
      char b[255];
      sprintf (b, _("Error:\n\n" "Directory does not exist: \"%s\"\n"), path);
      warning_dialog (GTK_WIDGET (top), b, D_NONE, 100);
      return;
    }

  if (p->image_directory) free (p->image_directory);
  p->image_directory = normalize_directory (path);

  gtk_entry_set_text (GTK_ENTRY (name_to_widget (s, "image_text")),
                      (p->image_directory ? p->image_directory : ""));
  demo_write_init_file (s, p);
}


static void
store_text_file (GtkWidget *button, gpointer user_data)
{
  file_selection_data *fsd = (file_selection_data *) user_data;
  state *s = fsd->state;
  GtkFileSelection *selector = fsd->widget;
  GtkWidget *top = s->toplevel_widget;
  saver_preferences *p = &s->prefs;
  const char *path = gtk_file_selection_get_filename (selector);

  if (p->text_file && !strcmp(p->text_file, path))
    return;  /* no change */

  if (!file_p (path))
    {
      char b[255];
      sprintf (b, _("Error:\n\n" "File does not exist: \"%s\"\n"), path);
      warning_dialog (GTK_WIDGET (top), b, D_NONE, 100);
      return;
    }

  if (p->text_file) free (p->text_file);
  p->text_file = normalize_directory (path);

  gtk_entry_set_text (GTK_ENTRY (name_to_widget (s, "text_file_entry")),
                      (p->text_file ? p->text_file : ""));
  demo_write_init_file (s, p);
}


static void
store_text_program (GtkWidget *button, gpointer user_data)
{
  file_selection_data *fsd = (file_selection_data *) user_data;
  state *s = fsd->state;
  GtkFileSelection *selector = fsd->widget;
  /*GtkWidget *top = s->toplevel_widget;*/
  saver_preferences *p = &s->prefs;
  const char *path = gtk_file_selection_get_filename (selector);

  if (p->text_program && !strcmp(p->text_program, path))
    return;  /* no change */

# if 0
  if (!file_p (path))
    {
      char b[255];
      sprintf (b, _("Error:\n\n" "File does not exist: \"%s\"\n"), path);
      warning_dialog (GTK_WIDGET (top), b, D_NONE, 100);
      return;
    }
# endif

  if (p->text_program) free (p->text_program);
  p->text_program = normalize_directory (path);

  gtk_entry_set_text (GTK_ENTRY (name_to_widget (s, "text_program_entry")),
                      (p->text_program ? p->text_program : ""));
  demo_write_init_file (s, p);
}



static void
browse_image_dir_cancel (GtkWidget *button, gpointer user_data)
{
  file_selection_data *fsd = (file_selection_data *) user_data;
  gtk_widget_hide (GTK_WIDGET (fsd->widget));
}

static void
browse_image_dir_ok (GtkWidget *button, gpointer user_data)
{
  browse_image_dir_cancel (button, user_data);
  store_image_directory (button, user_data);
}

static void
browse_text_file_ok (GtkWidget *button, gpointer user_data)
{
  browse_image_dir_cancel (button, user_data);
  store_text_file (button, user_data);
}

static void
browse_text_program_ok (GtkWidget *button, gpointer user_data)
{
  browse_image_dir_cancel (button, user_data);
  store_text_program (button, user_data);
}

static void
browse_image_dir_close (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  browse_image_dir_cancel (widget, user_data);
}


G_MODULE_EXPORT void
browse_image_dir_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  saver_preferences *p = &s->prefs;
  static file_selection_data *fsd = 0;

  GtkFileSelection *selector = GTK_FILE_SELECTION(
    gtk_file_selection_new ("Please select the image directory."));

  if (!fsd)
    fsd = (file_selection_data *) malloc (sizeof (*fsd));  

  fsd->widget = selector;
  fsd->state = s;

  if (p->image_directory && *p->image_directory)
    gtk_file_selection_set_filename (selector, p->image_directory);

  gtk_signal_connect (GTK_OBJECT (selector->ok_button),
                      "clicked", GTK_SIGNAL_FUNC (browse_image_dir_ok),
                      (gpointer *) fsd);
  gtk_signal_connect (GTK_OBJECT (selector->cancel_button),
                      "clicked", GTK_SIGNAL_FUNC (browse_image_dir_cancel),
                      (gpointer *) fsd);
  gtk_signal_connect (GTK_OBJECT (selector), "delete_event",
                      GTK_SIGNAL_FUNC (browse_image_dir_close),
                      (gpointer *) fsd);

  gtk_widget_set_sensitive (GTK_WIDGET (selector->file_list), False);

  gtk_window_set_modal (GTK_WINDOW (selector), True);
  gtk_widget_show (GTK_WIDGET (selector));
}


G_MODULE_EXPORT void
browse_text_file_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  saver_preferences *p = &s->prefs;
  static file_selection_data *fsd = 0;

  GtkFileSelection *selector = GTK_FILE_SELECTION(
    gtk_file_selection_new ("Please select a text file."));

  if (!fsd)
    fsd = (file_selection_data *) malloc (sizeof (*fsd));  

  fsd->widget = selector;
  fsd->state = s;

  if (p->text_file && *p->text_file)
    gtk_file_selection_set_filename (selector, p->text_file);

  gtk_signal_connect (GTK_OBJECT (selector->ok_button),
                      "clicked", GTK_SIGNAL_FUNC (browse_text_file_ok),
                      (gpointer *) fsd);
  gtk_signal_connect (GTK_OBJECT (selector->cancel_button),
                      "clicked", GTK_SIGNAL_FUNC (browse_image_dir_cancel),
                      (gpointer *) fsd);
  gtk_signal_connect (GTK_OBJECT (selector), "delete_event",
                      GTK_SIGNAL_FUNC (browse_image_dir_close),
                      (gpointer *) fsd);

  gtk_window_set_modal (GTK_WINDOW (selector), True);
  gtk_widget_show (GTK_WIDGET (selector));
}


G_MODULE_EXPORT void
browse_text_program_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  saver_preferences *p = &s->prefs;
  static file_selection_data *fsd = 0;

  GtkFileSelection *selector = GTK_FILE_SELECTION(
    gtk_file_selection_new ("Please select a text-generating program."));

  if (!fsd)
    fsd = (file_selection_data *) malloc (sizeof (*fsd));  

  fsd->widget = selector;
  fsd->state = s;

  if (p->text_program && *p->text_program)
    gtk_file_selection_set_filename (selector, p->text_program);

  gtk_signal_connect (GTK_OBJECT (selector->ok_button),
                      "clicked", GTK_SIGNAL_FUNC (browse_text_program_ok),
                      (gpointer *) fsd);
  gtk_signal_connect (GTK_OBJECT (selector->cancel_button),
                      "clicked", GTK_SIGNAL_FUNC (browse_image_dir_cancel),
                      (gpointer *) fsd);
  gtk_signal_connect (GTK_OBJECT (selector), "delete_event",
                      GTK_SIGNAL_FUNC (browse_image_dir_close),
                      (gpointer *) fsd);

  gtk_window_set_modal (GTK_WINDOW (selector), True);
  gtk_widget_show (GTK_WIDGET (selector));
}





G_MODULE_EXPORT void
settings_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  int list_elt = selected_list_element (s);

  populate_demo_window (s, list_elt);   /* reset the widget */
  populate_popup_window (s);		/* create UI on popup window */
  gtk_widget_show (s->popup_widget);
}

static void
settings_sync_cmd_text (state *s)
{
# ifdef HAVE_XML
  GtkWidget *cmd = GTK_WIDGET (name_to_widget (s, "cmd_text"));
  char *cmd_line = get_configurator_command_line (s->cdata);
  gtk_entry_set_text (GTK_ENTRY (cmd), cmd_line);
  gtk_entry_set_position (GTK_ENTRY (cmd), strlen (cmd_line));
  free (cmd_line);
# endif /* HAVE_XML */
}

G_MODULE_EXPORT void
settings_adv_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  GtkNotebook *notebook =
    GTK_NOTEBOOK (name_to_widget (s, "opt_notebook"));

  settings_sync_cmd_text (s);
  gtk_notebook_set_page (notebook, 1);
}

G_MODULE_EXPORT void
settings_std_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  GtkNotebook *notebook =
    GTK_NOTEBOOK (name_to_widget (s, "opt_notebook"));

  /* Re-create UI to reflect the in-progress command-line settings. */
  populate_popup_window (s);

  gtk_notebook_set_page (notebook, 0);
}

G_MODULE_EXPORT void
settings_switch_page_cb (GtkNotebook *notebook, GtkNotebookPage *page,
                         gint page_num, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  GtkWidget *adv = name_to_widget (s, "adv_button");
  GtkWidget *std = name_to_widget (s, "std_button");

  if (page_num == 0)
    {
      gtk_widget_show (adv);
      gtk_widget_hide (std);
    }
  else if (page_num == 1)
    {
      gtk_widget_hide (adv);
      gtk_widget_show (std);
    }
  else
    abort();
}



G_MODULE_EXPORT void
settings_cancel_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  gtk_widget_hide (s->popup_widget);
}

G_MODULE_EXPORT void
settings_ok_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  GtkNotebook *notebook = GTK_NOTEBOOK (name_to_widget (s, "opt_notebook"));
  int page = gtk_notebook_get_current_page (notebook);

  if (page == 0)
    /* Regenerate the command-line from the widget contents before saving.
       But don't do this if we're looking at the command-line page already,
       or we will blow away what they typed... */
    settings_sync_cmd_text (s);

  flush_popup_changes_and_save (s);
  gtk_widget_hide (s->popup_widget);
}

static gboolean
wm_popup_close_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  state *s = (state *) data;
  settings_cancel_cb (0, (gpointer) s);
  return TRUE;
}



/* Populating the various widgets
 */


/* Returns the number of the last hack run by the server.
 */
static int
server_current_hack (void)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  unsigned char *dataP = 0;
  Display *dpy = GDK_DISPLAY();
  int hack_number = -1;

  if (XGetWindowProperty (dpy, RootWindow (dpy, 0), /* always screen #0 */
                          XA_SCREENSAVER_STATUS,
                          0, 3, False, XA_INTEGER,
                          &type, &format, &nitems, &bytesafter,
                          &dataP)
      == Success
      && type == XA_INTEGER
      && nitems >= 3
      && dataP)
    {
      PROP32 *data = (PROP32 *) dataP;
      hack_number = (int) data[2] - 1;
    }

  if (dataP) XFree (dataP);

  return hack_number;
}


/* Finds the number of the last hack that was run, and makes that item be
   selected by default.
 */
static void
scroll_to_current_hack (state *s)
{
  saver_preferences *p = &s->prefs;
  int hack_number = -1;

  if (p->mode == ONE_HACK)		   /* in "one" mode, use the one */
    hack_number = p->selected_hack;
  if (hack_number < 0)			   /* otherwise, use the last-run */
    hack_number = server_current_hack ();
  if (hack_number < 0)			   /* failing that, last "one mode" */
    hack_number = p->selected_hack;
  if (hack_number < 0)			   /* failing that, newest hack. */
    {
      /* We should only get here if the user does not have a .xscreensaver
         file, and the screen has not been blanked with a hack since X
         started up: in other words, this is probably a fresh install.

         Instead of just defaulting to hack #0 (in either "programs" or
         "alphabetical" order) let's try to default to the last runnable
         hack in the "programs" list: this is probably the hack that was
         most recently added to the xscreensaver distribution (and so
         it's probably the currently-coolest one!)
       */
      hack_number = p->screenhacks_count-1;
      while (hack_number > 0 &&
             ! (s->hacks_available_p[hack_number] &&
                p->screenhacks[hack_number]->enabled_p))
        hack_number--;
    }

  if (hack_number >= 0 && hack_number < p->screenhacks_count)
    {
      int list_elt = s->hack_number_to_list_elt[hack_number];
      GtkWidget *list = name_to_widget (s, "list");
      force_list_select_item (s, list, list_elt, True);
      populate_demo_window (s, list_elt);
    }
}


static void
populate_hack_list (state *s)
{
  Display *dpy = GDK_DISPLAY();
#ifdef HAVE_GTK2
  saver_preferences *p = &s->prefs;
  GtkTreeView *list = GTK_TREE_VIEW (name_to_widget (s, "list"));
  GtkListStore *model;
  GtkTreeSelection *selection;
  GtkCellRenderer *ren;
  GtkTreeIter iter;
  int i;

  g_object_get (G_OBJECT (list),
		"model", &model,
		NULL);
  if (!model)
    {
      model = gtk_list_store_new (COL_LAST, G_TYPE_BOOLEAN, G_TYPE_STRING);
      g_object_set (G_OBJECT (list), "model", model, NULL);
      g_object_unref (model);

      ren = gtk_cell_renderer_toggle_new ();
      gtk_tree_view_insert_column_with_attributes (list, COL_ENABLED,
						   _("Use"), ren,
						   "active", COL_ENABLED,
						   NULL);

      g_signal_connect (ren, "toggled",
			G_CALLBACK (list_checkbox_cb),
			s);

      ren = gtk_cell_renderer_text_new ();
      gtk_tree_view_insert_column_with_attributes (list, COL_NAME,
						   _("Screen Saver"), ren,
						   "markup", COL_NAME,
						   NULL);

      g_signal_connect_after (list, "row_activated",
			      G_CALLBACK (list_activated_cb),
			      s);

      selection = gtk_tree_view_get_selection (list);
      g_signal_connect (selection, "changed",
			G_CALLBACK (list_select_changed_cb),
			s);

    }

  for (i = 0; i < s->list_count; i++)
    {
      int hack_number = s->list_elt_to_hack_number[i];
      screenhack *hack = (hack_number < 0 ? 0 : p->screenhacks[hack_number]);
      char *pretty_name;
      Bool available_p = (hack && s->hacks_available_p [hack_number]);

      if (!hack) continue;

      /* If we're to suppress uninstalled hacks, check $PATH now. */
      if (p->ignore_uninstalled_p && !available_p)
        continue;

      pretty_name = (hack->name
                     ? strdup (hack->name)
                     : make_hack_name (dpy, hack->command));

      if (!available_p)
        {
          /* Make the text foreground be the color of insensitive widgets
             (but don't actually make it be insensitive, since we still
             want to be able to click on it.)
           */
          GtkStyle *style = GTK_WIDGET (list)->style;
          GdkColor *fg = &style->fg[GTK_STATE_INSENSITIVE];
       /* GdkColor *bg = &style->bg[GTK_STATE_INSENSITIVE]; */
          char *buf = (char *) malloc (strlen (pretty_name) + 100);

          sprintf (buf, "<span foreground=\"#%02X%02X%02X\""
                      /*     " background=\"#%02X%02X%02X\""  */
                        ">%s</span>",
                   fg->red >> 8, fg->green >> 8, fg->blue >> 8,
                /* bg->red >> 8, bg->green >> 8, bg->blue >> 8, */
                   pretty_name);
          free (pretty_name);
          pretty_name = buf;
        }

      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter,
			  COL_ENABLED, hack->enabled_p,
			  COL_NAME, pretty_name,
			  -1);
      free (pretty_name);
    }

#else /* !HAVE_GTK2 */

  saver_preferences *p = &s->prefs;
  GtkList *list = GTK_LIST (name_to_widget (s, "list"));
  int i;
  for (i = 0; i < s->list_count; i++)
    {
      int hack_number = s->list_elt_to_hack_number[i];
      screenhack *hack = (hack_number < 0 ? 0 : p->screenhacks[hack_number]);

      /* A GtkList must contain only GtkListItems, but those can contain
         an arbitrary widget.  We add an Hbox, and inside that, a Checkbox
         and a Label.  We handle single and double click events on the
         line itself, for clicking on the text, but the interior checkbox
         also handles its own events.
       */
      GtkWidget *line;
      GtkWidget *line_hbox;
      GtkWidget *line_check;
      GtkWidget *line_label;
      char *pretty_name;
      Bool available_p = (hack && s->hacks_available_p [hack_number]);

      if (!hack) continue;

      /* If we're to suppress uninstalled hacks, check $PATH now. */
      if (p->ignore_uninstalled_p && !available_p)
        continue;

      pretty_name = (hack->name
                     ? strdup (hack->name)
                     : make_hack_name (hack->command));

      line = gtk_list_item_new ();
      line_hbox = gtk_hbox_new (FALSE, 0);
      line_check = gtk_check_button_new ();
      line_label = gtk_label_new (pretty_name);

      gtk_container_add (GTK_CONTAINER (line), line_hbox);
      gtk_box_pack_start (GTK_BOX (line_hbox), line_check, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (line_hbox), line_label, FALSE, FALSE, 0);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (line_check),
                                    hack->enabled_p);
      gtk_label_set_justify (GTK_LABEL (line_label), GTK_JUSTIFY_LEFT);

      gtk_widget_show (line_check);
      gtk_widget_show (line_label);
      gtk_widget_show (line_hbox);
      gtk_widget_show (line);

      free (pretty_name);

      gtk_container_add (GTK_CONTAINER (list), line);
      gtk_signal_connect (GTK_OBJECT (line), "button_press_event",
                          GTK_SIGNAL_FUNC (list_doubleclick_cb),
                          (gpointer) s);

      gtk_signal_connect (GTK_OBJECT (line_check), "toggled",
                          GTK_SIGNAL_FUNC (list_checkbox_cb),
                          (gpointer) s);

      gtk_widget_show (line);

      if (!available_p)
        {
          /* Make the widget be colored like insensitive widgets
             (but don't actually make it be insensitive, since we
             still want to be able to click on it.)
           */
          GtkRcStyle *rc_style;
          GdkColor fg, bg;

          gtk_widget_realize (GTK_WIDGET (line_label));

          fg = GTK_WIDGET (line_label)->style->fg[GTK_STATE_INSENSITIVE];
          bg = GTK_WIDGET (line_label)->style->bg[GTK_STATE_INSENSITIVE];

          rc_style = gtk_rc_style_new ();
          rc_style->fg[GTK_STATE_NORMAL] = fg;
          rc_style->bg[GTK_STATE_NORMAL] = bg;
          rc_style->color_flags[GTK_STATE_NORMAL] |= GTK_RC_FG|GTK_RC_BG;

          gtk_widget_modify_style (GTK_WIDGET (line_label), rc_style);
          gtk_rc_style_unref (rc_style);
        }
    }

  gtk_signal_connect (GTK_OBJECT (list), "select_child",
                      GTK_SIGNAL_FUNC (list_select_cb),
                      (gpointer) s);
  gtk_signal_connect (GTK_OBJECT (list), "unselect_child",
                      GTK_SIGNAL_FUNC (list_unselect_cb),
                      (gpointer) s);
#endif /* !HAVE_GTK2 */
}

static void
update_list_sensitivity (state *s)
{
  saver_preferences *p = &s->prefs;
  Bool sensitive = (p->mode == RANDOM_HACKS ||
                    p->mode == RANDOM_HACKS_SAME ||
                    p->mode == ONE_HACK);
  Bool checkable = (p->mode == RANDOM_HACKS ||
                    p->mode == RANDOM_HACKS_SAME);
  Bool blankable = (p->mode != DONT_BLANK);

#ifndef HAVE_GTK2
  GtkWidget *head     = name_to_widget (s, "col_head_hbox");
  GtkWidget *use      = name_to_widget (s, "use_col_frame");
#endif /* HAVE_GTK2 */
  GtkWidget *scroller = name_to_widget (s, "scroller");
  GtkWidget *buttons  = name_to_widget (s, "next_prev_hbox");
  GtkWidget *blanker  = name_to_widget (s, "blanking_table");

#ifdef HAVE_GTK2
  GtkTreeView *list      = GTK_TREE_VIEW (name_to_widget (s, "list"));
  GtkTreeViewColumn *use = gtk_tree_view_get_column (list, COL_ENABLED);
#else /* !HAVE_GTK2 */
  GtkList *list = GTK_LIST (name_to_widget (s, "list"));
  GList *kids   = gtk_container_children (GTK_CONTAINER (list));

  gtk_widget_set_sensitive (GTK_WIDGET (head),     sensitive);
#endif /* !HAVE_GTK2 */
  gtk_widget_set_sensitive (GTK_WIDGET (scroller), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (buttons),  sensitive);

  gtk_widget_set_sensitive (GTK_WIDGET (blanker),  blankable);

#ifdef HAVE_GTK2
  gtk_tree_view_column_set_visible (use, checkable);
#else  /* !HAVE_GTK2 */
  if (checkable)
    gtk_widget_show (use);   /* the "Use" column header */
  else
    gtk_widget_hide (use);

  while (kids)
    {
      GtkBin *line = GTK_BIN (kids->data);
      GtkContainer *line_hbox = GTK_CONTAINER (line->child);
      GtkWidget *line_check =
        GTK_WIDGET (gtk_container_children (line_hbox)->data);
      
      if (checkable)
        gtk_widget_show (line_check);
      else
        gtk_widget_hide (line_check);

      kids = kids->next;
    }
#endif /* !HAVE_GTK2 */
}


static void
populate_prefs_page (state *s)
{
  saver_preferences *p = &s->prefs;

  Bool can_lock_p = True;

  /* Disable all the "lock" controls if locking support was not provided
     at compile-time, or if running on MacOS. */
# if defined(NO_LOCKING) || defined(__APPLE__)
  can_lock_p = False;
# endif


  /* If there is only one screen, the mode menu contains
     "random" but not "random-same".
   */
  if (s->nscreens <= 1 && p->mode == RANDOM_HACKS_SAME)
    p->mode = RANDOM_HACKS;


  /* The file supports timeouts of less than a minute, but the GUI does
     not, so throttle the values to be at least one minute (since "0" is
     a bad rounding choice...)
   */
# define THROTTLE(NAME) if (p->NAME != 0 && p->NAME < 60000) p->NAME = 60000
  THROTTLE (timeout);
  THROTTLE (cycle);
  /* THROTTLE (passwd_timeout); */  /* GUI doesn't set this; leave it alone */
# undef THROTTLE

# define FMT_MINUTES(NAME,N) \
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (name_to_widget (s, (NAME))), (double)((N) + 59) / (60 * 1000))

# define FMT_SECONDS(NAME,N) \
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (name_to_widget (s, (NAME))), (double)((N) / 1000))

  FMT_MINUTES ("timeout_spinbutton",      p->timeout);
  FMT_MINUTES ("cycle_spinbutton",        p->cycle);
  FMT_MINUTES ("lock_spinbutton",         p->lock_timeout);
  FMT_MINUTES ("dpms_standby_spinbutton", p->dpms_standby);
  FMT_MINUTES ("dpms_suspend_spinbutton", p->dpms_suspend);
  FMT_MINUTES ("dpms_off_spinbutton",     p->dpms_off);
  FMT_SECONDS ("fade_spinbutton",         p->fade_seconds);

# undef FMT_MINUTES
# undef FMT_SECONDS

# define TOGGLE_ACTIVE(NAME,ACTIVEP) \
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (name_to_widget (s,(NAME))),\
                                (ACTIVEP))

  TOGGLE_ACTIVE ("lock_button",       p->lock_p);
#if 0
  TOGGLE_ACTIVE ("verbose_button",    p->verbose_p);
  TOGGLE_ACTIVE ("capture_button",    p->capture_stderr_p);
  TOGGLE_ACTIVE ("splash_button",     p->splash_p);
#endif
  TOGGLE_ACTIVE ("dpms_button",       p->dpms_enabled_p);
  TOGGLE_ACTIVE ("grab_desk_button",  p->grab_desktop_p);
  TOGGLE_ACTIVE ("grab_video_button", p->grab_video_p);
  TOGGLE_ACTIVE ("grab_image_button", p->random_image_p);
  TOGGLE_ACTIVE ("install_button",    p->install_cmap_p);
  TOGGLE_ACTIVE ("fade_button",       p->fade_p);
  TOGGLE_ACTIVE ("unfade_button",     p->unfade_p);

  switch (p->tmode)
    {
    case TEXT_LITERAL: TOGGLE_ACTIVE ("text_radio",         True); break;
    case TEXT_FILE:    TOGGLE_ACTIVE ("text_file_radio",    True); break;
    case TEXT_PROGRAM: TOGGLE_ACTIVE ("text_program_radio", True); break;
    case TEXT_URL:     TOGGLE_ACTIVE ("text_url_radio",     True); break;
    default:           TOGGLE_ACTIVE ("text_host_radio",    True); break;
    }

# undef TOGGLE_ACTIVE

  gtk_entry_set_text (GTK_ENTRY (name_to_widget (s, "image_text")),
                      (p->image_directory ? p->image_directory : ""));
  gtk_widget_set_sensitive (name_to_widget (s, "image_text"),
                            p->random_image_p);
  gtk_widget_set_sensitive (name_to_widget (s, "image_browse_button"),
                            p->random_image_p);

  gtk_entry_set_text (GTK_ENTRY (name_to_widget (s, "text_entry")),
                      (p->text_literal ? p->text_literal : ""));
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (s, "text_file_entry")),
                      (p->text_file ? p->text_file : ""));
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (s, "text_program_entry")),
                      (p->text_program ? p->text_program : ""));
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (s, "text_url_entry")),
                      (p->text_url ? p->text_url : ""));

  gtk_widget_set_sensitive (name_to_widget (s, "text_entry"),
                            p->tmode == TEXT_LITERAL);
  gtk_widget_set_sensitive (name_to_widget (s, "text_file_entry"),
                            p->tmode == TEXT_FILE);
  gtk_widget_set_sensitive (name_to_widget (s, "text_file_browse"),
                            p->tmode == TEXT_FILE);
  gtk_widget_set_sensitive (name_to_widget (s, "text_program_entry"),
                            p->tmode == TEXT_PROGRAM);
  gtk_widget_set_sensitive (name_to_widget (s, "text_program_browse"),
                            p->tmode == TEXT_PROGRAM);
  gtk_widget_set_sensitive (name_to_widget (s, "text_url_entry"),
                            p->tmode == TEXT_URL);


  /* Map the `saver_mode' enum to mode menu to values. */
  {
    GtkOptionMenu *opt = GTK_OPTION_MENU (name_to_widget (s, "mode_menu"));

    int i;
    for (i = 0; i < countof(mode_menu_order); i++)
      if (mode_menu_order[i] == p->mode)
        break;
    gtk_option_menu_set_history (opt, i);
    update_list_sensitivity (s);
  }

  {
    Bool found_any_writable_cells = False;
    Bool fading_possible = False;
    Bool dpms_supported = False;

    Display *dpy = GDK_DISPLAY();
    int nscreens = ScreenCount(dpy);  /* real screens, not Xinerama */
    int i;
    for (i = 0; i < nscreens; i++)
      {
	Screen *s = ScreenOfDisplay (dpy, i);
	if (has_writable_cells (s, DefaultVisualOfScreen (s)))
	  {
	    found_any_writable_cells = True;
	    break;
	  }
      }

    fading_possible = found_any_writable_cells;
#ifdef HAVE_XF86VMODE_GAMMA
    fading_possible = True;
#endif

#ifdef HAVE_DPMS_EXTENSION
    {
      int op = 0, event = 0, error = 0;
      if (XQueryExtension (dpy, "DPMS", &op, &event, &error))
        dpms_supported = True;
    }
#endif /* HAVE_DPMS_EXTENSION */


# define SENSITIZE(NAME,SENSITIVEP) \
    gtk_widget_set_sensitive (name_to_widget (s, (NAME)), (SENSITIVEP))

    /* Blanking and Locking
     */
    SENSITIZE ("lock_button",     can_lock_p);
    SENSITIZE ("lock_spinbutton", can_lock_p && p->lock_p);
    SENSITIZE ("lock_mlabel",     can_lock_p && p->lock_p);

    /* DPMS
     */
    SENSITIZE ("dpms_frame",              dpms_supported);
    SENSITIZE ("dpms_button",             dpms_supported);
    SENSITIZE ("dpms_standby_label",      dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_standby_mlabel",     dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_standby_spinbutton", dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_suspend_label",      dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_suspend_mlabel",     dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_suspend_spinbutton", dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_off_label",          dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_off_mlabel",         dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_off_spinbutton",     dpms_supported && p->dpms_enabled_p);

    /* Colormaps
     */
    SENSITIZE ("cmap_frame",      found_any_writable_cells || fading_possible);
    SENSITIZE ("install_button",  found_any_writable_cells);
    SENSITIZE ("fade_button",     fading_possible);
    SENSITIZE ("unfade_button",   fading_possible);

    SENSITIZE ("fade_label",      (fading_possible &&
                                   (p->fade_p || p->unfade_p)));
    SENSITIZE ("fade_spinbutton", (fading_possible &&
                                   (p->fade_p || p->unfade_p)));

# undef SENSITIZE
  }
}


static void
populate_popup_window (state *s)
{
  GtkLabel *doc = GTK_LABEL (name_to_widget (s, "doc"));
  char *doc_string = 0;

  /* #### not in Gtk 1.2
  gtk_label_set_selectable (doc);
   */

# ifdef HAVE_XML
  if (s->cdata)
    {
      free_conf_data (s->cdata);
      s->cdata = 0;
    }

  {
    saver_preferences *p = &s->prefs;
    int list_elt = selected_list_element (s);
    int hack_number = (list_elt >= 0 && list_elt < s->list_count
                       ? s->list_elt_to_hack_number[list_elt]
                       : -1);
    screenhack *hack = (hack_number >= 0 ? p->screenhacks[hack_number] : 0);
    if (hack)
      {
        GtkWidget *parent = name_to_widget (s, "settings_vbox");
        GtkWidget *cmd = GTK_WIDGET (name_to_widget (s, "cmd_text"));
        const char *cmd_line = gtk_entry_get_text (GTK_ENTRY (cmd));
        s->cdata = load_configurator (cmd_line, s->debug_p);
        if (s->cdata && s->cdata->widget)
          gtk_box_pack_start (GTK_BOX (parent), s->cdata->widget,
                              TRUE, TRUE, 0);
      }
  }

  doc_string = (s->cdata
                ? s->cdata->description
                : 0);
# else  /* !HAVE_XML */
  doc_string = _("Descriptions not available: no XML support compiled in.");
# endif /* !HAVE_XML */

  gtk_label_set_text (doc, (doc_string
                            ? _(doc_string)
                            : _("No description available.")));
}


static void
sensitize_demo_widgets (state *s, Bool sensitive_p)
{
  const char *names[] = { "demo", "settings",
                          "cmd_label", "cmd_text", "manual",
                          "visual", "visual_combo" };
  int i;
  for (i = 0; i < countof(names); i++)
    {
      GtkWidget *w = name_to_widget (s, names[i]);
      gtk_widget_set_sensitive (GTK_WIDGET(w), sensitive_p);
    }
}


static void
sensitize_menu_items (state *s, Bool force_p)
{
  static Bool running_p = False;
  static time_t last_checked = 0;
  time_t now = time ((time_t *) 0);
  const char *names[] = { "activate_menu", "lock_menu", "kill_menu",
                          /* "demo" */ };
  int i;

  if (force_p || now > last_checked + 10)   /* check every 10 seconds */
    {
      running_p = xscreensaver_running_p (s);
      last_checked = time ((time_t *) 0);
    }

  for (i = 0; i < countof(names); i++)
    {
      GtkWidget *w = name_to_widget (s, names[i]);
      gtk_widget_set_sensitive (GTK_WIDGET(w), running_p);
    }
}


/* When the File menu is de-posted after a "Restart Daemon" command,
   the window underneath doesn't repaint for some reason.  I guess this
   is a bug in exposure handling in GTK or GDK.  This works around it.
 */
static void
force_dialog_repaint (state *s)
{
#if 1
  /* Tell GDK to invalidate and repaint the whole window.
   */
  GdkWindow *w = s->toplevel_widget->window;
  GdkRegion *region = gdk_region_new ();
  GdkRectangle rect;
  rect.x = rect.y = 0;
  rect.width = rect.height = 32767;
  gdk_region_union_with_rect (region, &rect);
  gdk_window_invalidate_region (w, region, True);
  gdk_region_destroy (region);
  gdk_window_process_updates (w, True);
#else
  /* Force the server to send an exposure event by creating and then
     destroying a window as a child of the top level shell.
   */
  Display *dpy = GDK_DISPLAY();
  Window parent = GDK_WINDOW_XWINDOW (s->toplevel_widget->window);
  Window w;
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, parent, &xgwa);
  w = XCreateSimpleWindow (dpy, parent, 0, 0, xgwa.width, xgwa.height, 0,0,0);
  XMapRaised (dpy, w);
  XDestroyWindow (dpy, w);
  XSync (dpy, False);
#endif
}


/* Even though we've given these text fields a maximum number of characters,
   their default size is still about 30 characters wide -- so measure out
   a string in their font, and resize them to just fit that.
 */
static void
fix_text_entry_sizes (state *s)
{
  GtkWidget *w;

# if 0   /* appears no longer necessary with Gtk 1.2.10 */
  const char * const spinbuttons[] = {
    "timeout_spinbutton", "cycle_spinbutton", "lock_spinbutton",
    "dpms_standby_spinbutton", "dpms_suspend_spinbutton",
    "dpms_off_spinbutton",
    "-fade_spinbutton" };
  int i;
  int width = 0;

  for (i = 0; i < countof(spinbuttons); i++)
    {
      const char *n = spinbuttons[i];
      int cols = 4;
      while (*n == '-') n++, cols--;
      w = GTK_WIDGET (name_to_widget (s, n));
      width = gdk_text_width (w->style->font, "MMMMMMMM", cols);
      gtk_widget_set_usize (w, width, -2);
    }

  /* Now fix the width of the combo box.
   */
  w = GTK_WIDGET (name_to_widget (s, "visual_combo"));
  w = GTK_COMBO (w)->entry;
  width = gdk_string_width (w->style->font, "PseudoColor___");
  gtk_widget_set_usize (w, width, -2);

  /* Now fix the width of the file entry text.
   */
  w = GTK_WIDGET (name_to_widget (s, "image_text"));
  width = gdk_string_width (w->style->font, "mmmmmmmmmmmmmm");
  gtk_widget_set_usize (w, width, -2);

  /* Now fix the width of the command line text.
   */
  w = GTK_WIDGET (name_to_widget (s, "cmd_text"));
  width = gdk_string_width (w->style->font, "mmmmmmmmmmmmmmmmmmmm");
  gtk_widget_set_usize (w, width, -2);

# endif /* 0 */

  /* Now fix the height of the list widget:
     make it default to being around 10 text-lines high instead of 4.
   */
  w = GTK_WIDGET (name_to_widget (s, "list"));
  {
    int lines = 10;
    int height;
    int leading = 3;  /* approximate is ok... */
    int border = 2;

#ifdef HAVE_GTK2
    PangoFontMetrics *pain =
      pango_context_get_metrics (gtk_widget_get_pango_context (w),
                                 w->style->font_desc,
                                 gtk_get_default_language ());
    height = PANGO_PIXELS (pango_font_metrics_get_ascent (pain) +
                           pango_font_metrics_get_descent (pain));
#else  /* !HAVE_GTK2 */
    height = w->style->font->ascent + w->style->font->descent;
#endif /* !HAVE_GTK2 */

    height += leading;
    height *= lines;
    height += border * 2;
    w = GTK_WIDGET (name_to_widget (s, "scroller"));
    gtk_widget_set_usize (w, -2, height);
  }
}


#ifndef HAVE_GTK2

/* Pixmaps for the up and down arrow buttons (yeah, this is sleazy...)
 */

static char *up_arrow_xpm[] = {
  "15 15 4 1",
  " 	c None",
  "-	c #FFFFFF",
  "+	c #D6D6D6",
  "@	c #000000",

  "       @       ",
  "       @       ",
  "      -+@      ",
  "      -+@      ",
  "     -+++@     ",
  "     -+++@     ",
  "    -+++++@    ",
  "    -+++++@    ",
  "   -+++++++@   ",
  "   -+++++++@   ",
  "  -+++++++++@  ",
  "  -+++++++++@  ",
  " -+++++++++++@ ",
  " @@@@@@@@@@@@@ ",
  "               ",

  /* Need these here because gdk_pixmap_create_from_xpm_d() walks off
     the end of the array (Gtk 1.2.5.) */
  "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000",
  "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
};

static char *down_arrow_xpm[] = {
  "15 15 4 1",
  " 	c None",
  "-	c #FFFFFF",
  "+	c #D6D6D6",
  "@	c #000000",

  "               ",
  " ------------- ",
  " -+++++++++++@ ",
  "  -+++++++++@  ",
  "  -+++++++++@  ",
  "   -+++++++@   ",
  "   -+++++++@   ",
  "    -+++++@    ",
  "    -+++++@    ",
  "     -+++@     ",
  "     -+++@     ",
  "      -+@      ",
  "      -+@      ",
  "       @       ",
  "       @       ",

  /* Need these here because gdk_pixmap_create_from_xpm_d() walks off
     the end of the array (Gtk 1.2.5.) */
  "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000",
  "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
};

static void
pixmapify_button (state *s, int down_p)
{
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkWidget *pixmapwid;
  GtkStyle *style;
  GtkWidget *w;

  w = GTK_WIDGET (name_to_widget (s, (down_p ? "next" : "prev")));
  style = gtk_widget_get_style (w);
  mask = 0;
  pixmap = gdk_pixmap_create_from_xpm_d (w->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         (down_p
                                          ? (gchar **) down_arrow_xpm
                                          : (gchar **) up_arrow_xpm));
  pixmapwid = gtk_pixmap_new (pixmap, mask);
  gtk_widget_show (pixmapwid);
  gtk_container_remove (GTK_CONTAINER (w), GTK_BIN (w)->child);
  gtk_container_add (GTK_CONTAINER (w), pixmapwid);
}

static void
map_next_button_cb (GtkWidget *w, gpointer user_data)
{
  state *s = (state *) user_data;
  pixmapify_button (s, 1);
}

static void
map_prev_button_cb (GtkWidget *w, gpointer user_data)
{
  state *s = (state *) user_data;
  pixmapify_button (s, 0);
}
#endif /* !HAVE_GTK2 */


#ifndef HAVE_GTK2
/* Work around a Gtk bug that causes label widgets to wrap text too early.
 */

static void
you_are_not_a_unique_or_beautiful_snowflake (GtkWidget *label,
                                             GtkAllocation *allocation,
					     void *foo)
{
  GtkRequisition req;
  GtkWidgetAuxInfo *aux_info;

  aux_info = gtk_object_get_data (GTK_OBJECT (label), "gtk-aux-info");

  aux_info->width = allocation->width;
  aux_info->height = -2;
  aux_info->x = -1;
  aux_info->y = -1;

  gtk_widget_size_request (label, &req);
}

/* Feel the love.  Thanks to Nat Friedman for finding this workaround.
 */
static void
eschew_gtk_lossage (GtkLabel *label)
{
  GtkWidgetAuxInfo *aux_info = g_new0 (GtkWidgetAuxInfo, 1);
  aux_info->width = GTK_WIDGET (label)->allocation.width;
  aux_info->height = -2;
  aux_info->x = -1;
  aux_info->y = -1;

  gtk_object_set_data (GTK_OBJECT (label), "gtk-aux-info", aux_info);

  gtk_signal_connect (GTK_OBJECT (label), "size_allocate",
                      GTK_SIGNAL_FUNC (you_are_not_a_unique_or_beautiful_snowflake),
                      0);

  gtk_widget_set_usize (GTK_WIDGET (label), -2, -2);

  gtk_widget_queue_resize (GTK_WIDGET (label));
}
#endif /* !HAVE_GTK2 */


static void
populate_demo_window (state *s, int list_elt)
{
  Display *dpy = GDK_DISPLAY();
  saver_preferences *p = &s->prefs;
  screenhack *hack;
  char *pretty_name;
  GtkFrame *frame1 = GTK_FRAME (name_to_widget (s, "preview_frame"));
  GtkFrame *frame2 = GTK_FRAME (name_to_widget (s, "doc_frame"));
  GtkEntry *cmd    = GTK_ENTRY (name_to_widget (s, "cmd_text"));
  GtkCombo *vis    = GTK_COMBO (name_to_widget (s, "visual_combo"));
  GtkWidget *list  = GTK_WIDGET (name_to_widget (s, "list"));

  if (p->mode == BLANK_ONLY)
    {
      hack = 0;
      pretty_name = strdup (_("Blank Screen"));
      schedule_preview (s, 0);
    }
  else if (p->mode == DONT_BLANK)
    {
      hack = 0;
      pretty_name = strdup (_("Screen Saver Disabled"));
      schedule_preview (s, 0);
    }
  else
    {
      int hack_number = (list_elt >= 0 && list_elt < s->list_count
                         ? s->list_elt_to_hack_number[list_elt]
                         : -1);
      hack = (hack_number >= 0 ? p->screenhacks[hack_number] : 0);

      pretty_name = (hack
                     ? (hack->name
                        ? strdup (hack->name)
                        : make_hack_name (dpy, hack->command))
                     : 0);

      if (hack)
        schedule_preview (s, hack->command);
      else
        schedule_preview (s, 0);
    }

  if (!pretty_name)
    pretty_name = strdup (_("Preview"));

  gtk_frame_set_label (frame1, _(pretty_name));
  gtk_frame_set_label (frame2, _(pretty_name));

  gtk_entry_set_text (cmd, (hack ? hack->command : ""));
  gtk_entry_set_position (cmd, 0);

  {
    char title[255];
    sprintf (title, _("%s: %.100s Settings"),
             progclass, (pretty_name ? pretty_name : "???"));
    gtk_window_set_title (GTK_WINDOW (s->popup_widget), title);
  }

  gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (vis)->entry),
                      (hack
                       ? (hack->visual && *hack->visual
                          ? hack->visual
                          : _("Any"))
                       : ""));

  sensitize_demo_widgets (s, (hack ? True : False));

  if (pretty_name) free (pretty_name);

  ensure_selected_item_visible (list);

  s->_selected_list_element = list_elt;
}


static void
widget_deleter (GtkWidget *widget, gpointer data)
{
  /* #### Well, I want to destroy these widgets, but if I do that, they get
     referenced again, and eventually I get a SEGV.  So instead of
     destroying them, I'll just hide them, and leak a bunch of memory
     every time the disk file changes.  Go go go Gtk!

     #### Ok, that's a lie, I get a crash even if I just hide the widget
     and don't ever delete it.  Fuck!
   */
#if 0
  gtk_widget_destroy (widget);
#else
  gtk_widget_hide (widget);
#endif
}


static char **sort_hack_cmp_names_kludge;
static int
sort_hack_cmp (const void *a, const void *b)
{
  if (a == b)
    return 0;
  else
    {
      int aa = *(int *) a;
      int bb = *(int *) b;
      const char last[] = "\377\377\377\377\377\377\377\377\377\377\377";
      return strcmp ((aa < 0 ? last : sort_hack_cmp_names_kludge[aa]),
                     (bb < 0 ? last : sort_hack_cmp_names_kludge[bb]));
    }
}


static void
initialize_sort_map (state *s)
{
  Display *dpy = GDK_DISPLAY();
  saver_preferences *p = &s->prefs;
  int i, j;

  if (s->list_elt_to_hack_number) free (s->list_elt_to_hack_number);
  if (s->hack_number_to_list_elt) free (s->hack_number_to_list_elt);
  if (s->hacks_available_p) free (s->hacks_available_p);

  s->list_elt_to_hack_number = (int *)
    calloc (sizeof(int), p->screenhacks_count + 1);
  s->hack_number_to_list_elt = (int *)
    calloc (sizeof(int), p->screenhacks_count + 1);
  s->hacks_available_p = (Bool *)
    calloc (sizeof(Bool), p->screenhacks_count + 1);
  s->total_available = 0;

  /* Check which hacks actually exist on $PATH
   */
  for (i = 0; i < p->screenhacks_count; i++)
    {
      screenhack *hack = p->screenhacks[i];
      int on = on_path_p (hack->command) ? 1 : 0;
      s->hacks_available_p[i] = on;
      s->total_available += on;
    }

  /* Initialize list->hack table to unsorted mapping, omitting nonexistent
     hacks, if desired.
   */
  j = 0;
  for (i = 0; i < p->screenhacks_count; i++)
    {
      if (!p->ignore_uninstalled_p ||
          s->hacks_available_p[i])
        s->list_elt_to_hack_number[j++] = i;
    }
  s->list_count = j;

  for (; j < p->screenhacks_count; j++)
    s->list_elt_to_hack_number[j] = -1;


  /* Generate list of sortable names (once)
   */
  sort_hack_cmp_names_kludge = (char **)
    calloc (sizeof(char *), p->screenhacks_count);
  for (i = 0; i < p->screenhacks_count; i++)
    {
      screenhack *hack = p->screenhacks[i];
      char *name = (hack->name && *hack->name
                    ? strdup (hack->name)
                    : make_hack_name (dpy, hack->command));
      char *str;
      for (str = name; *str; str++)
        *str = tolower(*str);
      sort_hack_cmp_names_kludge[i] = name;
    }

  /* Sort list->hack map alphabetically
   */
  qsort (s->list_elt_to_hack_number,
         p->screenhacks_count,
         sizeof(*s->list_elt_to_hack_number),
         sort_hack_cmp);

  /* Free names
   */
  for (i = 0; i < p->screenhacks_count; i++)
    free (sort_hack_cmp_names_kludge[i]);
  free (sort_hack_cmp_names_kludge);
  sort_hack_cmp_names_kludge = 0;

  /* Build inverse table */
  for (i = 0; i < p->screenhacks_count; i++)
    {
      int n = s->list_elt_to_hack_number[i];
      if (n != -1)
        s->hack_number_to_list_elt[n] = i;
    }
}


static int
maybe_reload_init_file (state *s)
{
  Display *dpy = GDK_DISPLAY();
  saver_preferences *p = &s->prefs;
  int status = 0;

  static Bool reentrant_lock = False;
  if (reentrant_lock) return 0;
  reentrant_lock = True;

  if (init_file_changed_p (p))
    {
      const char *f = init_file_name();
      char *b;
      int list_elt;
      GtkWidget *list;

      if (!f || !*f) return 0;
      b = (char *) malloc (strlen(f) + 1024);
      sprintf (b,
               _("Warning:\n\n"
		 "file \"%s\" has changed, reloading.\n"),
               f);
      warning_dialog (s->toplevel_widget, b, D_NONE, 100);
      free (b);

      load_init_file (dpy, p);
      initialize_sort_map (s);

      list_elt = selected_list_element (s);
      list = name_to_widget (s, "list");
      gtk_container_foreach (GTK_CONTAINER (list), widget_deleter, NULL);
      populate_hack_list (s);
      force_list_select_item (s, list, list_elt, True);
      populate_prefs_page (s);
      populate_demo_window (s, list_elt);
      ensure_selected_item_visible (list);

      status = 1;
    }

  reentrant_lock = False;
  return status;
}



/* Making the preview window have the right X visual (so that GL works.)
 */

static Visual *get_best_gl_visual (state *);

static GdkVisual *
x_visual_to_gdk_visual (Visual *xv)
{
  GList *gvs = gdk_list_visuals();
  if (!xv) return gdk_visual_get_system();
  for (; gvs; gvs = gvs->next)
    {
      GdkVisual *gv = (GdkVisual *) gvs->data;
      if (xv == GDK_VISUAL_XVISUAL (gv))
        return gv;
    }
  fprintf (stderr, "%s: couldn't convert X Visual 0x%lx to a GdkVisual\n",
           blurb(), (unsigned long) xv->visualid);
  abort();
}

static void
clear_preview_window (state *s)
{
  GtkWidget *p;
  GdkWindow *window;

  if (!s->toplevel_widget) return;  /* very early */
  p = name_to_widget (s, "preview");
  window = p->window;

  if (!window) return;

  /* Flush the widget background down into the window, in case a subproc
     has changed it. */
  gdk_window_set_background (window, &p->style->bg[GTK_STATE_NORMAL]);
  gdk_window_clear (window);

  {
    int list_elt = selected_list_element (s);
    int hack_number = (list_elt >= 0
                       ? s->list_elt_to_hack_number[list_elt]
                       : -1);
    Bool available_p = (hack_number >= 0
                        ? s->hacks_available_p [hack_number]
                        : True);
    Bool nothing_p = (s->total_available < 5);

#ifdef HAVE_GTK2
    GtkWidget *notebook = name_to_widget (s, "preview_notebook");
    gtk_notebook_set_page (GTK_NOTEBOOK (notebook),
			   (s->running_preview_error_p
                            ? (available_p ? 1 :
                               nothing_p ? 3 : 2)
                            : 0));
#else /* !HAVE_GTK2 */
    if (s->running_preview_error_p)
      {
        const char * const lines1[] = { N_("No Preview"), N_("Available") };
        const char * const lines2[] = { N_("Not"), N_("Installed") };
        int nlines = countof(lines1);
        int lh = p->style->font->ascent + p->style->font->descent;
        int y, i;
        gint w, h;

        const char * const *lines = (available_p ? lines1 : lines2);

        gdk_window_get_size (window, &w, &h);
        y = (h - (lh * nlines)) / 2;
        y += p->style->font->ascent;
        for (i = 0; i < nlines; i++)
          {
            int sw = gdk_string_width (p->style->font, _(lines[i]));
            int x = (w - sw) / 2;
            gdk_draw_string (window, p->style->font,
                             p->style->fg_gc[GTK_STATE_NORMAL],
                             x, y, _(lines[i]));
            y += lh;
          }
      }
#endif /* !HAVE_GTK2 */
  }

  gdk_flush ();
}


static void
reset_preview_window (state *s)
{
  /* On some systems (most recently, MacOS X) OpenGL programs get confused
     when you kill one and re-start another on the same window.  So maybe
     it's best to just always destroy and recreate the preview window
     when changing hacks, instead of always trying to reuse the same one?
   */
  GtkWidget *pr = name_to_widget (s, "preview");
  if (GTK_WIDGET_REALIZED (pr))
    {
      Window oid = (pr->window ? GDK_WINDOW_XWINDOW (pr->window) : 0);
      Window id;
      gtk_widget_hide (pr);
      gtk_widget_unrealize (pr);
      gtk_widget_realize (pr);
      gtk_widget_show (pr);
      id = (pr->window ? GDK_WINDOW_XWINDOW (pr->window) : 0);
      if (s->debug_p)
        fprintf (stderr, "%s: window id 0x%X -> 0x%X\n", blurb(),
                 (unsigned int) oid,
                 (unsigned int) id);
    }
}


static void
fix_preview_visual (state *s)
{
  GtkWidget *widget = name_to_widget (s, "preview");
  Visual *xvisual = get_best_gl_visual (s);
  GdkVisual *visual = x_visual_to_gdk_visual (xvisual);
  GdkVisual *dvisual = gdk_visual_get_system();
  GdkColormap *cmap = (visual == dvisual
                       ? gdk_colormap_get_system ()
                       : gdk_colormap_new (visual, False));

  if (s->debug_p)
    fprintf (stderr, "%s: using %s visual 0x%lx\n", blurb(),
             (visual == dvisual ? "default" : "non-default"),
             (xvisual ? (unsigned long) xvisual->visualid : 0L));

  if (!GTK_WIDGET_REALIZED (widget) ||
      gtk_widget_get_visual (widget) != visual)
    {
      gtk_widget_unrealize (widget);
      gtk_widget_set_visual (widget, visual);
      gtk_widget_set_colormap (widget, cmap);
      gtk_widget_realize (widget);
    }

  /* Set the Widget colors to be white-on-black. */
  {
    GdkWindow *window = widget->window;
    GtkStyle *style = gtk_style_copy (widget->style);
    GdkColormap *cmap = gtk_widget_get_colormap (widget);
    GdkColor *fg = &style->fg[GTK_STATE_NORMAL];
    GdkColor *bg = &style->bg[GTK_STATE_NORMAL];
    GdkGC *fgc = gdk_gc_new(window);
    GdkGC *bgc = gdk_gc_new(window);
    if (!gdk_color_white (cmap, fg)) abort();
    if (!gdk_color_black (cmap, bg)) abort();
    gdk_gc_set_foreground (fgc, fg);
    gdk_gc_set_background (fgc, bg);
    gdk_gc_set_foreground (bgc, bg);
    gdk_gc_set_background (bgc, fg);
    style->fg_gc[GTK_STATE_NORMAL] = fgc;
    style->bg_gc[GTK_STATE_NORMAL] = fgc;
    gtk_widget_set_style (widget, style);

    /* For debugging purposes, put a title on the window (so that
       it can be easily found in the output of "xwininfo -tree".)
     */
    gdk_window_set_title (window, "Preview");
  }

  gtk_widget_show (widget);
}


/* Subprocesses
 */

static char *
subproc_pretty_name (state *s)
{
  if (s->running_preview_cmd)
    {
      char *ps = strdup (s->running_preview_cmd);
      char *ss = strchr (ps, ' ');
      if (ss) *ss = 0;
      ss = strrchr (ps, '/');
      if (!ss)
        ss = ps;
      else
        {
          ss = strdup (ss+1);
          free (ps);
        }
      return ss;
    }
  else
    return strdup ("???");
}


static void
reap_zombies (state *s)
{
  int wait_status = 0;
  pid_t pid;
  while ((pid = waitpid (-1, &wait_status, WNOHANG|WUNTRACED)) > 0)
    {
      if (s->debug_p)
        {
          if (pid == s->running_preview_pid)
            {
              char *ss = subproc_pretty_name (s);
              fprintf (stderr, "%s: pid %lu (%s) died\n", blurb(),
                       (unsigned long) pid, ss);
              free (ss);
            }
          else
            fprintf (stderr, "%s: pid %lu died\n", blurb(),
                     (unsigned long) pid);
        }
    }
}


/* Mostly lifted from driver/subprocs.c */
static Visual *
get_best_gl_visual (state *s)
{
  Display *dpy = GDK_DISPLAY();
  pid_t forked;
  int fds [2];
  int in, out;
  char buf[1024];

  char *av[10];
  int ac = 0;

  av[ac++] = "xscreensaver-gl-helper";
  av[ac] = 0;

  if (pipe (fds))
    {
      perror ("error creating pipe:");
      return 0;
    }

  in = fds [0];
  out = fds [1];

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
        sprintf (buf, "%s: couldn't fork", blurb());
        perror (buf);
        exit (1);
      }
    case 0:
      {
        int stdout_fd = 1;

        close (in);  /* don't need this one */
        close (ConnectionNumber (dpy));		/* close display fd */

        if (dup2 (out, stdout_fd) < 0)		/* pipe stdout */
          {
            perror ("could not dup() a new stdout:");
            return 0;
          }

        execvp (av[0], av);			/* shouldn't return. */

        if (errno != ENOENT)
          {
            /* Ignore "no such file or directory" errors, unless verbose.
               Issue all other exec errors, though. */
            sprintf (buf, "%s: running %s", blurb(), av[0]);
            perror (buf);
          }

        /* Note that one must use _exit() instead of exit() in procs forked
           off of Gtk programs -- Gtk installs an atexit handler that has a
           copy of the X connection (which we've already closed, for safety.)
           If one uses exit() instead of _exit(), then one sometimes gets a
           spurious "Gdk-ERROR: Fatal IO error on X server" error message.
        */
        _exit (1);                              /* exits fork */
        break;
      }
    default:
      {
        int result = 0;
        int wait_status = 0;

        FILE *f = fdopen (in, "r");
        unsigned int v = 0;
        char c;

        close (out);  /* don't need this one */

        *buf = 0;
        if (!fgets (buf, sizeof(buf)-1, f))
          *buf = 0;
        fclose (f);

        /* Wait for the child to die. */
        waitpid (-1, &wait_status, 0);

        if (1 == sscanf (buf, "0x%x %c", &v, &c))
          result = (int) v;

        if (result == 0)
          {
            if (s->debug_p)
              fprintf (stderr, "%s: %s did not report a GL visual!\n",
                       blurb(), av[0]);
            return 0;
          }
        else
          {
            Visual *v = id_to_visual (DefaultScreenOfDisplay (dpy), result);
            if (s->debug_p)
              fprintf (stderr, "%s: %s says the GL visual is 0x%X.\n",
                       blurb(), av[0], result);
            if (!v) abort();
            return v;
          }
      }
    }

  abort();
}


static void
kill_preview_subproc (state *s, Bool reset_p)
{
  s->running_preview_error_p = False;

  reap_zombies (s);
  clear_preview_window (s);

  if (s->subproc_check_timer_id)
    {
      gtk_timeout_remove (s->subproc_check_timer_id);
      s->subproc_check_timer_id = 0;
      s->subproc_check_countdown = 0;
    }

  if (s->running_preview_pid)
    {
      int status = kill (s->running_preview_pid, SIGTERM);
      char *ss = subproc_pretty_name (s);

      if (status < 0)
        {
          if (errno == ESRCH)
            {
              if (s->debug_p)
                fprintf (stderr, "%s: pid %lu (%s) was already dead.\n",
                         blurb(), (unsigned long) s->running_preview_pid, ss);
            }
          else
            {
              char buf [1024];
              sprintf (buf, "%s: couldn't kill pid %lu (%s)",
                       blurb(), (unsigned long) s->running_preview_pid, ss);
              perror (buf);
            }
        }
      else {
	int endstatus;
	waitpid(s->running_preview_pid, &endstatus, 0);
	if (s->debug_p)
	  fprintf (stderr, "%s: killed pid %lu (%s)\n", blurb(),
		   (unsigned long) s->running_preview_pid, ss);
      }

      free (ss);
      s->running_preview_pid = 0;
      if (s->running_preview_cmd) free (s->running_preview_cmd);
      s->running_preview_cmd = 0;
    }

  reap_zombies (s);

  if (reset_p)
    {
      reset_preview_window (s);
      clear_preview_window (s);
    }
}


/* Immediately and unconditionally launches the given process,
   after appending the -window-id option; sets running_preview_pid.
 */
static void
launch_preview_subproc (state *s)
{
  saver_preferences *p = &s->prefs;
  Window id;
  char *new_cmd = 0;
  pid_t forked;
  const char *cmd = s->desired_preview_cmd;

  GtkWidget *pr = name_to_widget (s, "preview");
  GdkWindow *window;

  reset_preview_window (s);

  window = pr->window;

  s->running_preview_error_p = False;

  if (s->preview_suppressed_p)
    {
      kill_preview_subproc (s, False);
      goto DONE;
    }

  new_cmd = malloc (strlen (cmd) + 40);

  id = (window ? GDK_WINDOW_XWINDOW (window) : 0);
  if (id == 0)
    {
      /* No window id?  No command to run. */
      free (new_cmd);
      new_cmd = 0;
    }
  else
    {
      strcpy (new_cmd, cmd);
      sprintf (new_cmd + strlen (new_cmd), " -window-id 0x%X",
               (unsigned int) id);
    }

  kill_preview_subproc (s, False);
  if (! new_cmd)
    {
      s->running_preview_error_p = True;
      clear_preview_window (s);
      goto DONE;
    }

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
        char buf[255];
        sprintf (buf, "%s: couldn't fork", blurb());
        perror (buf);
        s->running_preview_error_p = True;
        goto DONE;
        break;
      }
    case 0:
      {
        close (ConnectionNumber (GDK_DISPLAY()));

        hack_subproc_environment (id, s->debug_p);

        usleep (250000);  /* pause for 1/4th second before launching, to give
                             the previous program time to die and flush its X
                             buffer, so we don't get leftover turds on the
                             window. */

        exec_command (p->shell, new_cmd, p->nice_inferior);
        /* Don't bother printing an error message when we are unable to
           exec subprocesses; we handle that by polling the pid later.

           Note that one must use _exit() instead of exit() in procs forked
           off of Gtk programs -- Gtk installs an atexit handler that has a
           copy of the X connection (which we've already closed, for safety.)
           If one uses exit() instead of _exit(), then one sometimes gets a
           spurious "Gdk-ERROR: Fatal IO error on X server" error message.
        */
        _exit (1);  /* exits child fork */
        break;

      default:

        if (s->running_preview_cmd) free (s->running_preview_cmd);
        s->running_preview_cmd = strdup (s->desired_preview_cmd);
        s->running_preview_pid = forked;

        if (s->debug_p)
          {
            char *ss = subproc_pretty_name (s);
            fprintf (stderr, "%s: forked %lu (%s)\n", blurb(),
                     (unsigned long) forked, ss);
            free (ss);
          }
        break;
      }
    }

  schedule_preview_check (s);

 DONE:
  if (new_cmd) free (new_cmd);
  new_cmd = 0;
}


/* Modify $DISPLAY and $PATH for the benefit of subprocesses.
 */
static void
hack_environment (state *s)
{
  static const char *def_path =
# ifdef DEFAULT_PATH_PREFIX
    DEFAULT_PATH_PREFIX;
# else
    "";
# endif

  Display *dpy = GDK_DISPLAY();
  const char *odpy = DisplayString (dpy);
  char *ndpy = (char *) malloc(strlen(odpy) + 20);
  strcpy (ndpy, "DISPLAY=");
  strcat (ndpy, odpy);
  if (putenv (ndpy))
    abort ();

  if (s->debug_p)
    fprintf (stderr, "%s: %s\n", blurb(), ndpy);

  /* don't free(ndpy) -- some implementations of putenv (BSD 4.4, glibc
     2.0) copy the argument, but some (libc4,5, glibc 2.1.2) do not.
     So we must leak it (and/or the previous setting).  Yay.
   */

  if (def_path && *def_path)
    {
      const char *opath = getenv("PATH");
      char *npath = (char *) malloc(strlen(def_path) + strlen(opath) + 20);
      strcpy (npath, "PATH=");
      strcat (npath, def_path);
      strcat (npath, ":");
      strcat (npath, opath);

      if (putenv (npath))
	abort ();
      /* do not free(npath) -- see above */

      if (s->debug_p)
        fprintf (stderr, "%s: added \"%s\" to $PATH\n", blurb(), def_path);
    }
}


static void
hack_subproc_environment (Window preview_window_id, Bool debug_p)
{
  /* Store a window ID in $XSCREENSAVER_WINDOW -- this isn't strictly
     necessary yet, but it will make programs work if we had invoked
     them with "-root" and not with "-window-id" -- which, of course,
     doesn't happen.
   */
  char *nssw = (char *) malloc (40);
  sprintf (nssw, "XSCREENSAVER_WINDOW=0x%X", (unsigned int) preview_window_id);

  /* Allegedly, BSD 4.3 didn't have putenv(), but nobody runs such systems
     any more, right?  It's not Posix, but everyone seems to have it. */
  if (putenv (nssw))
    abort ();

  if (debug_p)
    fprintf (stderr, "%s: %s\n", blurb(), nssw);

  /* do not free(nssw) -- see above */
}


/* Called from a timer:
   Launches the currently-chosen subprocess, if it's not already running.
   If there's a different process running, kills it.
 */
static int
update_subproc_timer (gpointer data)
{
  state *s = (state *) data;
  if (! s->desired_preview_cmd)
    kill_preview_subproc (s, True);
  else if (!s->running_preview_cmd ||
           !!strcmp (s->desired_preview_cmd, s->running_preview_cmd))
    launch_preview_subproc (s);

  s->subproc_timer_id = 0;
  return FALSE;  /* do not re-execute timer */
}


/* Call this when you think you might want a preview process running.
   It will set a timer that will actually launch that program a second
   from now, if you haven't changed your mind (to avoid double-click
   spazzing, etc.)  `cmd' may be null meaning "no process".
 */
static void
schedule_preview (state *s, const char *cmd)
{
  int delay = 1000 * 0.5;   /* 1/2 second hysteresis */

  if (s->debug_p)
    {
      if (cmd)
        fprintf (stderr, "%s: scheduling preview \"%s\"\n", blurb(), cmd);
      else
        fprintf (stderr, "%s: scheduling preview death\n", blurb());
    }

  if (s->desired_preview_cmd) free (s->desired_preview_cmd);
  s->desired_preview_cmd = (cmd ? strdup (cmd) : 0);

  if (s->subproc_timer_id)
    gtk_timeout_remove (s->subproc_timer_id);
  s->subproc_timer_id = gtk_timeout_add (delay, update_subproc_timer, s);
}


/* Called from a timer:
   Checks to see if the subproc that should be running, actually is.
 */
static int
check_subproc_timer (gpointer data)
{
  state *s = (state *) data;
  Bool again_p = True;

  if (s->running_preview_error_p ||   /* already dead */
      s->running_preview_pid <= 0)
    {
      again_p = False;
    }
  else
    {
      int status;
      reap_zombies (s);
      status = kill (s->running_preview_pid, 0);
      if (status < 0 && errno == ESRCH)
        s->running_preview_error_p = True;

      if (s->debug_p)
        {
          char *ss = subproc_pretty_name (s);
          fprintf (stderr, "%s: timer: pid %lu (%s) is %s\n", blurb(),
                   (unsigned long) s->running_preview_pid, ss,
                   (s->running_preview_error_p ? "dead" : "alive"));
          free (ss);
        }

      if (s->running_preview_error_p)
        {
          clear_preview_window (s);
          again_p = False;
        }
    }

  /* Otherwise, it's currently alive.  We might be checking again, or we
     might be satisfied. */

  if (--s->subproc_check_countdown <= 0)
    again_p = False;

  if (again_p)
    return TRUE;     /* re-execute timer */
  else
    {
      s->subproc_check_timer_id = 0;
      s->subproc_check_countdown = 0;
      return FALSE;  /* do not re-execute timer */
    }
}


/* Call this just after launching a subprocess.
   This sets a timer that will, five times a second for two seconds,
   check whether the program is still running.  The assumption here
   is that if the process didn't stay up for more than a couple of
   seconds, then either the program doesn't exist, or it doesn't
   take a -window-id argument.
 */
static void
schedule_preview_check (state *s)
{
  int seconds = 2;
  int ticks = 5;

  if (s->debug_p)
    fprintf (stderr, "%s: scheduling check\n", blurb());

  if (s->subproc_check_timer_id)
    gtk_timeout_remove (s->subproc_check_timer_id);
  s->subproc_check_timer_id =
    gtk_timeout_add (1000 / ticks,
                     check_subproc_timer, (gpointer) s);
  s->subproc_check_countdown = ticks * seconds;
}


static Bool
screen_blanked_p (void)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  unsigned char *dataP = 0;
  Display *dpy = GDK_DISPLAY();
  Bool blanked_p = False;

  if (XGetWindowProperty (dpy, RootWindow (dpy, 0), /* always screen #0 */
                          XA_SCREENSAVER_STATUS,
                          0, 3, False, XA_INTEGER,
                          &type, &format, &nitems, &bytesafter,
                          &dataP)
      == Success
      && type == XA_INTEGER
      && nitems >= 3
      && dataP)
    {
      Atom *data = (Atom *) dataP;
      blanked_p = (data[0] == XA_BLANK || data[0] == XA_LOCK);
    }

  if (dataP) XFree (dataP);

  return blanked_p;
}

/* Wake up every now and then and see if the screen is blanked.
   If it is, kill off the small-window demo -- no point in wasting
   cycles by running two screensavers at once...
 */
static int
check_blanked_timer (gpointer data)
{
  state *s = (state *) data;
  Bool blanked_p = screen_blanked_p ();
  if (blanked_p && s->running_preview_pid)
    {
      if (s->debug_p)
        fprintf (stderr, "%s: screen is blanked: killing preview\n", blurb());
      kill_preview_subproc (s, True);
    }

  return True;  /* re-execute timer */
}


/* How many screens are there (including Xinerama.)
 */
static int
screen_count (Display *dpy)
{
  int nscreens = ScreenCount(dpy);
# ifdef HAVE_XINERAMA
  if (nscreens <= 1)
    {
      int event_number, error_number;
      if (XineramaQueryExtension (dpy, &event_number, &error_number) &&
          XineramaIsActive (dpy))
        {
          XineramaScreenInfo *xsi = XineramaQueryScreens (dpy, &nscreens);
          if (xsi) XFree (xsi);
        }
    }
# endif /* HAVE_XINERAMA */

  return nscreens;
}


/* Setting window manager icon
 */

static void
init_icon (GdkWindow *window)
{
  GdkBitmap *mask = 0;
  GdkColor transp;
  GdkPixmap *pixmap =
    gdk_pixmap_create_from_xpm_d (window, &mask, &transp,
                                  (gchar **) logo_50_xpm);
  if (pixmap)
    gdk_window_set_icon (window, 0, pixmap, mask);
}


/* The main demo-mode command loop.
 */

#if 0
static Bool
mapper (XrmDatabase *db, XrmBindingList bindings, XrmQuarkList quarks,
	XrmRepresentation *type, XrmValue *value, XPointer closure)
{
  int i;
  for (i = 0; quarks[i]; i++)
    {
      if (bindings[i] == XrmBindTightly)
	fprintf (stderr, (i == 0 ? "" : "."));
      else if (bindings[i] == XrmBindLoosely)
	fprintf (stderr, "*");
      else
	fprintf (stderr, " ??? ");
      fprintf(stderr, "%s", XrmQuarkToString (quarks[i]));
    }

  fprintf (stderr, ": %s\n", (char *) value->addr);

  return False;
}
#endif


static Window
gnome_screensaver_window (Screen *screen)
{
  Display *dpy = DisplayOfScreen (screen);
  Window root = RootWindowOfScreen (screen);
  Window parent, *kids;
  unsigned int nkids;
  Window gnome_window = 0;
  int i;

  if (! XQueryTree (dpy, root, &root, &parent, &kids, &nkids))
    abort ();
  for (i = 0; i < nkids; i++)
    {
      Atom type;
      int format;
      unsigned long nitems, bytesafter;
      unsigned char *name;
      if (XGetWindowProperty (dpy, kids[i], XA_WM_COMMAND, 0, 128,
                              False, XA_STRING, &type, &format, &nitems,
                              &bytesafter, &name)
          == Success
          && type != None
          && !strcmp ((char *) name, "gnome-screensaver"))
	{
	  gnome_window = kids[i];
          break;
	}
    }

  if (kids) XFree ((char *) kids);
  return gnome_window;
}

static Bool
gnome_screensaver_active_p (void)
{
  Display *dpy = GDK_DISPLAY();
  Window w = gnome_screensaver_window (DefaultScreenOfDisplay (dpy));
  return (w ? True : False);
}

static void
kill_gnome_screensaver (void)
{
  Display *dpy = GDK_DISPLAY();
  Window w = gnome_screensaver_window (DefaultScreenOfDisplay (dpy));
  if (w) XKillClient (dpy, (XID) w);
}

static Bool
kde_screensaver_active_p (void)
{
  FILE *p = popen ("dcop kdesktop KScreensaverIface isEnabled 2>/dev/null",
                   "r");
  char buf[255];
  fgets (buf, sizeof(buf)-1, p);
  pclose (p);
  if (!strcmp (buf, "true\n"))
    return True;
  else
    return False;
}

static void
kill_kde_screensaver (void)
{
  system ("dcop kdesktop KScreensaverIface enable false");
}


static void
the_network_is_not_the_computer (state *s)
{
  Display *dpy = GDK_DISPLAY();
  char *rversion = 0, *ruser = 0, *rhost = 0;
  char *luser, *lhost;
  char *msg = 0;
  struct passwd *p = getpwuid (getuid ());
  const char *d = DisplayString (dpy);

# if defined(HAVE_UNAME)
  struct utsname uts;
  if (uname (&uts) < 0)
    lhost = "<UNKNOWN>";
  else
    lhost = uts.nodename;
# elif defined(VMS)
  strcpy (lhost, getenv("SYS$NODE"));
# else  /* !HAVE_UNAME && !VMS */
  strcat (lhost, "<UNKNOWN>");
# endif /* !HAVE_UNAME && !VMS */

  if (p && p->pw_name)
    luser = p->pw_name;
  else
    luser = "???";

  server_xscreensaver_version (dpy, &rversion, &ruser, &rhost);

  /* Make a buffer that's big enough for a number of copies of all the
     strings, plus some. */
  msg = (char *) malloc (10 * ((rversion ? strlen(rversion) : 0) +
			       (ruser ? strlen(ruser) : 0) +
			       (rhost ? strlen(rhost) : 0) +
			       strlen(lhost) +
			       strlen(luser) +
			       strlen(d) +
			       1024));
  *msg = 0;

  if (!rversion || !*rversion)
    {
      sprintf (msg,
	       _("Warning:\n\n"
		 "The XScreenSaver daemon doesn't seem to be running\n"
		 "on display \"%s\".  Launch it now?"),
	       d);
    }
  else if (p && ruser && *ruser && !!strcmp (ruser, p->pw_name))
    {
      /* Warn that the two processes are running as different users.
       */
      sprintf(msg,
	    _("Warning:\n\n"
	      "%s is running as user \"%s\" on host \"%s\".\n"
	      "But the xscreensaver managing display \"%s\"\n"
	      "is running as user \"%s\" on host \"%s\".\n"
	      "\n"
	      "Since they are different users, they won't be reading/writing\n"
	      "the same ~/.xscreensaver file, so %s isn't\n"
	      "going to work right.\n"
	      "\n"
	      "You should either re-run %s as \"%s\", or re-run\n"
	      "xscreensaver as \"%s\".\n"
              "\n"
              "Restart the xscreensaver daemon now?\n"),
	      progname, luser, lhost,
	      d,
	      (ruser ? ruser : "???"), (rhost ? rhost : "???"),
	      progname,
	      progname, (ruser ? ruser : "???"),
	      luser);
    }
  else if (rhost && *rhost && !!strcmp (rhost, lhost))
    {
      /* Warn that the two processes are running on different hosts.
       */
      sprintf (msg,
	      _("Warning:\n\n"
	       "%s is running as user \"%s\" on host \"%s\".\n"
	       "But the xscreensaver managing display \"%s\"\n"
	       "is running as user \"%s\" on host \"%s\".\n"
	       "\n"
	       "If those two machines don't share a file system (that is,\n"
	       "if they don't see the same ~%s/.xscreensaver file) then\n"
	       "%s won't work right.\n"
               "\n"
               "Restart the daemon on \"%s\" as \"%s\" now?\n"),
	       progname, luser, lhost,
	       d,
	       (ruser ? ruser : "???"), (rhost ? rhost : "???"),
	       luser,
	       progname,
               lhost, luser);
    }
  else if (!!strcmp (rversion, s->short_version))
    {
      /* Warn that the version numbers don't match.
       */
      sprintf (msg,
	     _("Warning:\n\n"
	       "This is %s version %s.\n"
	       "But the xscreensaver managing display \"%s\"\n"
	       "is version %s.  This could cause problems.\n"
	       "\n"
	       "Restart the xscreensaver daemon now?\n"),
	       progname, s->short_version,
	       d,
	       rversion);
    }


  if (*msg)
    warning_dialog (s->toplevel_widget, msg, D_LAUNCH, 1);

  if (rversion) free (rversion);
  if (ruser) free (ruser);
  if (rhost) free (rhost);
  free (msg);
  msg = 0;

  /* Note: since these dialogs are not modal, they will stack up.
     So we do this check *after* popping up the "xscreensaver is not
     running" dialog so that these are on top.  Good enough.
   */

  if (gnome_screensaver_active_p ())
    warning_dialog (s->toplevel_widget,
                    _("Warning:\n\n"
                      "The GNOME screensaver daemon appears to be running.\n"
                      "It must be stopped for XScreenSaver to work properly.\n"
                      "\n"
                      "Stop the GNOME screen saver daemon now?\n"),
                    D_GNOME, 1);

  if (kde_screensaver_active_p ())
    warning_dialog (s->toplevel_widget,
                    _("Warning:\n\n"
                      "The KDE screen saver daemon appears to be running.\n"
                      "It must be stopped for XScreenSaver to work properly.\n"
                      "\n"
                      "Stop the KDE screen saver daemon now?\n"),
                    D_KDE, 1);
}


/* We use this error handler so that X errors are preceeded by the name
   of the program that generated them.
 */
static int
demo_ehandler (Display *dpy, XErrorEvent *error)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  fprintf (stderr, "\nX error in %s:\n", blurb());
  XmuPrintDefaultErrorMessage (dpy, error, stderr);
  kill_preview_subproc (s, False);
  exit (-1);
  return 0;
}


/* We use this error handler so that Gtk/Gdk errors are preceeded by the name
   of the program that generated them; and also that we can ignore one
   particular bogus error message that Gdk madly spews.
 */
static void
g_log_handler (const gchar *log_domain, GLogLevelFlags log_level,
               const gchar *message, gpointer user_data)
{
  /* Ignore the message "Got event for unknown window: 0x...".
     Apparently some events are coming in for the xscreensaver window
     (presumably reply events related to the ClientMessage) and Gdk
     feels the need to complain about them.  So, just suppress any
     messages that look like that one.
   */
  if (strstr (message, "unknown window"))
    return;

  fprintf (stderr, "%s: %s-%s: %s%s", blurb(),
           (log_domain ? log_domain : progclass),
           (log_level == G_LOG_LEVEL_ERROR    ? "error" :
            log_level == G_LOG_LEVEL_CRITICAL ? "critical" :
            log_level == G_LOG_LEVEL_WARNING  ? "warning" :
            log_level == G_LOG_LEVEL_MESSAGE  ? "message" :
            log_level == G_LOG_LEVEL_INFO     ? "info" :
            log_level == G_LOG_LEVEL_DEBUG    ? "debug" : "???"),
           message,
           ((!*message || message[strlen(message)-1] != '\n')
            ? "\n" : ""));
}


#ifdef __GNUC__
 __extension__     /* shut up about "string length is greater than the length
                      ISO C89 compilers are required to support" when including
                      the .ad file... */
#endif

STFU
static char *defaults[] = {
#include "XScreenSaver_ad.h"
 0
};

#if 0
#ifdef HAVE_CRAPPLET
static struct poptOption crapplet_options[] = {
  {NULL, '\0', 0, NULL, 0}
};
#endif /* HAVE_CRAPPLET */
#endif /* 0 */

const char *usage = "[--display dpy] [--prefs]"
# ifdef HAVE_CRAPPLET
                    " [--crapplet]"
# endif
            "\n\t\t   [--debug] [--sync] [--no-xshm] [--configdir dir]";

static void
map_popup_window_cb (GtkWidget *w, gpointer user_data)
{
  state *s = (state *) user_data;
  Boolean oi = s->initializing_p;
#ifndef HAVE_GTK2
  GtkLabel *label = GTK_LABEL (name_to_widget (s, "doc"));
#endif
  s->initializing_p = True;
#ifndef HAVE_GTK2
  eschew_gtk_lossage (label);
#endif
  s->initializing_p = oi;
}


#if 0
static void
print_widget_tree (GtkWidget *w, int depth)
{
  int i;
  for (i = 0; i < depth; i++)
    fprintf (stderr, "  ");
  fprintf (stderr, "%s\n", gtk_widget_get_name (w));

  if (GTK_IS_LIST (w))
    {
      for (i = 0; i < depth+1; i++)
        fprintf (stderr, "  ");
      fprintf (stderr, "...list kids...\n");
    }
  else if (GTK_IS_CONTAINER (w))
    {
      GList *kids = gtk_container_children (GTK_CONTAINER (w));
      while (kids)
        {
          print_widget_tree (GTK_WIDGET (kids->data), depth+1);
          kids = kids->next;
        }
    }
}
#endif /* 0 */

static int
delayed_scroll_kludge (gpointer data)
{
  state *s = (state *) data;
  GtkWidget *w = GTK_WIDGET (name_to_widget (s, "list"));
  ensure_selected_item_visible (w);

  /* Oh, this is just fucking lovely, too. */
  w = GTK_WIDGET (name_to_widget (s, "preview"));
  gtk_widget_hide (w);
  gtk_widget_show (w);

  return FALSE;  /* do not re-execute timer */
}

#ifdef HAVE_GTK2

GtkWidget *
create_xscreensaver_demo (void)
{
  GtkWidget *nb;

  nb = name_to_widget (global_state_kludge, "preview_notebook");
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), FALSE);

  return name_to_widget (global_state_kludge, "xscreensaver_demo");
}

GtkWidget *
create_xscreensaver_settings_dialog (void)
{
  GtkWidget *w, *box;

  box = name_to_widget (global_state_kludge, "dialog_action_area");

  w = name_to_widget (global_state_kludge, "adv_button");
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (box), w, TRUE);

  w = name_to_widget (global_state_kludge, "std_button");
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (box), w, TRUE);

  return name_to_widget (global_state_kludge, "xscreensaver_settings_dialog");
}

#endif /* HAVE_GTK2 */

int
main (int argc, char **argv)
{
  XtAppContext app;
  state S, *s;
  saver_preferences *p;
  Bool prefs = False;
  int i;
  Display *dpy;
  Widget toplevel_shell;
  char *real_progname = argv[0];
  char *window_title;
  char *geom = 0;
  Bool crapplet_p = False;
  char *str;

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  textdomain (GETTEXT_PACKAGE);

# ifdef HAVE_GTK2
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
# else  /* !HAVE_GTK2 */
  if (!setlocale (LC_ALL, ""))
    fprintf (stderr, "%s: locale not supported by C library\n", real_progname);
# endif /* !HAVE_GTK2 */

#endif /* ENABLE_NLS */

  str = strrchr (real_progname, '/');
  if (str) real_progname = str+1;

  s = &S;
  memset (s, 0, sizeof(*s));
  s->initializing_p = True;
  p = &s->prefs;

  global_state_kludge = s;  /* I hate C so much... */

  progname = real_progname;

  s->short_version = (char *) malloc (5);
  memcpy (s->short_version, screensaver_id + 17, 4);
  s->short_version [4] = 0;


  /* Register our error message logger for every ``log domain'' known.
     There's no way to do this globally, so I grepped the Gtk/Gdk sources
     for all of the domains that seem to be in use.
  */
  {
    const char * const domains[] = { 0,
                                     "Gtk", "Gdk", "GLib", "GModule",
                                     "GThread", "Gnome", "GnomeUI" };
    for (i = 0; i < countof(domains); i++)
      g_log_set_handler (domains[i], G_LOG_LEVEL_MASK, g_log_handler, 0);
  }

#ifdef DEFAULT_ICONDIR  /* from -D on compile line */
# ifndef HAVE_GTK2
  {
    const char *dir = DEFAULT_ICONDIR;
    if (*dir) add_pixmap_directory (dir);
  }
# endif /* !HAVE_GTK2 */
#endif /* DEFAULT_ICONDIR */

  /* This is gross, but Gtk understands --display and not -display...
   */
  for (i = 1; i < argc; i++)
    if (argv[i][0] && argv[i][1] && 
        !strncmp(argv[i], "-display", strlen(argv[i])))
      argv[i] = "--display";


  /* We need to parse this arg really early... Sigh. */
  for (i = 1; i < argc; i++)
    {
      if (argv[i] &&
          (!strcmp(argv[i], "--crapplet") ||
           !strcmp(argv[i], "--capplet")))
        {
# if defined(HAVE_CRAPPLET) || defined(HAVE_GTK2)
          int j;
          crapplet_p = True;
          for (j = i; j < argc; j++)  /* remove it from the list */
            argv[j] = argv[j+1];
          argc--;
# else  /* !HAVE_CRAPPLET && !HAVE_GTK2 */
          fprintf (stderr, "%s: not compiled with --crapplet support\n",
                   real_progname);
          fprintf (stderr, "%s: %s\n", real_progname, usage);
          exit (1);
# endif /* !HAVE_CRAPPLET && !HAVE_GTK2 */
        }
      else if (argv[i] &&
               (!strcmp(argv[i], "--debug") ||
                !strcmp(argv[i], "-debug") ||
                !strcmp(argv[i], "-d")))
        {
          int j;
          s->debug_p = True;
          for (j = i; j < argc; j++)  /* remove it from the list */
            argv[j] = argv[j+1];
          argc--;
          i--;
        }
      else if (argv[i] &&
               argc > i+1 &&
               *argv[i+1] &&
               (!strcmp(argv[i], "-geometry") ||
                !strcmp(argv[i], "-geom") ||
                !strcmp(argv[i], "-geo") ||
                !strcmp(argv[i], "-g")))
        {
          int j;
          geom = argv[i+1];
          for (j = i; j < argc; j++)  /* remove them from the list */
            argv[j] = argv[j+2];
          argc -= 2;
          i -= 2;
        }
      else if (argv[i] &&
               argc > i+1 &&
               *argv[i+1] &&
               (!strcmp(argv[i], "--configdir")))
        {
          int j;
          struct stat st;
          hack_configuration_path = argv[i+1];
          for (j = i; j < argc; j++)  /* remove them from the list */
            argv[j] = argv[j+2];
          argc -= 2;
          i -= 2;

          if (0 != stat (hack_configuration_path, &st))
            {
              char buf[255];
              sprintf (buf, "%s: %.200s", blurb(), hack_configuration_path);
              perror (buf);
              exit (1);
            }
          else if (!S_ISDIR (st.st_mode))
            {
              fprintf (stderr, "%s: not a directory: %s\n",
                       blurb(), hack_configuration_path);
              exit (1);
            }
        }
    }


  if (s->debug_p)
    fprintf (stderr, "%s: using config directory \"%s\"\n",
             progname, hack_configuration_path);


  /* Let Gtk open the X connection, then initialize Xt to use that
     same connection.  Doctor Frankenstein would be proud.
   */
# ifdef HAVE_CRAPPLET
  if (crapplet_p)
    {
      GnomeClient *client;
      GnomeClientFlags flags = 0;

      int init_results = gnome_capplet_init ("screensaver-properties",
                                             s->short_version,
                                             argc, argv, NULL, 0, NULL);
      /* init_results is:
         0 upon successful initialization;
         1 if --init-session-settings was passed on the cmdline;
         2 if --ignore was passed on the cmdline;
        -1 on error.

         So the 1 signifies just to init the settings, and quit, basically.
         (Meaning launch the xscreensaver daemon.)
       */

      if (init_results < 0)
        {
#  if 0
          g_error ("An initialization error occurred while "
                   "starting xscreensaver-capplet.\n");
#  else  /* !0 */
          fprintf (stderr, "%s: gnome_capplet_init failed: %d\n",
                   real_progname, init_results);
          exit (1);
#  endif /* !0 */
        }

      client = gnome_master_client ();

      if (client)
        flags = gnome_client_get_flags (client);

      if (flags & GNOME_CLIENT_IS_CONNECTED)
        {
          int token =
            gnome_startup_acquire_token ("GNOME_SCREENSAVER_PROPERTIES",
                                         gnome_client_get_id (client));
          if (token)
            {
              char *session_args[20];
              int i = 0;
              session_args[i++] = real_progname;
              session_args[i++] = "--capplet";
              session_args[i++] = "--init-session-settings";
              session_args[i] = 0;
              gnome_client_set_priority (client, 20);
              gnome_client_set_restart_style (client, GNOME_RESTART_ANYWAY);
              gnome_client_set_restart_command (client, i, session_args);
            }
          else
            {
              gnome_client_set_restart_style (client, GNOME_RESTART_NEVER);
            }

          gnome_client_flush (client);
        }

      if (init_results == 1)
	{
	  system ("xscreensaver -nosplash &");
	  return 0;
	}

    }
  else
# endif /* HAVE_CRAPPLET */
    {
      gtk_init (&argc, &argv);
    }


  /* We must read exactly the same resources as xscreensaver.
     That means we must have both the same progclass *and* progname,
     at least as far as the resource database is concerned.  So,
     put "xscreensaver" in argv[0] while initializing Xt.
   */
  argv[0] = "xscreensaver";
  progname = argv[0];


  /* Teach Xt to use the Display that Gtk/Gdk have already opened.
   */
  XtToolkitInitialize ();
  app = XtCreateApplicationContext ();
  dpy = GDK_DISPLAY();
  XtAppSetFallbackResources (app, defaults);
  XtDisplayInitialize (app, dpy, progname, progclass, 0, 0, &argc, argv);
  toplevel_shell = XtAppCreateShell (progname, progclass,
                                     applicationShellWidgetClass,
                                     dpy, 0, 0);

  dpy = XtDisplay (toplevel_shell);
  db = XtDatabase (dpy);
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);
  XSetErrorHandler (demo_ehandler);

  /* Let's just ignore these.  They seem to confuse Irix Gtk... */
  signal (SIGPIPE, SIG_IGN);

  /* After doing Xt-style command-line processing, complain about any
     unrecognized command-line arguments.
   */
  for (i = 1; i < argc; i++)
    {
      char *str = argv[i];
      if (str[0] == '-' && str[1] == '-')
	str++;
      if (!strcmp (str, "-prefs"))
	prefs = True;
      else if (crapplet_p)
        /* There are lots of random args that we don't care about when we're
           started as a crapplet, so just ignore unknown args in that case. */
        ;
      else
	{
	  fprintf (stderr, _("%s: unknown option: %s\n"), real_progname,
                   argv[i]);
          fprintf (stderr, "%s: %s\n", real_progname, usage);
          exit (1);
	}
    }

  /* Load the init file, which may end up consulting the X resource database
     and the site-wide app-defaults file.  Note that at this point, it's
     important that `progname' be "xscreensaver", rather than whatever
     was in argv[0].
   */
  p->db = db;
  s->nscreens = screen_count (dpy);

  hack_environment (s);  /* must be before initialize_sort_map() */

  load_init_file (dpy, p);
  initialize_sort_map (s);

  /* Now that Xt has been initialized, and the resources have been read,
     we can set our `progname' variable to something more in line with
     reality.
   */
  progname = real_progname;


#if 0
  /* Print out all the resources we read. */
  {
    XrmName name = { 0 };
    XrmClass class = { 0 };
    int count = 0;
    XrmEnumerateDatabase (db, &name, &class, XrmEnumAllLevels, mapper,
			  (POINTER) &count);
  }
#endif


  /* Intern the atoms that xscreensaver_command() needs.
   */
  XA_VROOT = XInternAtom (dpy, "__SWM_VROOT", False);
  XA_SCREENSAVER = XInternAtom (dpy, "SCREENSAVER", False);
  XA_SCREENSAVER_VERSION = XInternAtom (dpy, "_SCREENSAVER_VERSION",False);
  XA_SCREENSAVER_STATUS = XInternAtom (dpy, "_SCREENSAVER_STATUS", False);
  XA_SCREENSAVER_ID = XInternAtom (dpy, "_SCREENSAVER_ID", False);
  XA_SCREENSAVER_RESPONSE = XInternAtom (dpy, "_SCREENSAVER_RESPONSE", False);
  XA_SELECT = XInternAtom (dpy, "SELECT", False);
  XA_DEMO = XInternAtom (dpy, "DEMO", False);
  XA_ACTIVATE = XInternAtom (dpy, "ACTIVATE", False);
  XA_BLANK = XInternAtom (dpy, "BLANK", False);
  XA_LOCK = XInternAtom (dpy, "LOCK", False);
  XA_EXIT = XInternAtom (dpy, "EXIT", False);
  XA_RESTART = XInternAtom (dpy, "RESTART", False);


  /* Create the window and all its widgets.
   */
  s->base_widget     = create_xscreensaver_demo ();
  s->popup_widget    = create_xscreensaver_settings_dialog ();
  s->toplevel_widget = s->base_widget;


  /* Set the main window's title. */
  {
    char *base_title = _("Screensaver Preferences");
    char *v = (char *) strdup(strchr(screensaver_id, ' '));
    char *s1, *s2, *s3, *s4;
    s1 = (char *) strchr(v,  ' '); s1++;
    s2 = (char *) strchr(s1, ' ');
    s3 = (char *) strchr(v,  '('); s3++;
    s4 = (char *) strchr(s3, ')');
    *s2 = 0;
    *s4 = 0;

    window_title = (char *) malloc (strlen (base_title) +
                                    strlen (progclass) +
                                    strlen (s1) + strlen (s3) +
                                    100);
    sprintf (window_title, "%s  (%s %s, %s)", base_title, progclass, s1, s3);
    gtk_window_set_title (GTK_WINDOW (s->toplevel_widget), window_title);
    gtk_window_set_title (GTK_WINDOW (s->popup_widget),    window_title);
    free (v);
  }

  /* Adjust the (invisible) notebooks on the popup dialog... */
  {
    GtkNotebook *notebook =
      GTK_NOTEBOOK (name_to_widget (s, "opt_notebook"));
    GtkWidget *std = GTK_WIDGET (name_to_widget (s, "std_button"));
    int page = 0;

# ifdef HAVE_XML
    gtk_widget_hide (std);
# else  /* !HAVE_XML */
    /* Make the advanced page be the only one available. */
    gtk_widget_set_sensitive (std, False);
    std = GTK_WIDGET (name_to_widget (s, "adv_button"));
    gtk_widget_hide (std);
    page = 1;
# endif /* !HAVE_XML */

    gtk_notebook_set_page (notebook, page);
    gtk_notebook_set_show_tabs (notebook, False);
  }

  /* Various other widget initializations...
   */
  gtk_signal_connect (GTK_OBJECT (s->toplevel_widget), "delete_event",
                      GTK_SIGNAL_FUNC (wm_toplevel_close_cb),
                      (gpointer) s);
  gtk_signal_connect (GTK_OBJECT (s->popup_widget), "delete_event",
                      GTK_SIGNAL_FUNC (wm_popup_close_cb),
                      (gpointer) s);

  populate_hack_list (s);
  populate_prefs_page (s);
  sensitize_demo_widgets (s, False);
  fix_text_entry_sizes (s);
  scroll_to_current_hack (s);

  gtk_signal_connect (GTK_OBJECT (name_to_widget (s, "cancel_button")),
                      "map", GTK_SIGNAL_FUNC(map_popup_window_cb),
                      (gpointer) s);

#ifndef HAVE_GTK2
  gtk_signal_connect (GTK_OBJECT (name_to_widget (s, "prev")),
                      "map", GTK_SIGNAL_FUNC(map_prev_button_cb),
                      (gpointer) s);
  gtk_signal_connect (GTK_OBJECT (name_to_widget (s, "next")),
                      "map", GTK_SIGNAL_FUNC(map_next_button_cb),
                      (gpointer) s);
#endif /* !HAVE_GTK2 */

  /* Hook up callbacks to the items on the mode menu. */
  {
    GtkOptionMenu *opt = GTK_OPTION_MENU (name_to_widget (s, "mode_menu"));
    GtkMenu *menu = GTK_MENU (gtk_option_menu_get_menu (opt));
    GList *kids = gtk_container_children (GTK_CONTAINER (menu));
    int i;
    for (i = 0; kids; kids = kids->next, i++)
      {
        gtk_signal_connect (GTK_OBJECT (kids->data), "activate",
                            GTK_SIGNAL_FUNC (mode_menu_item_cb),
                            (gpointer) s);

        /* The "random-same" mode menu item does not appear unless
           there are multple screens.
         */
        if (s->nscreens <= 1 &&
            mode_menu_order[i] == RANDOM_HACKS_SAME)
          gtk_widget_hide (GTK_WIDGET (kids->data));
      }

    if (s->nscreens <= 1)   /* recompute option-menu size */
      {
        gtk_widget_unrealize (GTK_WIDGET (menu));
        gtk_widget_realize (GTK_WIDGET (menu));
      }
  }


  /* Handle the -prefs command-line argument. */
  if (prefs)
    {
      GtkNotebook *notebook =
        GTK_NOTEBOOK (name_to_widget (s, "notebook"));
      gtk_notebook_set_page (notebook, 1);
    }

# ifdef HAVE_CRAPPLET
  if (crapplet_p)
    {
      GtkWidget *capplet;
      GtkWidget *outer_vbox;

      gtk_widget_hide (s->toplevel_widget);

      capplet = capplet_widget_new ();

      /* Make there be a "Close" button instead of "OK" and "Cancel" */
# ifdef HAVE_CRAPPLET_IMMEDIATE
      capplet_widget_changes_are_immediate (CAPPLET_WIDGET (capplet));
# endif /* HAVE_CRAPPLET_IMMEDIATE */
      /* In crapplet-mode, take off the menubar. */
      gtk_widget_hide (name_to_widget (s, "menubar"));

      /* Reparent our top-level container to be a child of the capplet
         window.
       */
      outer_vbox = GTK_BIN (s->toplevel_widget)->child;
      gtk_widget_ref (outer_vbox);
      gtk_container_remove (GTK_CONTAINER (s->toplevel_widget),
                            outer_vbox);
      STFU GTK_OBJECT_SET_FLAGS (outer_vbox, GTK_FLOATING);
      gtk_container_add (GTK_CONTAINER (capplet), outer_vbox);

      /* Find the window above us, and set the title and close handler. */
      {
        GtkWidget *window = capplet;
        while (window && !GTK_IS_WINDOW (window))
          window = window->parent;
        if (window)
          {
            gtk_window_set_title (GTK_WINDOW (window), window_title);
            gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                                GTK_SIGNAL_FUNC (wm_toplevel_close_cb),
                                (gpointer) s);
          }
      }

      s->toplevel_widget = capplet;
    }
# endif /* HAVE_CRAPPLET */


  /* The Gnome folks hate the menubar.  I think it's important to have access
     to the commands on the File menu (Restart Daemon, etc.) and to the
     About and Documentation commands on the Help menu.
   */
#if 0
#ifdef HAVE_GTK2
  gtk_widget_hide (name_to_widget (s, "menubar"));
#endif
#endif

  free (window_title);
  window_title = 0;

#ifdef HAVE_GTK2
  /* After picking the default size, allow -geometry to override it. */
  if (geom)
    gtk_window_parse_geometry (GTK_WINDOW (s->toplevel_widget), geom);
#endif

  gtk_widget_show (s->toplevel_widget);
  init_icon (GTK_WIDGET (s->toplevel_widget)->window);  /* after `show' */
  fix_preview_visual (s);

  /* Realize page zero, so that we can diddle the scrollbar when the
     user tabs back to it -- otherwise, the current hack isn't scrolled
     to the first time they tab back there, when started with "-prefs".
     (Though it is if they then tab away, and back again.)

     #### Bah!  This doesn't work.  Gtk eats my ass!  Someone who
     #### understands this crap, explain to me how to make this work.
  */
  gtk_widget_realize (name_to_widget (s, "demos_table"));


  gtk_timeout_add (60 * 1000, check_blanked_timer, s);


  /* Issue any warnings about the running xscreensaver daemon. */
  if (! s->debug_p)
    the_network_is_not_the_computer (s);


  /* Run the Gtk event loop, and not the Xt event loop.  This means that
     if there were Xt timers or fds registered, they would never get serviced,
     and if there were any Xt widgets, they would never have events delivered.
     Fortunately, we're using Gtk for all of the UI, and only initialized
     Xt so that we could process the command line and use the X resource
     manager.
   */
  s->initializing_p = False;

  /* This totally sucks -- set a timer that whacks the scrollbar 0.5 seconds
     after we start up.  Otherwise, it always appears scrolled to the top
     when in crapplet-mode. */
  gtk_timeout_add (500, delayed_scroll_kludge, s);


#if 1
  /* Load every configurator in turn, to scan them for errors all at once. */
  if (s->debug_p)
    {
      int i;
      for (i = 0; i < p->screenhacks_count; i++)
        {
          screenhack *hack = p->screenhacks[i];
          conf_data *d = load_configurator (hack->command, s->debug_p);
          if (d) free_conf_data (d);
        }
    }
#endif


# ifdef HAVE_CRAPPLET
  if (crapplet_p)
    capplet_gtk_main ();
  else
# endif /* HAVE_CRAPPLET */
    gtk_main ();

  kill_preview_subproc (s, False);
  exit (0);
}

#endif /* HAVE_GTK -- whole file */