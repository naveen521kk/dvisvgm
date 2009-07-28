/***********************************************************************
** SpecialManager.h                                                   **
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

#ifndef SPECIALMANAGER_H
#define SPECIALMANAGER_H

#include <map>
#include <ostream>
#include <string>
#include <vector>
#include "SpecialHandler.h"

class SpecialActions;

class SpecialManager
{
	typedef std::vector<SpecialHandler*> HandlerPool;
	typedef std::map<std::string,SpecialHandler*> HandlerMap;
	typedef HandlerMap::iterator Iterator;
	typedef HandlerMap::const_iterator ConstIterator;

   public:
		SpecialManager () {}
      ~SpecialManager ();
		void registerHandler (SpecialHandler *handler);
		void registerHandlers (SpecialHandler **handlers, const char *ignorelist);
		void unregisterHandlers ();
		bool process (const std::string &special, SpecialActions *actions);
		void notifyEndPage ();
		void writeHandlerInfo (std::ostream &os) const;

	protected:
		SpecialManager (const SpecialManager &) {}
		void operator = (const SpecialManager &) {}
		SpecialHandler* findHandler (const std::string &prefix) const;

   private:
		HandlerPool _pool;     ///< stores pointers to all handlers 
		HandlerMap _handlers;  ///< pointers to handlers for corresponding prefixes
};

#endif