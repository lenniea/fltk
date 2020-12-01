//
//      Simple example of an interactive Hex Editor using Fl_Table.
//      Uses Mr. Satan's technique of instancing an Fl_Input around.
//
// Copyright 1998-2010 by Bill Spitzak and others.
// Copyright 2020 by Lennie Araki
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Table.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Sys_Menu_Bar.H>

const int MAX_COLS = 100;
const int MAX_ROWS = 100;
const int COL_WIDTH = 10;
const int SEL_SIZE  = 2;
const int CELL_BORDER = 5;

////////////////////////////////////////////
//           R E S O U R C E S
////////////////////////////////////////////

#include "res/Folder.xpm"
#include "res/Save.xpm"

////////////////////////////////////////////
//               T Y P E S
////////////////////////////////////////////

typedef enum bwl_t {
  BWL_SIGNED = 0x10,
  BWL_BYTE = 0, 
  BWL_WORD = 1, 
  BWL_THREE = 2, 
  BWL_LONG = 3,
  BWL_SBYTE = BWL_SIGNED | BWL_BYTE, 
  BWL_SWORD = BWL_SIGNED | BWL_WORD, 
  BWL_STHREE = BWL_SIGNED | BWL_THREE, 
  BWL_SLONG = BWL_SIGNED | BWL_LONG,
  BWL_MASK = 0x03,
} BWL_T;

BWL_T bwl = BWL_WORD;
int cellsigned = 0;

int base = 16;

////////////////////////////////////////////
//          F U N C T I O N S
////////////////////////////////////////////

int parse_num(const char* buf) {
  int n = 0;
  // Parse (base n) number
  char ch;
  while ((ch = tolower(*buf++)) != '\0') {
    int digit;
    if (ch >= '0' && ch <= '9') {
      digit = ch - '0';
    } else if (ch >= 'a' && ch <= 'f') {
      digit = ch - 'a' + 10;
    } else {
      break;
    }
    n = (n * base) | digit;
  }
  return n;
}

int8_t iOctWidth[] = {
  3, 6, 9, 11
};
int8_t iDecWidth[] = {
  3, 5, 8, 10
};

int col_chars(void) {
  int n = (bwl & BWL_SIGNED) ? 2 : 1;
  if (base == 8) {
    return iOctWidth[bwl & BWL_MASK] + n;
  }
  if (base == 10) {
    return iDecWidth[bwl & BWL_MASK] + n;
  }
  return ((bwl & BWL_MASK) + 1) * 2 + n;
}

const char* szOctFormat[] = {
  "%03o", "%06o", "%08o", "%011o"
};

const char* szHexFormat[] = {
  "%02X", "%04X", "%06X", "%08X"
};

int format_num(char* buf, int n) {
  int len = 0;
  if (bwl & BWL_SIGNED && n < 0) {
    buf[len++] = '-';
    n = -n;
  }
  if (base == 8) {
    len += sprintf(buf + len, szOctFormat[bwl & BWL_MASK], n);
  } else if (base == 10) {
    len += sprintf(buf + len, "%u", n);
  } else {
    len += sprintf(buf + len, szHexFormat[bwl & BWL_MASK], n);
  }
  return len;
}

size_t auto_width(size_t count)  {
  if ((count % 200) == 0) {
    return 200;
  }
  if ((count % 416) == 0) {
    return 416;
  }
  if ((count % 640) == 0) {
    return 640;
  }
  return 32;
}

class HexGrid : public Fl_Table {
  Fl_Input *input;                    // single instance of Fl_Input widget
  uint8_t* values;                    // array of bytes for cells
  size_t count;
  int row_edit, col_edit;             // row/col being modified

protected:
  void draw_cell(TableContext context,int=0,int=0,int=0,int=0,int=0,int=0);
  void event_callback2();                               // table's event callback (instance)
  static void event_callback(Fl_Widget*,void *v) {      // table's event callback (static)
    ((HexGrid*)v)->event_callback2();
  }
  static void input_cb(Fl_Widget*,void* v) {            // input widget's callback
    ((HexGrid*)v)->set_value_hide();
  }

public:
  HexGrid(int X,int Y,int W,int H,const char* L = 0) : Fl_Table(X,Y,W,H,L) {
    callback(&event_callback, (void*)this);
    when(FL_WHEN_NOT_CHANGED|when());
    // Create input widget that we'll use whenever user clicks on a cell
    input = new Fl_Input(0,0,W,10);
    input->hide();                                      // hide until needed
    input->callback(input_cb, (void*)this);
    input->when(FL_WHEN_ENTER_KEY_ALWAYS);              // callback triggered when user hits Enter
    input->maximum_size(col_chars());
//    input->color(FL_YELLOW);                          // yellow to standout during editing
    values = NULL;
    count = 0;
    row_edit = col_edit = 0;
    set_selection(0,0,0,0);
  }

  void auto_shape(void) {
    // Calculate number of rows
    size_t row_bytes = auto_width(count);
    size_t bytes_per_col = (bwl & BWL_MASK) + 1;
    int width = row_bytes / bytes_per_col;
    cols(width);
    int height = (count + row_bytes - 1) / row_bytes;
    rows(height);
  }

  uint8_t* make_cells(size_t n) {
    // Delete previous buffer (if set)
    if (values != NULL) {
      delete [] values;
    }
    count = n;
    values = new uint8_t[count];
    auto_shape();
    return values;
  }

  int format_col(char* buf, int n) {
  #if defined(EXCEL_HEADERS)
    int len = (n < 26) ? sprintf(buf, "%c", 'A'+n)
            : sprintf(buf, "%c%c", 'A' + n/26 - 1, 'A' + n%26);
  #else
    size_t bytes_per_col= (bwl & BWL_MASK) + 1;
    int len = sprintf(buf, "%02lX", n * bytes_per_col);
  #endif
    return len;
  }

  int format_row(char* buf, int n) {
  #if defined(EXCEL_HEADERS)
    int len = sprintf(buf, "%u", n + 1);
  #else
    size_t bytes_per_col = (bwl & BWL_MASK) + 1;
    size_t row_bytes = bytes_per_col * cols();
    int len = sprintf(buf, "%08lX", n * row_bytes);
  #endif
    return len;
  }

  int open_file(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
      fl_alert("Can't open file '%s'", filename);
      return -1;
    }
    fseek(fp, 0L, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    uint8_t* buf = make_cells(len);
    size_t nread = fread(buf, 1, len, fp);
    fprintf(stderr, "Read %lu bytes\n", nread);
    if (nread != len) {
      fl_alert("Can't read %lu bytes", nread);
      return -2;
    }
    fclose(fp);
    table->end();
    table->redraw();
    return 0;
  }
  int save_file(const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) {
      fl_alert("Can't create file '%s'", filename);
      return -1;
    }
    size_t nwrite = fread(values, 1, count, fp);
    fprintf(stderr, "Write %lu bytes\n", nwrite);
    if (nwrite != count) {
      fl_alert("Can't write %lu bytes", nwrite);
      return -2;
    }
    fclose(fp);
    return 0;
  }
  int get_cell(int R, int C);
  void set_cell(int R, int C, int value);
  ~HexGrid() {
    delete [] values;
   }

  // Apply value from input widget to values[row][col] array and hide (done editing)
  void set_value_hide() {
    set_cell(row_edit,col_edit, parse_num(input->value()));
    input->hide();
    window()->cursor(FL_CURSOR_DEFAULT);                // XXX: if we don't do this, cursor can disappear!
  }
  // Start editing a new cell: move the Fl_Input widget to specified row/column
  //    Preload the widget with the cell's current value,
  //    and make the widget 'appear' at the cell's location.
  //
  void start_editing(int R, int C) {
    row_edit = R;                                       // Now editing this row/col
    col_edit = C;
//    set_selection(R,C,R,C);                             // Clear any previous multicell selection
    int X,Y,W,H;
    find_cell(CONTEXT_CELL, R,C, X,Y,W,H);              // Find X/Y/W/H of cell
    input->resize(X,Y,W,H);                             // Move Fl_Input widget there
    char s[30]; 
    int value = get_cell(R,C);
    format_num(s, value);
    input->value(s);
    input->position(0,strlen(s));                       // Select entire input field
    input->show();                                      // Show the input widget, now that we've positioned it
    input->take_focus();                                // Put keyboard focus into the input widget
    damage(FL_DAMAGE_ALL);
  }
  // Tell the input widget it's done editing, and to 'hide'
  void done_editing() {
    if (input->visible()) {                             // input widget visible, ie. edit in progress?
      set_value_hide();                                 // Transfer its current contents to cell and hide
      row_edit = 0;                                     // Disable these until needed again
      col_edit = 0;
    }
  }
};

HexGrid* table = NULL;

int HexGrid::get_cell(int R, int C) {
  int index = R * cols() + C;
  switch (bwl) {
    case BWL_BYTE:
      return ((uint8_t*) values)[index];
    case BWL_SBYTE:
      return ((int8_t*) values)[index];
    case BWL_WORD:
      return ((uint16_t*) values)[index];
    case BWL_SWORD:
      return ((int16_t*) values)[index];
    case BWL_LONG:
      return ((uint32_t*) values)[index];
    case BWL_SLONG:
      return ((int32_t*) values)[index];
    default:
      assert("Unsupported");
  }
  return 0;
}

void HexGrid::set_cell(int R, int C, int value) {
  int index = R * cols() + C;
  switch (bwl) {
    case BWL_BYTE:
    case BWL_SBYTE:
      ((int8_t*) values)[index] = value;
      break;
    case BWL_WORD:
    case BWL_SWORD:
      ((int16_t*) values)[index] = value;
      break;
    case BWL_LONG:
    case BWL_SLONG:
      ((uint32_t*) values)[index] = value;
      break;
    default:
      assert("Unsupported");
  }
}

// Handle drawing all cells in table
void HexGrid::draw_cell(TableContext context, int R,int C, int X,int Y,int W,int H) {
  static char s[30];
  switch ( context ) {
    case CONTEXT_STARTPAGE:                     // table about to redraw
      break;

    case CONTEXT_COL_HEADER:                    // table wants us to draw a column heading (C is column)
      fl_font(FL_HELVETICA | FL_BOLD, 14);      // set font for heading to bold
      fl_push_clip(X,Y,W,H);                    // clip region for text
      {
        fl_draw_box(FL_UP_BOX, X,Y,W,H, col_header_color());
        if (is_selected(row_edit,C)) {
          // draw selection on bottom edge
          fl_color(FL_DARK_BLUE);
          fl_rectf(X,Y+H-SEL_SIZE-1, W, SEL_SIZE);
        }
        fl_color(FL_BLACK);
        format_col(s, C);
        fl_draw(s, X,Y,W,H, FL_ALIGN_CENTER);
      }
      fl_pop_clip();
      return;

    case CONTEXT_ROW_HEADER:                    // table wants us to draw a row heading (R is row)
      fl_font(FL_HELVETICA | FL_BOLD, 14);      // set font for row heading to bold
      fl_push_clip(X,Y,W,H);
      {
        fl_draw_box(FL_UP_BOX, X,Y,W,H, row_header_color());
        if (is_selected(R,col_edit)) {
          // draw selection on right edge
          fl_color(FL_DARK_BLUE);
          fl_rectf(X+W-SEL_SIZE-1, Y, SEL_SIZE, H);
        }
        fl_color(FL_BLACK);
        format_row(s, R);
        fl_draw(s, X,Y,W,H, FL_ALIGN_CENTER);
      }
      fl_pop_clip();
      return;

    case CONTEXT_CELL: {                        // table wants us to draw a cell
      if (R == row_edit && C == col_edit && input->visible()) {
        return;                                 // dont draw for cell with input widget over it
      }
      // Background
      bool selected = is_selected(R,C);
      fl_draw_box(FL_THIN_UP_BOX, X,Y,W,H, selected ? FL_DARK_BLUE : FL_WHITE);

      // Text
      fl_push_clip(X+3, Y+3, W-6, H-6);
      {
        fl_color(selected ? FL_WHITE : FL_BLACK);
        fl_font(FL_HELVETICA, 14);            // ..in regular font
        format_num(s, get_cell(R,C));
        fl_draw(s, X+3,Y+3,W-6,H-6, FL_ALIGN_RIGHT);
      }
      fl_pop_clip();
      return;
    }

    case CONTEXT_RC_RESIZE:                     // table resizing rows or columns
      if ( input->visible() ) {
        find_cell(CONTEXT_TABLE, row_edit, col_edit, X, Y, W, H);
        input->resize(X,Y,W,H);
        init_sizes();
      }
      return;

    default:
      return;
  }
}

// Callback whenever someone clicks on different parts of the table
void HexGrid::event_callback2() {
  int R = callback_row();
  int C = callback_col();
  TableContext context = callback_context();

  switch ( context ) {
    case CONTEXT_CELL: {                                // A table event occurred on a cell
      switch (Fl::event()) {                            // see what FLTK event caused it
        case FL_PUSH:                                   // mouse click?
          done_editing();                               // finish editing previous
          if (R != rows()-1 && C != cols()-1 )          // only edit cells not in total's columns
            start_editing(R,C);                         // start new edit
          return;

        case FL_KEYBOARD:                               // key press in table?
          if ( Fl::event_key() == FL_Escape ) exit(0);  // ESC closes app
          done_editing();                               // finish any previous editing
          if (C==cols()-1 || R==rows()-1) return;       // no editing of totals column
          switch ( tolower(Fl::e_text[0]) ) {
            case '0': case '1': case '2': case '3':     // any of these should start editing new cell
            case '4': case '5': case '6': case '7':
            case '8': case '9': case '+': case '-':
            case 'a': case 'b': case 'c': case 'd': 
            case 'e': case 'f':
              start_editing(R,C);                       // start new edit
              input->handle(Fl::event());               // pass typed char to input
              break;
            case '\r': case '\n':                       // let enter key edit the cell
              start_editing(R,C);                       // start new edit
              break;
          }
          return;
      }
      return;
    }

    case CONTEXT_TABLE:                                 // A table event occurred on dead zone in table
    case CONTEXT_ROW_HEADER:                            // A table event occurred on row/column header
    case CONTEXT_COL_HEADER:
      done_editing();                                   // done editing, hide
      return;

    default:
      return;
  }
}

Fl_Native_File_Chooser g_chooser;

static void new_cb(Fl_Widget *, void *v) {
  // File/New: make a buffer of 16-bit words
  bwl = BWL_WORD;
  table->make_cells(MAX_ROWS * MAX_COLS * sizeof(uint16_t));
  for (int c = 0; c < MAX_COLS; c++) {
    for (int r = 0; r < MAX_ROWS; r++) {
      table->set_cell(r,c,c + (r*MAX_COLS));                // initialize cells
    }
  }
  table->end();
  table->redraw();
}

Fl_Double_Window* win = NULL;

static void open_cb(Fl_Widget *, void *v) {
  // Handle File/Open...
  g_chooser.directory(".");                                // directory to start browsing with
  g_chooser.filter("Binary files\t*.bin,*.raw\n");
  g_chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);     // only picks files that exist
  g_chooser.title("Pick a file please..");                 // custom title for chooser window
  if (g_chooser.show() == 0) {
    const char* filename = g_chooser.filename();
    if (table->open_file(filename) >= 0) {
      win->label(filename);
    }
  }
}
static void save_cb(Fl_Widget *, void *v) {
  const char* filename = g_chooser.filename();
  if ((long) v == 0 && filename != NULL) {
    // File Save (with valid filename)
    table->save_file(filename);
  } else {
    // File/Save As...
    g_chooser.directory(".");                                // directory to start browsing with
    g_chooser.filter("Binary files\t*.bin,*.raw\n");
    g_chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);     // only picks files that exist
    g_chooser.title("Save file as..");
    if (g_chooser.show() == 0) {
      filename = g_chooser.filename();
      table->save_file(filename);
    }
  }
}
#if !defined(__APPLE__)
static void quit_cb(Fl_Widget *, void *v) {
}
#endif
static void copy_cb(Fl_Widget *, void *v) {
  // Edit/Copy
  int top, left, bot, right;
  table->get_selection(top, left, bot, right);
  int width = right - left + 1;
  int height = bot - top + 1;
  int cells = width * height;
  fprintf(stderr, "copy %d x %d = %d cells\n", width, height, cells);
  char* buf = new char[cells * col_chars()+1];
  int len = 0;
  for (int row = top; row <= bot; ++row) {
    for (int col = left; col <= right; ++col) {
      int n = table->get_cell(row, col);
      len += format_num(buf + len, n);
      buf[len++] = (col == right) ? '\n' : '\t';
    }
  }
  buf[len] = '\0';
  Fl::copy(buf, len, 1);
  delete [] buf;
}
static void paste_cb(Fl_Widget *, void *v) {
  // TODO: handle paste from clipboard
}
static void view_cb(Fl_Widget *w, void *v) {
  switch ((long) v) {
    case BWL_BYTE:
    case BWL_WORD:
    case BWL_LONG:
      bwl = (BWL_T) ((bwl & ~BWL_MASK) | ((long) v & BWL_MASK));
      break;
    case BWL_SIGNED:
      bwl = (BWL_T) (bwl ^ BWL_SIGNED);
      break;
    default:
      assert("Invalid size");
  }
  int width = col_chars() * COL_WIDTH;
  table->col_width_all(width);
  table->auto_shape();
  table->redraw();
}
static void base_cb(Fl_Widget *, void *v) {
  long b = (long) v;
  switch (b) {
    case 8:
      base = b;
      break;
    case 10:
      base = b;
      break;
    case 16:
      base = b;
      break;
    default:
      assert("Invalid base");
  }
  table->redraw();
}

Fl_Menu_Item Main_Menu[] = {
{"&File",0,0,0,FL_SUBMENU},
  {"&New", FL_COMMAND+'n', new_cb, 0},
  {"&Open...", FL_COMMAND+'o', open_cb, 0},
  {"&Save", FL_COMMAND+'s', save_cb, 0},
  {"Save &As...", FL_COMMAND+FL_SHIFT+'s', save_cb, (void*)1, FL_MENU_DIVIDER},
//  {relative_history[0], FL_COMMAND+'0', open_history_cb, absolute_history[0]},
//  {relative_history[1], FL_COMMAND+'1', open_history_cb, absolute_history[1]},
//  {relative_history[2], FL_COMMAND+'2', open_history_cb, absolute_history[2]},
//  {relative_history[3], FL_COMMAND+'3', open_history_cb, absolute_history[3]},
//  {relative_history[4], FL_COMMAND+'4', open_history_cb, absolute_history[4]},
//  {relative_history[5], FL_COMMAND+'5', open_history_cb, absolute_history[5]},
//  {relative_history[6], FL_COMMAND+'6', open_history_cb, absolute_history[6]},
//  {relative_history[7], FL_COMMAND+'7', open_history_cb, absolute_history[7]},
//  {relative_history[8], FL_COMMAND+'8', open_history_cb, absolute_history[8]},
//  {relative_history[9], FL_COMMAND+'9', open_history_cb, absolute_history[9], FL_MENU_DIVIDER},
#if !defined(__APPLE__)
  {"&Quit", FL_COMMAND+'q', quit_cb},
#endif
  {0},
{"&Edit",0,0,0,FL_SUBMENU},
//  {"&Undo", FL_COMMAND+'z', undo_cb},
//  {"&Redo", FL_COMMAND+FL_SHIFT+'z', redo_cb, 0, FL_MENU_DIVIDER},
//  {"C&ut", FL_COMMAND+'x', cut_cb},
  {"&Copy", FL_COMMAND+'c', copy_cb},
  {"&Paste", FL_COMMAND+'v', paste_cb},
  {0},
{"&View",0,0,0,FL_SUBMENU},
  {"&Byte", 0, view_cb, (void*) BWL_BYTE, FL_MENU_RADIO},
  {"&Word", 0, view_cb, (void*)  BWL_WORD, FL_MENU_CHECK|FL_MENU_RADIO},
  {"&Long", 0, view_cb, (void*) BWL_LONG, FL_MENU_RADIO|FL_MENU_DIVIDER},
  {"&Octal", 0, base_cb, (void*) 8, FL_MENU_RADIO},
  {"&Decimal", 0, base_cb, (void*) 10, FL_MENU_RADIO},
  {"&Hex", 0, base_cb, (void*) 16, FL_MENU_CHECK|FL_MENU_RADIO|FL_MENU_DIVIDER},
  {"&Signed", 0, view_cb, (void*) BWL_SIGNED, FL_MENU_TOGGLE},
  {0},
{0}
};

#ifdef __APPLE__
#define TOOLBAR_Y 0
#else
#define TOOLBAR_Y 25
#endif

int main() {
  win = new Fl_Double_Window(920, 480, "Fl Hex Editor");
  Fl_Sys_Menu_Bar menubar(0, 0, win->w(), 25);
  menubar.menu(Main_Menu);
  table = new HexGrid(CELL_BORDER, CELL_BORDER, win->w()-CELL_BORDER*2, win->h()-CELL_BORDER*2 - TOOLBAR_Y);
  table->tab_cell_nav(1);               // enable tab navigation of table cells (instead of fltk widgets)
  table->tooltip("Use keyboard to navigate cells:\n"
                 "Arrow keys or Tab/Shift-Tab");
  // Table rows
  table->row_header(1);
  table->row_header_width(80);
  table->row_resize(1);
  table->rows(MAX_ROWS);
  table->row_height_all(25);
  // Table cols
  table->col_header(1);
  table->col_header_height(25);
  table->col_resize(1);
  table->cols(MAX_COLS);
  int width = col_chars() * COL_WIDTH;
  table->col_width_all(width);
  // Show window
  new_cb(table, 0);
  win->resizable(table);
  win->show();
  return Fl::run();
}
