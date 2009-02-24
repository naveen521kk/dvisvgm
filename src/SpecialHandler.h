/***********************************************************************
** SpecialHandler.h                                                   **
**                                                                    **
** This file is part of dvisvgm -- the DVI to SVG converter           **
** Copyright (C) 2005-2009 Martin Gieseking <martin.gieseking@uos.de> **
**                                                                    **
** This program is free software; you can redistribute it and/or      **
** modify it under the terms of the GNU General Public License        **
** as published by the Free Software Foundation; either version 2     **
** of the License, or (at your option) any later version.             **
**                                                                    **
** This program is distributed in the hope that it will be useful,    **
** but WITHOUT ANY WARRANTY; without even the implied warranty of     **
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      **
** GNU General Public License for more details.                       **
**                                                                    **
** You should have received a copy of the GNU General Public License  **
** along with this program; if not, write to the Free Software        **
** Foundation, Inc., 51 Franklin Street, Fifth Floor,                 **
** Boston, MA 02110-1301, USA.                                        **
***********************************************************************/

#ifndef SPECIALHANDLER_H
#define SPECIALHANDLER_H

#include <istream>
#include <list>
#include "MessageException.h"
#include "SpecialActions.h"


struct SpecialException : public MessageException
{
	SpecialException (const string &msg) : MessageException(msg) {}
};


struct SpecialHandler
{
	virtual ~SpecialHandler () {}
	virtual const char* prefix () const=0;
	virtual const char* info () const=0;
	virtual const char* name () const {return prefix();}
	virtual bool process (const char *prefix, std::istream &is, SpecialActions *actions)=0;
	virtual void endPage () {}
};


#endif
