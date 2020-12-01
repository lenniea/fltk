//
//      Simple example of an interactive spreadsheet using Fl_Table.
//      Uses Mr. Satan's technique of instancing an Fl_Input around.
//
// Copyright 1998-2010 by Bill Spitzak and others.
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
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Int_Input.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Sys_Menu_Bar.H>

const int MAX_COLS = 100;
const int MAX_ROWS = 100;
const int COL_WIDTH = 10;
const int SEL_SIZE  = 2;

int base = 10;

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
  int n = 1;
  if (base == 8) {
    return 11 + n;
  }
  if (base == 10) {
    return 10 + n;
  }
  return 8 + n;
}

int format_num(char* buf, int n) {
  int len = 0;
  if (base == 8) {
    len += sprintf(buf + len, "%o", n);
  } else if (base == 10) {
    len += sprintf(buf + len, "%u", n);
  } else {
    len += sprintf(buf + len, "%08X", n);
  }
  return len;
}

class Spreadsheet : public Fl_Table {
  Fl_Int_Input *input;                                  // single instance of Fl_Int_Input widget
  int values[MAX_ROWS][MAX_COLS];                       // array of data for cells
  int row_edit, col_edit;                               // row/col being modified

protected:
  void draw_cell(TableContext context,int=0,int=0,int=0,int=0,int=0,int=0);
  void event_callback2();                               // table's event callback (instance)
  static void event_callback(Fl_Widget*,void *v) {      // table's event callback (static)
    ((Spreadsheet*)v)->event_callback2();
  }
  static void input_cb(Fl_Widget*,void* v) {            // input widget's callback
    ((Spreadsheet*)v)->set_value_hide();
  }

public:
  Spreadsheet(int X,int Y,int W,int H,const char* L=0) : Fl_Table(X,Y,W,H,L) {
    callback(&event_callback, (void*)this);
    when(FL_WHEN_NOT_CHANGED|when());
    // Create input widget that we'll use whenever user clicks on a cell
    input = new Fl_Int_Input(W/2,H/2,0,0);
    input->hide();                                      // hide until needed
    input->callback(input_cb, (void*)this);
    input->when(FL_WHEN_ENTER_KEY_ALWAYS);              // callback triggered when user hits Enter
    input->maximum_size(5);
    input->color(FL_YELLOW);                            // yellow to standout during editing
    for (int c = 0; c < MAX_COLS; c++)
      for (int r = 0; r < MAX_ROWS; r++)
        values[r][c] = c + (r*MAX_COLS);                // initialize cells
    end();
    row_edit = col_edit = 0;
    set_selection(0,0,0,0);
  }
  int format_col(char* buf, int n) {
    int len = (n < 26) ? sprintf(buf, "%c", 'A'+n)
            : sprintf(buf, "%c%c", 'A' + n/26 - 1, 'A' + n%26);
    return len;
  }

  int format_row(char* buf, int n) {
    int len = sprintf(buf, "%u", n + 1);
    return len;
  }
  int format_cell(char* buf, int R, int C) {
    int value = values[R][C];
    return format_num(buf, value);
  }
  void set_cell(int R, int C, int value) {
    values[R][C] = value;
  }

  ~Spreadsheet() { }

  // Apply value from input widget to values[row][col] array and hide (done editing)
  void set_value_hide() {
    values[row_edit][col_edit] = parse_num(input->value());
    input->hide();
    window()->cursor(FL_CURSOR_DEFAULT);                // XXX: if we don't do this, cursor can disappear!
  }
  // Start editing a new cell: move the Fl_Int_Input widget to specified row/column
  //    Preload the widget with the cell's current value,
  //    and make the widget 'appear' at the cell's location.
  //
  void start_editing(int R, int C) {
    row_edit = R;                                       // Now editing this row/col
    col_edit = C;
    set_selection(R,C,R,C);                             // Clear any previous multicell selection
    int X,Y,W,H;
    find_cell(CONTEXT_CELL, R,C, X,Y,W,H);              // Find X/Y/W/H of cell
    input->resize(X,Y,W,H);                             // Move Fl_Input widget there
    char s[30];
    int value = values[R][C];
    format_num(s, value);                               // Load input widget with cell's current value
    input->value(s);
    input->position(0,strlen(s));                       // Select entire input field
    input->show();                                      // Show the input widget, now that we've positioned it
    input->take_focus();                                // Put keyboard focus into the input widget
  }
  // Tell the input widget it's done editing, and to 'hide'
  void done_editing() {
    if (input->visible()) {                             // input widget visible, ie. edit in progress?
      set_value_hide();                                 // Transfer its current contents to cell and hide
      row_edit = 0;                                     // Disable these until needed again
      col_edit = 0;
    }
  }
  // Return the sum of all rows in this column
  int sum_rows(int C) {
    int sum = 0;
    for (int r=0; r<rows()-1; ++r)                      // -1: don't include cell data in 'totals' column
      sum += values[r][C];
    return(sum);
  }
  // Return the sum of all cols in this row
  int sum_cols(int R) {
    int sum = 0;
    for (int c=0; c<cols()-1; ++c)                      // -1: don't include cell data in 'totals' column
      sum += values[R][c];
    return(sum);
  }
  // Return the sum of all cells in table
  int sum_all() {
    int sum = 0;
    for (int c=0; c<cols()-1; ++c)                      // -1: don't include cell data in 'totals' column
      for (int r=0; r<rows()-1; ++r)                    // -1: ""
        sum += values[r][c];
    return(sum);
  }
};

Spreadsheet* table = NULL;

// Handle drawing all cells in table
void Spreadsheet::draw_cell(TableContext context, int R,int C, int X,int Y,int W,int H) {
  static char s[30];
  switch ( context ) {
    case CONTEXT_STARTPAGE:                     // table about to redraw
      break;

    case CONTEXT_COL_HEADER:                    // table wants us to draw a column heading (C is column)
      fl_font(FL_HELVETICA | FL_BOLD, 14);      // set font for heading to bold
      fl_push_clip(X,Y,W,H);                    // clip region for text
      {
        fl_draw_box(FL_THIN_UP_BOX, X,Y,W,H, col_header_color());
        if (is_selected(row_edit,C)) {
          // draw selection on bottom edge
          fl_color(FL_DARK_BLUE);
          fl_rectf(X,Y+H-SEL_SIZE-1, W, SEL_SIZE);
        }
        fl_color(FL_BLACK);
        if (C == cols()-1) {                    // Last column? show 'TOTAL'
          fl_draw("TOTAL", X,Y,W,H, FL_ALIGN_CENTER);
        } else {                                // Not last column? show column letter
          format_col(s, C);
          fl_draw(s, X,Y,W,H, FL_ALIGN_CENTER);
        }
      }
      fl_pop_clip();
      return;

    case CONTEXT_ROW_HEADER:                    // table wants us to draw a row heading (R is row)
      fl_font(FL_HELVETICA | FL_BOLD, 14);      // set font for row heading to bold
      fl_push_clip(X,Y,W,H);
      {
        fl_draw_box(FL_THIN_UP_BOX, X,Y,W,H, row_header_color());
        if (is_selected(R,col_edit)) {
          // draw selection on right edge
          fl_color(FL_DARK_BLUE);
          fl_rectf(X+W-SEL_SIZE-1, Y, SEL_SIZE, H);
        }
        fl_color(FL_BLACK);
        if (R == rows()-1) {                    // Last row? Show 'Total'
          fl_draw("TOTAL", X,Y,W,H, FL_ALIGN_CENTER);
        } else {                                // Not last row? show row#
          format_row(s, R);
          fl_draw(s, X,Y,W,H, FL_ALIGN_CENTER);
        }
      }
      fl_pop_clip();
      return;

    case CONTEXT_CELL: {                        // table wants us to draw a cell
      if (R == row_edit && C == col_edit && input->visible()) {
        return;                                 // dont draw for cell with input widget over it
      }
      // Background
      if ( C < cols()-1 && R < rows()-1 ) {
        fl_draw_box(FL_THIN_UP_BOX, X,Y,W,H, is_selected(R,C) ? FL_YELLOW : FL_WHITE);
      } else {
        fl_draw_box(FL_THIN_UP_BOX, X,Y,W,H, is_selected(R,C) ? 0xddffdd00 : 0xbbddbb00);       // money green
      }
      // Text
      fl_push_clip(X+3, Y+3, W-6, H-6);
      {
        fl_color(FL_BLACK);
        if (C == cols()-1 || R == rows()-1) {   // Last row or col? Show total
          fl_font(FL_HELVETICA | FL_BOLD, 14);  // ..in bold font
          if (C == cols()-1 && R == rows()-1) { // Last row+col? Total all cells
            format_num(s, sum_all());
          } else if (C == cols()-1) {           // Row subtotal
            format_num(s, sum_cols(R));
          } else if (R == rows()-1) {           // Col subtotal
            format_num(s, sum_rows(C));
          }
          fl_draw(s, X+3,Y+3,W-6,H-6, FL_ALIGN_RIGHT);
        } else {                                // Not last row or col? Show cell contents
          fl_font(FL_HELVETICA, 14);            // ..in regular font
          format_num(s, values[R][C]);
          fl_draw(s, X+3,Y+3,W-6,H-6, FL_ALIGN_RIGHT);
        }
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
void Spreadsheet::event_callback2() {
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
          switch ( Fl::e_text[0] ) {
            case '0': case '1': case '2': case '3':     // any of these should start editing new cell
            case '4': case '5': case '6': case '7':
            case '8': case '9': case '+': case '-':
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

static void new_cb(Fl_Widget *, void *v) {
  // TODO: Handle File New
  fprintf(stderr, "new_cb\n");
}
static void open_cb(Fl_Widget *, void *v) {
  // TODO: Handle File/Open...
  fprintf(stderr, "open_cb\n");
}
static void save_cb(Fl_Widget *, void *v) {
  // TODO: Handle File/Save/SaveAs...
  fprintf(stderr, "save_cb\n");
}
#if !defined(__APPLE__)
static void quit_cb(Fl_Widget *, void *v) {
  exit(0);
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
      len += table->format_cell(buf + len, row, col);
      buf[len++] = (col == right) ? '\n' : '\t';
    }
  }
  buf[len] = '\0';
  Fl::copy(buf, len, 1);
  delete [] buf;
}
static void paste_cb(Fl_Widget *, void *v) {
  // TODO: Handle Edit/Paste
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
  int width = col_chars() * COL_WIDTH;
  table->col_width_all(width);
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
  {"&Octal", 0, base_cb, (void*) 8, FL_MENU_RADIO},
  {"&Decimal", 0, base_cb, (void*) 10, FL_MENU_CHECK|FL_MENU_RADIO},
  {"&Hex", 0, base_cb, (void*) 16, FL_MENU_RADIO|FL_MENU_DIVIDER},
  {0},
{0}
};

#ifdef __APPLE__
#define TOOLBAR_Y 0
#else
#define TOOLBAR_Y 25
#endif

int main() {
  Fl_Double_Window *win = new Fl_Double_Window(862, 322, "Fl_Table Spreadsheet");
  Fl_Sys_Menu_Bar menubar(0, 0, win->w(), 0);
  menubar.menu(Main_Menu);

  table = new Spreadsheet(10, 10, win->w()-20, win->h()-20);
  table->tab_cell_nav(1);               // enable tab navigation of table cells (instead of fltk widgets)
  table->tooltip("Use keyboard to navigate cells:\n"
                 "Arrow keys or Tab/Shift-Tab");
  // Table rows
  table->row_header(1);
  table->row_header_width(70);
  table->row_resize(1);
  table->rows(MAX_ROWS+1);              // +1: leaves room for 'total row'
  table->row_height_all(25);
  // Table cols
  table->col_header(1);
  table->col_header_height(25);
  table->col_resize(1);
  table->cols(MAX_COLS+1);              // +1: leaves room for 'total column'
  int width = col_chars() * COL_WIDTH;
  table->col_width_all(width);
  // Show window
  win->end();
  win->resizable(table);
  win->show();
  return Fl::run();
}
