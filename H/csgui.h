/*  
    csgui.h:

    Copyright (C) 2002 Barry Vercoe, John ffitch

    This file is part of Csound.

    The Csound Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Csound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

// generated by Fast Light User Interface Designer (fluid) version 1.0100

#ifndef csgui_h
#define csgui_h
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Text_Display.H>
extern Fl_Text_Display *Output;
#include <FL/Fl_Button.H>
extern Fl_Button *Render;
#include <FL/Fl_File_Input.H>
extern Fl_File_Input *Orch;
extern Fl_File_Input *Score;
extern Fl_File_Input *SFName;
#include <FL/Fl_Choice.H>
extern Fl_Choice *OType;
extern Fl_Choice *SSize;
#include <FL/Fl_Value_Slider.H>
extern void callback(Fl_Value_Slider*, void*);
extern Fl_Value_Slider *Messages;
#include <FL/Fl_Input.H>
extern Fl_Input *Args;
Fl_Window* make_window();
extern Fl_Menu_Item menu_OType[];
extern Fl_Menu_Item menu_Size[];
#endif
