/*************************************************************************
** SVGOutput.hpp                                                        **
**                                                                      **
** This file is part of dvisvgm -- a fast DVI to SVG converter          **
** Copyright (C) 2005-2018 Martin Gieseking <martin.gieseking@uos.de>   **
**                                                                      **
** This program is free software; you can redistribute it and/or        **
** modify it under the terms of the GNU General Public License as       **
** published by the Free Software Foundation; either version 3 of       **
** the License, or (at your option) any later version.                  **
**                                                                      **
** This program is distributed in the hope that it will be useful, but  **
** WITHOUT ANY WARRANTY; without even the implied warranty of           **
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the         **
** GNU General Public License for more details.                         **
**                                                                      **
** You should have received a copy of the GNU General Public License    **
** along with this program; if not, see <http://www.gnu.org/licenses/>. **
*************************************************************************/

#ifndef SVGOUTPUT_HPP
#define SVGOUTPUT_HPP

#include <memory>
#include <ostream>
#include <string>
#include "FilePath.hpp"


struct SVGOutputBase {
	virtual ~SVGOutputBase () =default;
	virtual std::ostream& getPageStream (int page, int numPages, const std::string &hash="") const =0;
	virtual std::string filename (int page, int numPages, const std::string &hash="") const =0;
	virtual bool ignoresHashes () const {return true;}
};


class SVGOutput : public SVGOutputBase {
	public:
		SVGOutput () : SVGOutput("", "", 0) {}
		SVGOutput (const std::string &base) : SVGOutput(base, "", 0) {}
		SVGOutput (const std::string &base, const std::string &pattern) : SVGOutput(base, pattern, 0) {}
		SVGOutput (const std::string &base, const std::string &pattern, int zipLevel);
		std::ostream& getPageStream (int page, int numPages, const std::string &hash) const override;
		std::string filename (int page, int numPages, const std::string &hash) const override;
		bool ignoresHashes () const override;

	protected:
		std::string expandFormatString (std::string str, int page, int numPages, const std::string &hash) const;

	private:
		FilePath _path;
		std::string _pattern;
		bool _stdout;      ///< write to STDOUT?
		int _zipLevel;     ///< compression level
		mutable int _page; ///< number of current page being written
		mutable std::unique_ptr<std::ostream> _osptr;
};

#endif
