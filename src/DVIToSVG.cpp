/*************************************************************************
** DVIToSVG.cpp                                                         **
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

#include <config.h>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>
#include "Calculator.hpp"
#include "DVIToSVG.hpp"
#include "DVIToSVGActions.hpp"
#include "FileSystem.hpp"
#include "Font.hpp"
#include "FontManager.hpp"
#include "GlyphTracerMessages.hpp"
#include "HashFunction.hpp"
#include "InputBuffer.hpp"
#include "InputReader.hpp"
#include "PageRanges.hpp"
#include "PageSize.hpp"
#include "PreScanDVIReader.hpp"
#include "SignalHandler.hpp"
#include "SVGOutput.hpp"
#include "utility.hpp"
#include "version.hpp"

///////////////////////////////////
// special handlers

#include "BgColorSpecialHandler.hpp"
#include "ColorSpecialHandler.hpp"
#include "DvisvgmSpecialHandler.hpp"
#include "EmSpecialHandler.hpp"
#include "HtmlSpecialHandler.hpp"
#include "PapersizeSpecialHandler.hpp"
#include "PdfSpecialHandler.hpp"
#include "TpicSpecialHandler.hpp"
#ifndef HAVE_LIBGS
	#include "NoPsSpecialHandler.hpp"
#endif
#ifndef DISABLE_GS
	#include "PsSpecialHandler.hpp"
#endif

///////////////////////////////////

using namespace std;

/** 'a': trace all glyphs even if some of them are already cached
 *  'm': trace missing glyphs, i.e. glyphs not yet cached
 *   0 : only trace actually required glyphs */
char DVIToSVG::TRACE_MODE = 0;
bool DVIToSVG::COMPUTE_PROGRESS = false;
string DVIToSVG::HASH_ALGO_NAME;


DVIToSVG::DVIToSVG (istream &is, SVGOutputBase &out) : DVIReader(is), _out(out)
{
	_pageHeight = _pageWidth = 0;
	_tx = _ty = 0;    // no cursor translation
	_pageByte = 0;
	_prevXPos = _prevYPos = numeric_limits<double>::min();
	_prevWritingMode = WritingMode::LR;
	_actions = util::make_unique<DVIToSVGActions>(*this, _svg);
}


/** Starts the conversion process.
 *  @param[in] first number of first page to convert
 *  @param[in] last number of last page to convert
 *  @param[in] hashFunc pointer to function to be used to compute page hashes */
void DVIToSVG::convert (unsigned first, unsigned last, HashFunction *hashFunc) {
	if (first > last)
		swap(first, last);
	if (first > numberOfPages()) {
		ostringstream oss;
		oss << "file contains only " << numberOfPages() << " page";
		if (numberOfPages() > 1)
			oss << 's';
		throw DVIException(oss.str());
	}
	last = min(last, numberOfPages());
	bool computeHashes = (hashFunc && !_out.ignoresHashes());
	for (unsigned i=first; i <= last; ++i) {
		string hash;
		if (computeHashes) {
			computePageHash(i, *hashFunc);
			hash = hashFunc->digestString();
			hashFunc->reset();
		}
		string fname = _out.filename(i, numberOfPages(), hash);
		if (!hash.empty() && FileSystem::exists(fname)) {
			Message::mstream(false, Message::MC_PAGE_NUMBER) << "skipping page " << i;
			Message::mstream().indent(1);
			Message::mstream(false, Message::MC_PAGE_WRITTEN) << "\nfile " << fname << " exists\n";
			Message::mstream().indent(0);
		}
		else {
			executePage(i);
			_svg.removeRedundantElements();
			embedFonts(_svg.rootNode());
			bool success = _svg.write(_out.getPageStream(currentPageNumber(), numberOfPages(), hash));
			if (fname.empty())
				fname = "<stdout>";
			if (success)
				Message::mstream(false, Message::MC_PAGE_WRITTEN) << "\noutput written to " << fname << '\n';
			else
				Message::wstream(true) << "failed to write output to " << fname << '\n';
			_svg.reset();
			_actions->reset();
		}
	}
}


/** Convert DVI pages specified by a page range string.
 *  @param[in] rangestr string describing the pages to convert
 *  @param[out] pageinfo (number of converted pages, number of total pages) */
void DVIToSVG::convert (const string &rangestr, pair<int,int> *pageinfo) {
	PageRanges ranges;
	if (!ranges.parse(rangestr, numberOfPages()))
		throw MessageException("invalid page range format");

	Message::mstream(false, Message::MC_PAGE_NUMBER) << "pre-processing DVI file (format version "  << getDVIVersion() << ")\n";
	if (DVIToSVGActions *actions = dynamic_cast<DVIToSVGActions*>(_actions.get())) {
		PreScanDVIReader prescan(getInputStream(), actions);
		actions->setDVIReader(prescan);
		prescan.executeAllPages();
		actions->setDVIReader(*this);
		SpecialManager::instance().notifyPreprocessingFinished();
	}

	unique_ptr<HashFunction> hashFunc;
	if (!HASH_ALGO_NAME.empty()) {
		hashFunc = HashFunction::create(HASH_ALGO_NAME);
		if (!hashFunc) {
			string msg = "unknown hash algorithm '"+HASH_ALGO_NAME+"' (supported algorithms: ";
			for (const string &name : HashFunction::supportedAlgorithms())
				msg += name + ", ";
			msg.pop_back();
			msg.back() = ')';
			throw MessageException(msg);
		}
	}
	for (const auto &range : ranges)
		convert(range.first, range.second, hashFunc.get());
	if (pageinfo) {
		pageinfo->first = ranges.numberOfPages();
		pageinfo->second = numberOfPages();
	}
}


int DVIToSVG::executeCommand () {
	SignalHandler::instance().check();
	const streampos cmdpos = tell();
	int opcode = DVIReader::executeCommand();
	if (dviState().v+_ty != _prevYPos) {
		_tx = _ty = 0;
		_prevYPos = dviState().v;
	}
	_prevXPos = dviState().h+_tx;
	_prevWritingMode = dviState().d;
	if (COMPUTE_PROGRESS && inPage() && _actions) {
		size_t pagelen = numberOfPageBytes(currentPageNumber()-1);
		_pageByte += tell()-cmdpos;
		_actions->progress(_pageByte, pagelen);
	}
	return opcode;
}


/** This template method is called by parent class DVIReader before
 *  executing the BOP actions.
 *  @param[in] pageno physical page number (1 = first page)
 *  @param[in] c contains information about the page (page number etc.) */
void DVIToSVG::enterBeginPage (unsigned pageno, const vector<int32_t> &c) {
	if (dynamic_cast<DVIToSVGActions*>(_actions.get())) {
		Message::mstream().indent(0);
		Message::mstream(false, Message::MC_PAGE_NUMBER) << "processing page " << pageno;
		if (pageno != (unsigned)c[0])  // Does page number shown on page differ from physical page number?
			Message::mstream(false) << " [" << c[0] << ']';
		Message::mstream().indent(1);
		_svg.appendToDoc(util::make_unique<XMLCommentNode>(" This file was generated by dvisvgm " + string(PROGRAM_VERSION) + " "));
	}
}


/** This template method is called by DVIReader::cmdEop() after
 *  executing the EOP actions. */
void DVIToSVG::leaveEndPage (unsigned) {
	if (!dynamic_cast<DVIToSVGActions*>(_actions.get()))
		return;

	// set bounding box and apply page transformations
	BoundingBox bbox = _actions->bbox();  // bounding box derived from the DVI commands executed
	if (_bboxFormatString == "min" || _bboxFormatString == "preview" || _bboxFormatString == "papersize") {
		bbox.unlock();
		bbox.transform(getPageTransformation());
	}
	else if (_bboxFormatString == "dvi") {
		// center page content
		double dx = (_pageWidth-bbox.width())/2;
		double dy = (_pageHeight-bbox.height())/2;
		bbox += BoundingBox(-dx, -dy, dx, dy);
	}
	else if (_bboxFormatString != "none") {
		istringstream iss(_bboxFormatString);
		StreamInputReader ir(iss);
		ir.skipSpace();
		if (isalpha(ir.peek())) {
			// set explicitly given page format
			PageSize size(_bboxFormatString);
			if (size.valid()) {
				// convention: DVI position (0,0) equals (1in, 1in) relative
				// to the upper left vertex of the page (see DVI specification)
				const double border = -72;
				bbox = BoundingBox(border, border, size.width().bp()+border, size.height().bp()+border);
			}
		}
		else { // set/modify bounding box by explicitly given values
			try {
				vector<Length> lengths = BoundingBox::extractLengths(_bboxFormatString);
				if (lengths.size() == 1 || lengths.size() == 2) {  // relative box size?
					// apply the page transformation and adjust the bbox afterwards
					bbox.transform(getPageTransformation());
				}
				bbox.set(lengths);
			}
			catch (const MessageException &e) {
			}
		}
	}
	if (bbox.width() == 0)
		Message::wstream(false) << "\npage is empty\n";
	if (_bboxFormatString != "none") {
		_svg.setBBox(bbox);
		const double bp2pt = 72.27/72;
		const double bp2mm = 25.4/72;
		Message::mstream(false) << '\n';
		Message::mstream(false, Message::MC_PAGE_SIZE) << "graphic size: " << XMLString(bbox.width()*bp2pt) << "pt"
			" x " << XMLString(bbox.height()*bp2pt) << "pt"
			" (" << XMLString(bbox.width()*bp2mm) << "mm"
			" x " << XMLString(bbox.height()*bp2mm) << "mm)";
		Message::mstream(false) << '\n';
	}
}


Matrix DVIToSVG::getPageTransformation () const {
	Matrix matrix(1); // unity matrix
	if (!_transCmds.empty()) {
		Calculator calc;
		if (_actions) {
			const double bp2pt = (1_bp).pt();
			BoundingBox &bbox = _actions->bbox();
			calc.setVariable("ux", bbox.minX()*bp2pt);
			calc.setVariable("uy", bbox.minY()*bp2pt);
			calc.setVariable("w",  bbox.width()*bp2pt);
			calc.setVariable("h",  bbox.height()*bp2pt);
		}
		// add constants for length units to calculator
		for (auto unit : Length::getUnits())
			calc.setVariable(unit.first, Length(1, unit.second).pt());
		matrix.set(_transCmds, calc);
	}
	return matrix;
}


static void collect_chars (unordered_map<const Font*, set<int>> &fontmap) {
	for (const auto &entry : fontmap) {
		if (entry.first->uniqueFont() != entry.first) {
			for (int c : entry.second)
				fontmap[entry.first->uniqueFont()].insert(c);
		}
	}
}


/** Adds the font information to the SVG tree.
 *  @param[in] svgElement the font nodes are added to this node */
void DVIToSVG::embedFonts (XMLElementNode *svgElement) {
	if (!svgElement || !_actions) // no dvi actions => no chars written => no fonts to embed
		return;

	const DVIToSVGActions *svgActions = static_cast<DVIToSVGActions*>(_actions.get());
	auto &usedCharsMap = svgActions->getUsedChars();

	collect_chars(usedCharsMap);

	GlyphTracerMessages messages;
	unordered_set<const Font*> tracedFonts;  // collect unique fonts already traced
	for (const auto &fontchar : usedCharsMap) {
		const Font *font = fontchar.first;
		if (const PhysicalFont *ph_font = dynamic_cast<const PhysicalFont*>(font)) {
			// Check if glyphs should be traced. Only trace the glyphs of unique fonts, i.e.
			// avoid retracing the same glyphs again if they are referenced in various sizes.
			if (TRACE_MODE != 0 && tracedFonts.find(ph_font->uniqueFont()) == tracedFonts.end()) {
				ph_font->traceAllGlyphs(TRACE_MODE == 'a', &messages);
				tracedFonts.insert(ph_font->uniqueFont());
			}
			if (font->path())  // does font file exist?
				_svg.append(*ph_font, fontchar.second, &messages);
			else
				Message::wstream(true) << "can't embed font '" << font->name() << "'\n";
		}
		else
			Message::wstream(true) << "can't embed font '" << font->name() << "'\n";
	}
	_svg.appendFontStyles(svgActions->getUsedFonts());
}


/** Enables or disables processing of specials. If ignorelist == 0, all
 *  supported special handlers are loaded. To disable selected sets of specials,
 *  the corresponding prefixes can be given separated by non alpha-numeric characters,
 *  e.g. "color, ps, em" or "color: ps em" etc.
 *  A single "*" in the ignore list disables all specials.
 *  @param[in] ignorelist list of special prefixes to ignore
 *  @param[in] pswarning if true, shows warning about disabled PS support
 *  @return the SpecialManager that handles special statements */
void DVIToSVG::setProcessSpecials (const char *ignorelist, bool pswarning) {
	if (ignorelist && strcmp(ignorelist, "*") == 0)  // ignore all specials?
		SpecialManager::instance().unregisterHandlers();
	else {
		// add special handlers
		vector<unique_ptr<SpecialHandler>> handlers;
		handlers.emplace_back(util::make_unique<BgColorSpecialHandler>());   // handles background color special
		handlers.emplace_back(util::make_unique<ColorSpecialHandler>());     // handles color specials
		handlers.emplace_back(util::make_unique<DvisvgmSpecialHandler>());   // handles raw SVG embeddings
		handlers.emplace_back(util::make_unique<EmSpecialHandler>());        // handles emTeX specials
		handlers.emplace_back(util::make_unique<HtmlSpecialHandler>());      // handles hyperref specials
		handlers.emplace_back(util::make_unique<PapersizeSpecialHandler>()); // handles papersize special
		handlers.emplace_back(util::make_unique<PdfSpecialHandler>());       // handles pdf specials
		handlers.emplace_back(util::make_unique<TpicSpecialHandler>());      // handles tpic specials
#ifndef DISABLE_GS
		if (Ghostscript().available())
			handlers.emplace_back(util::make_unique<PsSpecialHandler>());     // handles PostScript specials
		else
#endif
		{
#ifndef HAVE_LIBGS
			// dummy PS special handler that only prints warning messages
			handlers.emplace_back(util::make_unique<NoPsSpecialHandler>());
			if (pswarning) {
#ifdef DISABLE_GS
				Message::wstream() << "processing of PostScript specials has been disabled permanently\n";
#else
				Message::wstream() << "processing of PostScript specials is disabled (Ghostscript not found)\n";
#endif
			}
#endif
		}
		SpecialManager::instance().unregisterHandlers();
		SpecialManager::instance().registerHandlers(handlers, ignorelist);
	}
}


string DVIToSVG::getSVGFilename (unsigned pageno) const {
	return _out.filename(pageno, numberOfPages());
}


void DVIToSVG::moveRight (double dx, MoveMode mode) {
	DVIReader::moveRight(dx, mode);
	if (_actions) {
		if (dviState().d == WritingMode::LR)
			_actions->moveToX(dviState().h+_tx, mode == MoveMode::CHANGEPOS);
		else
			_actions->moveToY(dviState().v+_ty, mode == MoveMode::CHANGEPOS);
	}
}


void DVIToSVG::moveDown (double dy, MoveMode mode) {
	DVIReader::moveDown(dy, mode);
	if (_actions) {
		if (dviState().d == WritingMode::LR)
			_actions->moveToY(dviState().v+_ty, mode == MoveMode::CHANGEPOS);
		else
			_actions->moveToX(dviState().h+_tx, mode == MoveMode::CHANGEPOS);
	}
}


void DVIToSVG::dviPost (uint16_t, uint16_t pages, uint32_t pw, uint32_t ph, uint32_t, uint32_t, uint32_t, uint32_t) {
	_pageHeight = ph; // height of tallest page in dvi units
	_pageWidth  = pw; // width of widest page in dvi units
}


void DVIToSVG::dviBop (const std::vector<int32_t> &c, int32_t) {
	_pageByte = 0;
	enterBeginPage(currentPageNumber(), c);
	if (_actions) {
		_actions->progress(0, 1);  // ensure that progress is called at 0%
		_actions->beginPage(currentPageNumber(), c);
	}
}


void DVIToSVG::dviEop () {
	if (_actions) {
		_actions->endPage(currentPageNumber());
		_pageByte = numberOfPageBytes(currentPageNumber()-1);
		_actions->progress(_pageByte, _pageByte); // ensure that progress is called at 100%
	}
	leaveEndPage(currentPageNumber());
}


void DVIToSVG::dviSetChar0 (uint32_t c, const Font *font) {
	if (_actions && !dynamic_cast<const VirtualFont*>(font))
		_actions->setChar(dviState().h+_tx, dviState().v+_ty, c, dviState().d != WritingMode::LR, *font);
}


void DVIToSVG::dviSetChar (uint32_t c, const Font *font) {
	dviSetChar0(c, font);
}


void DVIToSVG::dviPutChar (uint32_t c, const Font *font) {
	dviSetChar0(c, font);
}


void DVIToSVG::dviSetRule (double height, double width) {
	if (_actions && height > 0 && width > 0)
		_actions->setRule(dviState().h+_tx, dviState().v+_ty, height, width);
}


void DVIToSVG::dviPutRule (double height, double width) {
	dviSetRule(height, width);
}


void DVIToSVG::dviPop () {
	if (_actions) {
		if (_prevXPos != dviState().h+_tx)
			_actions->moveToX(dviState().h + _tx, true);  // force setting the SVG position
		if (_prevYPos != dviState().v+_ty)
			_actions->moveToY(dviState().v + _ty, true);  // force setting the SVG position
		if (_prevWritingMode != dviState().d)
			_actions->setTextOrientation(dviState().d != WritingMode::LR);
	}
}


void DVIToSVG::dviFontNum (uint32_t fontnum, SetFontMode, const Font *font) {
	if (_actions && font && !dynamic_cast<const VirtualFont*>(font))
		_actions->setFont(FontManager::instance().fontID(fontnum), *font);  // all fonts get a recomputed ID
}


void DVIToSVG::dviDir (WritingMode dir) {
	if (_actions)
		_actions->setTextOrientation(dir != WritingMode::LR);
}


void DVIToSVG::dviXXX (const std::string &str) {
	if (_actions)
		_actions->special(str, dvi2bp());
}


void DVIToSVG::dviXGlyphArray (std::vector<double> &dx, vector<double> &dy, vector<uint16_t> &glyphs, const Font &font) {
	if (_actions) {
		for (size_t i=0; i < glyphs.size(); i++)
			_actions->setChar(dviState().h+dx[i]+_tx, dviState().v+dy[i]+_ty, glyphs[i], false, font);
	}
}


void DVIToSVG::dviXGlyphString (vector<double> &dx, vector<uint16_t> &glyphs, const Font &font) {
	if (_actions) {
		for (size_t i=0; i < glyphs.size(); i++)
			_actions->setChar(dviState().h+dx[i]+_tx, dviState().v+_ty, glyphs[i], false, font);
	}
}


void DVIToSVG::dviXTextAndGlyphs (vector<double> &dx, vector<double> &dy, vector<uint16_t>&, vector<uint16_t> &glyphs, const Font &font) {
	dviXGlyphArray(dx, dy, glyphs, font);
}
