//=============================================================================
//  nped Program Editor
//
//  Copyright (C) 2025-2026 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#pragma once

static const char* const CMD_QUIT        = "Shift+F1; Ctrl + K,  Ctrl + Q";
static const char* const CMD_SAVE_QUIT   = "F1";
static const char* const CMD_CHAR_RIGHT  = "Right; Ctrl + D";
static const char* const CMD_CHAR_LEFT   = "Left;  Ctrl + S";
static const char* const CMD_LINE_UP     = "Up;    Ctrl + E";
static const char* const CMD_LINE_DOWN   = "Down;  Ctrl + X";
static const char* const CMD_LINE_START  = "Ctrl + Q,  Ctrl + S";
static const char* const CMD_LINE_END    = "Ctrl + Q,  Ctrl + D";
static const char* const CMD_LINE_TOP    = "Ctrl + Q,  Ctrl + E";
static const char* const CMD_LINE_BOTTOM = "Ctrl + Q,  Ctrl + X";
static const char* const CMD_PAGE_UP     = "Ctrl + R;  PageUp";
static const char* const CMD_PAGE_DOWN   = "Ctrl + C;  PageDown";
static const char* const CMD_FILE_BEGIN  = "Ctrl + Q,  Ctrl + R";
static const char* const CMD_FILE_END    = "Ctrl + Q,  Ctrl + C";
static const char* const CMD_WORD_LEFT   = "Ctrl + A";
static const char* const CMD_WORD_RIGHT  = "Ctrl + F";
static const char* const CMD_ENTER       = " Escape";

// file commands
static const char* const CMD_SAVE          = "Ctrl + K,  Ctrl + S";
static const char* const CMD_KONTEXT_COPY  = "Ctrl + K,  Ctrl + K; F4";
static const char* const CMD_KONTEXT_PREV  = "Ctrl + K,  Ctrl + J; Shift + F3";
static const char* const CMD_KONTEXT_NEXT  = "Ctrl + K,  Ctrl + L; F3";
static const char* const CMD_SHOW_BRACKETS = "Ctrl + K,  Ctrl + I";
static const char* const CMD_SHOW_LEVEL    = "Ctrl + K,  Ctrl + M";

static const char* const CMD_KONTEXT_UP        = "Ctrl + Up";
static const char* const CMD_KONTEXT_DOWN      = "Ctrl + Down";
static const char* const CMD_DELETE_LINE       = "Ctrl + Y";
static const char* const CMD_DELETE_LINE_RIGHT = "Ctrl + Q,  Ctrl + Y";
static const char* const CMD_UNDO              = "Ctrl + Z";
static const char* const CMD_REDO              = "Shift + Ctrl + Z";
static const char* const CMD_RUBOUT            = "Delete; Backspace";
static const char* const CMD_CHAR_DELETE       = "Ctrl + G";

static const char* const CMD_INSERT_LINE = "Ctrl + N";

static const char* const CMD_TAB         = "Tab";
static const char* const CMD_SELECT_ROW  = "F5";
static const char* const CMD_SELECT_COL  = "F6";
static const char* const CMD_SEARCH_NEXT = "F7";
static const char* const CMD_RENAME      = "Ctrl + O, Ctrl + R";
static const char* const CMD_PICK        = "F8";
static const char* const CMD_PUT         = "F9";
static const char* const CMD_SEARCH_PREV = "SHIFT + F7";
static const char* const CMD_DELETE_WORD = "Ctrl + T";
static const char* const CMD_ENTER_WORD  = "Ctrl + O,  Ctrl + W";
static const char* const CMD_GOTO_BACK   = "Ctrl + F12";
static const char* const CMD_SHOW_INFO   = "Ctrl + H";

// IDE commands
static const char* const CMD_FORMAT               = "Ctrl + O,  Ctrl + F";
static const char* const CMD_VIEW_FUNCTIONS       = "Ctrl + V";
static const char* const CMD_VIEW_BUGS            = "Ctrl + B";
static const char* const CMD_GOTO_TYPE_DEFINITION = "F10";
static const char* const CMD_GOTO_IMPLEMENTATION  = "F11";
static const char* const CMD_GOTO_DEFINITION      = "F12";
static const char* const CMD_EXPAND_MACROS        = "Ctrl + O, Ctrl + E";
static const char* const CMD_COMPLETIONS          = "Ctrl + Tab; Shift + Tab";
static const char* const CMD_FOLD_ALL             = "Ctrl + M";
static const char* const CMD_UNFOLD_ALL           = "Ctrl + Shift + M";
static const char* const CMD_FOLD_TOGGLE          = "Ctrl + <";
static const char* const CMD_FUNCTION_HEADER      = "Ctrl + O, Ctrl + H";
static const char* const CMD_GIT_TOGGLE           = "Ctrl + O, Ctrl + G";

// Ctrl-O
// CMD_ENTER_WORD             = "Ctrl + O, Ctrl + W";
// CMD_FORMAT                 = "Ctrl + O, Ctrl + F";
// CMD_FUNCTION_HEADER        = "Ctrl + O, Ctrl + H";
// CMD_EXPAND_MACROS          = "Ctrl + O, Ctrl + E";
// CMD_GIT_TOGGLE             = "Ctrl + O, Ctrl + G";
// CMD_RENAME                 = "Ctrl + O, Ctrl + R";

// Function Keys
// CMD_SAVE_QUIT              = "F1";
// CMD_KONTEXT_NEXT           = "F3";
// CMD_KONTEXT_COPY           = "F4";
// CMD_SELECT_ROW             = "F5";
// CMD_SELECT_COL             = "F6";
// CMD_SEARCH_NEXT            = "F7";
// CMD_SEARCH_PREV            = "SHIFT + F7";
// CMD_PICK                   = "F8";
// CMD_PUT                    = "F9";
// CMD_GOTO_TYPE_DEFINITION   = "F10";
// CMD_GOTO_IMPLEMENTATION    = "F11";
// CMD_GOTO_DEFINITION        = "F12";
