<?xml version="1.0"?>
<!-- *********************************************************************
** Stylesheet to rearrange font-/path-elements in an SVG file.          **
** It's part of the dvisvgm package.                                    **
** Copyright (C) 2009-2017 Martin Gieseking <martin.gieseking@uos.de>   **
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
**                                                                      **
** The code generated by this script is also licensed under the terms   **
** of the GNU general public license version 3 or later.                **
***********************************************************************-->
<xsl:stylesheet version="1.0" 
	xmlns="http://www.w3.org/2000/svg"
	xmlns:svg="http://www.w3.org/2000/svg"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:xlink="http://www.w3.org/1999/xlink"
	xmlns:exsl="http://exslt.org/common"
	xmlns:str="http://exslt.org/strings"
	xmlns:mg="my-namespace"
	extension-element-prefixes="exsl str"
	exclude-result-prefixes="svg xlink exsl str mg">

	<xsl:key name="path-by-id" match="/svg:defs/svg:path" use="@id"/>


	<xsl:variable name="styles-rtf">
		<xsl:for-each select="str:tokenize(/*/svg:style[@type='text/css'], '&#10;')">
			<xsl:sort select="substring-after(., ' ')"/>
			<mg:style new-id="f{position()}" id="{substring-before(substring-after(., '.'), ' ')}">
				<xsl:value-of select="substring-after(., ' ')"/>
			</mg:style>
		</xsl:for-each>
	</xsl:variable>

	<xsl:variable name="styles" select="exsl:node-set($styles-rtf)/mg:style"/>

	<xsl:variable name="num-paths" select="count(/*/svg:defs/svg:path)"/>

	<xsl:variable name="defs-rtf">
		<xsl:for-each select="/*/svg:defs/svg:path">
			<xsl:sort select="@d"/>
			<mg:path id="{@id}" new-id="g{position()}"/>
		</xsl:for-each>
		<xsl:for-each select="/*/svg:defs/svg:use">
			<xsl:sort select="concat(key('path-by-id', substring(@xlink:href, 2))/@d, @transform)"/>
			<mg:use id="{@id}" new-id="g{position()+$num-paths}"/>
		</xsl:for-each>
	</xsl:variable>

	<xsl:variable name="defs" select="exsl:node-set($defs-rtf)"/>


	<xsl:template match="*|@*">
		<xsl:copy>
			<xsl:apply-templates select="@*|node()"/>
		</xsl:copy>
	</xsl:template>


	<xsl:template match="svg:defs[svg:path]">
		<xsl:copy>
			<xsl:apply-templates select="svg:path">
				<xsl:sort select="@d"/>
			</xsl:apply-templates>
			<xsl:apply-templates select="svg:use">
				<xsl:sort select="$defs/mg:use[@id=current()/@id]/@new-id"/>
			</xsl:apply-templates>
		</xsl:copy>
	</xsl:template>


	<xsl:template match="svg:defs[svg:font]">
		<xsl:copy>
			<xsl:apply-templates>
				<xsl:sort select="@id"/>
			</xsl:apply-templates>
		</xsl:copy>
	</xsl:template>


	<xsl:template match="svg:defs/svg:path">
		<path id="{$defs/mg:path[@id=current()/@id]/@new-id}" d="{@d}"/>
	</xsl:template>


	<xsl:template match="svg:defs/text()"/>


	<xsl:template match="svg:use">
		<xsl:variable name="href" select="substring(@xlink:href, 2)"/>
		<xsl:copy>
			<xsl:apply-templates select="@*"/>
			<xsl:if test="ancestor::svg:defs">
				<xsl:attribute name="id">
					<xsl:value-of select="$defs/mg:use[@id=current()/@id]/@new-id"/>
				</xsl:attribute>
			</xsl:if>
			<xsl:attribute name="xlink:href">
				<xsl:value-of select="concat('#', $defs/*[@id=$href]/@new-id)"/>
			</xsl:attribute>
		</xsl:copy>
	</xsl:template>


	<xsl:template match="svg:style[@type='text/css']">
		<style type="text/css">
			<xsl:for-each select="$styles">
				<xsl:value-of select="concat('text.f', position(), ' ', ., '&#10;')"/>
			</xsl:for-each>
		</style>
	</xsl:template>


	<xsl:template match="svg:text">
		<xsl:copy>
			<xsl:copy-of select="@*"/>
			<xsl:attribute name="class">
				<xsl:value-of select="$styles[@id=current()/@class]/@new-id"/>
			</xsl:attribute>
			<xsl:apply-templates/>
		</xsl:copy>
	</xsl:template>
</xsl:stylesheet>

