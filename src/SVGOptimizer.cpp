/*************************************************************************
** SVGOptimizer.cpp                                                     **
**                                                                      **
** This file is part of dvisvgm -- a fast DVI to SVG converter          **
** Copyright (C) 2005-2019 Martin Gieseking <martin.gieseking@uos.de>   **
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

#include <algorithm>
#include <array>
#include "SVGOptimizer.hpp"
#include "SVGTree.hpp"

using namespace std;

bool SVGOptimizer::GROUP_ATTRIBUTES=false;

void SVGOptimizer::execute () {
	if (_svg.pageNode()) {
		if (GROUP_ATTRIBUTES) {
			AttributeExtractor().execute(*_svg.pageNode());
			GroupCollapser().execute(*_svg.pageNode());
		}
		if (_svg.defsNode())
			RedundantElementRemover().execute(*_svg.defsNode(), *_svg.pageNode());
	}
}

/////////////////////////////////////////////////////////////////////////////

/** Constructs a new run object for an attribute and a given range [first, last) of nodes.
 *  @param[in] attr attribute to look for
 *  @param[in] first first node in sequence to consider
 *  @param[in] last first node after last node in sequence to consider */
AttributeExtractor::AttributeRun::AttributeRun (const Attribute &attr, Iterator first, Iterator last) {
	_length = 1;
	_begin = _end = first;
	for (++_end; _end != last; ++_end) {
		if ((*_end)->toText() || (*_end)->toCData())  // don't include text/CDATA nodes
			break;
		if (XMLElement *elem = (*_end)->toElement()) {
			if (!groupable(*elem))
				break;
			const char *val = elem->getAttributeValue(attr.name);
			if (val && val == attr.value)
				++_length;
			else
				break;
		}
	}
}


/** Performs the attribute extraction on a given context node. Each extracted
 *  attribute gets its own group, i.e. the extraction of multiple attributes
 *  of the same elements lead to nested groups.
 *  @param[in] context attributes of all children in this element are extracted
 *  @param[in] recurse if true, the algorithm is recursively performed on all descendant elements */
void AttributeExtractor::execute (XMLElement &context, bool recurse) {
	if (context.children().empty())
		return;
	if (recurse) {
		for (auto &node : context) {
			if (XMLElement *elem = node->toElement())
				execute(*elem, true);
		}
	}
	for (auto it=context.begin(); it != context.end(); ++it)
		it = extractAttribute(it, context);
}


/** Looks for the first attribute not yet processed and extracts it if possible.
 *  @param[in] pos points to the first node of a node sequence with potentially identical attributes
 *  @param[in] parent parent element whose child nodes are being processed
 *  @return iterator to group element if extraction was successful, 'pos' otherwise */
AttributeExtractor::Iterator AttributeExtractor::extractAttribute (Iterator pos, XMLElement &parent) {
	if (XMLElement *elem = (*pos)->toElement()) {
		for (const auto &currentAttribute : elem->attributes()) {
			if (!inheritable(currentAttribute) || extracted(currentAttribute))
				continue;
			AttributeRun run(currentAttribute, pos, parent.end());
			if (run.length() >= MIN_RUN_LENGTH) {
				XMLElement::Attribute attrib = currentAttribute;
				auto groupIt = parent.wrap(run.begin(), run.end(), "g");
				auto group = (*groupIt)->toElement();
				group->addAttribute(attrib.name, attrib.value);
				// remove attribute from the grouped elements but keep it on elements with 'id' attribute
				// since they can be referenced, and keep 'fill' attribute on animation elements
				for (auto &node : group->children()) {
					XMLElement *elem = node->toElement();
					if (elem && extractable(attrib, *elem))
						elem->removeAttribute(attrib.name);
				}
				// continue with children of the new group but ignore the just extracted attribute
				_extractedAttributes.insert(attrib.name);
				execute(*group, false);
				_extractedAttributes.erase(attrib.name);
				return groupIt;
			}
		}
	}
	return pos;
}


/** Checks whether an element type is allowed to be put in a group element (<g>...</g>).
 *  For now we only consider a subset of the actually allowed set of elements.
 *  @param[in] elem name of element to check
 *  @return true if the element is groupable */
bool AttributeExtractor::groupable (const XMLElement &elem) {
	// https://www.w3.org/TR/SVG/struct.html#GElement
	static constexpr auto names = util::make_array(
		"a", "altGlyphDef", "animate", "animateColor", "animateMotion", "animateTransform",
		"circle", "clipPath", "color-profile", "cursor", "defs", "desc", "ellipse", "filter",
		"font", "font-face", "foreignObject", "g", "image", "line", "linearGradient", "marker",
		"mask", "path", "pattern", "polygon", "polyline", "radialGradient", "rect", "set",
		"style", "switch", "symbol", "text", "title", "use", "view"
	);
	return binary_search(names.begin(), names.end(), elem.getName(), [](const string &name1, const string &name2) {
		return name1 < name2;
	});
}


/** Checks whether an SVG attribute A of an element E implicitly propagates its properties
 *  to all child elements of E that don't specify A. For now we only consider a subset of
 *  the inheritable properties.
 *  @param[in] attrib name of attribute to check
 *  @return true if the attribute is inheritable */
bool AttributeExtractor::inheritable (const Attribute &attrib) {
	// subset of inheritable properties listed on https://www.w3.org/TR/SVG11/propidx.html
	// clip-path is not inheritable but can be moved to the parent element as long as
	// no child gets an different clip-path attribute
	// https://www.w3.org/TR/SVG11/styling.html#Inheritance
	static constexpr auto names = util::make_array(
		"clip-path", "clip-rule", "color", "color-interpolation", "color-interpolation-filters", "color-profile",
		"color-rendering", "direction", "fill", "fill-opacity", "fill-rule", "font", "font-family", "font-size",
		"font-size-adjust", "font-stretch", "font-style", "font-variant", "font-weight", "glyph-orientation-horizontal",
		"glyph-orientation-vertical", "letter-spacing", "paint-order", "stroke", "stroke-dasharray", "stroke-dashoffset",
		"stroke-linecap", "stroke-linejoin", "stroke-miterlimit", "stroke-opacity", "stroke-width", "transform",
		"visibility", "word-spacing", "writing-mode"
	);
	return binary_search(names.begin(), names.end(), attrib.name, [](const string &name1, const string &name2) {
		return name1 < name2;
	});
}


/** Checks whether an attribute is allowed to be removed from a given element. */
bool AttributeExtractor::extractable (const Attribute &attrib, XMLElement &element) {
	if (element.hasAttribute("id"))
		return false;
	if (attrib.name != "fill")
		return true;
	// the 'fill' attribute of animation elements has different semantics than
	// that of graphics elements => don't extract it from animation nodes
	// https://www.w3.org/TR/SVG11/animate.html#TimingAttributes
	static constexpr auto names = util::make_array(
		"animate", "animateColor", "animateMotion", "animateTransform", "set"
	);
	auto it = find_if(names.begin(), names.end(), [&](const string &name) {
		return element.getName() == name;
	});
	return it == names.end();
}


/** Returns true if a given attribute was already extracted from the
 *  current run of elements. */
bool AttributeExtractor::extracted (const Attribute &attr) const {
	return _extractedAttributes.find(attr.name) != _extractedAttributes.end();
}

/////////////////////////////////////////////////////////////////////////////

/** Recursively removes all redundant group elements from the given context element
 *  and moves the attributes to the corresponding parent element. */
void GroupCollapser::execute (XMLElement &context) {
	for (auto &node : context) {
		if (XMLElement *elem = node->toElement()) {
			execute(*elem);
			if (elem->children().size() == 1 && collapsible(*elem)) {
				if (XMLElement *child = (*elem->begin())->toElement()) {
					if (unwrappable(*child, context) && moveAttributes(*child, *elem))
						elem->unwrap(elem->begin());
				}
			}
		}
	}
}


/** Moves all attributes from an element to another one. The attributes are
 *  removed from the source. Attributes already present in the destination
 *  element are overwritten or combined.
 *  @param[in] source element the attributes are taken from
 *  @param[in] dest element that receives the attributes
 *  @return true if all attributes have been moved */
bool GroupCollapser::moveAttributes (XMLElement &source, XMLElement &dest) {
	vector<string> movedAttributes;
	for (const XMLElement::Attribute &attr : source.attributes()) {
		if (attr.name == "transform") {
			string transform;
			if (const char *destvalue = dest.getAttributeValue("transform"))
				transform = destvalue+attr.value;
			else
				transform = attr.value;
			dest.addAttribute("transform", transform);
			movedAttributes.emplace_back("transform");
		}
		else if (AttributeExtractor::inheritable(attr)) {
			dest.addAttribute(attr.name, attr.value);
			movedAttributes.emplace_back(attr.name);
		}
	}
	for (const string &attrname : movedAttributes)
		source.removeAttribute(attrname);
	return source.attributes().empty();
}


/** Returns true if a given element is allowed to take the inheritable attributes
 *  and children of a child group without changing the semantics.
 *  @param[in] element group element to check */
bool GroupCollapser::collapsible (const XMLElement &element) {
	// the 'fill' attribute of animation elements has different semantics than
	// that of graphics elements => don't collapse them
	static constexpr auto names = util::make_array(
		"animate", "animateColor", "animateMotion", "animateTransform", "set"
	);
	auto it = find_if(names.begin(), names.end(), [&](const string &name) {
		return element.getName() == name;
	});
	return it == names.end();
}


/** Returns true if a given group element is allowed to be unwrapped, i.e. its attributes
 *  and children can be moved to its parent element without changing the semantics.
 *  @param[in] element group element to check */
bool GroupCollapser::unwrappable (const XMLElement &element, const XMLElement &parent) {
	if (element.getName() != "g")
		return false;
	// check for colliding clip-path attributes
	if (const char *cp1 = element.getAttributeValue("clip-path")) {
		if (const char *cp2 = parent.getAttributeValue("clip-path")) {
			if (string(cp1) != cp2)
				return false;
		}
	}
	// these attributes prevent a group from being unwrapped
	static constexpr auto attribs = util::make_array(
		"class", "id", "filter", "mask", "style"
	);
	auto it = find_if(attribs.begin(), attribs.end(), [&](const string &name) {
		return element.hasAttribute(name);
	});
	return it == attribs.end();
}

/////////////////////////////////////////////////////////////////////////////

#include "DependencyGraph.hpp"

/** Extracts the ID from a local URL reference like url(#abcde) */
static inline string extract_id_from_url (const string &url) {
	return url.substr(5, url.length()-6);
}


/** Removes elements present in the SVG tree that are not required.
 *  For now, only clipPath elements are removed. */
void RedundantElementRemover::execute (XMLElement &defs, XMLElement &context) {
	vector<XMLElement*> clipPathElements;
	if (!defs.getDescendants("clipPath", nullptr, clipPathElements))
		return;

	// collect dependencies between clipPath elements in the defs section of the SVG tree
	DependencyGraph<string> idTree;
	for (const XMLElement *clip : clipPathElements) {
		if (const char *id = clip->getAttributeValue("id")) {
			if (const char *url = clip->getAttributeValue("clip-path"))
				idTree.insert(extract_id_from_url(url), id);
			else
				idTree.insert(id);
		}
	}
	// collect elements that reference a clipPath, i.e. have a clip-path attribute
	vector<XMLElement*> descendants;
	context.getDescendants(nullptr, "clip-path", descendants);
	// remove referenced IDs and their dependencies from the dependency graph
	for (const XMLElement *elem : descendants) {
		string idref = extract_id_from_url(elem->getAttributeValue("clip-path"));
		idTree.removeDependencyPath(idref);
	}
	descendants.clear();
	for (const string &str : idTree.getKeys()) {
		XMLElement *node = defs.getFirstDescendant("clipPath", "id", str.c_str());
		defs.remove(node);
	}
}