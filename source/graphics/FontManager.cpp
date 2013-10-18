/* Copyright (C) 2013 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "FontManager.h"

#include "graphics/Font.h"
#include "graphics/TextureManager.h"
#include "ps/CLogger.h"
#include "ps/CStr.h"
#include "ps/Filesystem.h"
#include "renderer/Renderer.h"

shared_ptr<CFont> CFontManager::LoadFont(const CStrW& fontName)
{
	FontsMap::iterator it = m_Fonts.find(fontName);
	if (it != m_Fonts.end())
		return it->second;

	shared_ptr<CFont> font(new CFont());

	if (!ReadFont(font.get(), fontName))
	{
		// Fall back to default font (unless this is the default font)
		if (fontName == L"sans-10")
			font.reset();
		else
			font = LoadFont(L"sans-10");
	}

	m_Fonts[fontName] = font;
	return font;
}

bool CFontManager::ReadFont(CFont* font, const CStrW& fontName)
{
	const VfsPath path(L"fonts/");

	// Read font definition file into a stringstream
	shared_ptr<u8> buf;
	size_t size;
	const VfsPath fntName(fontName + L".fnt");
	if (g_VFS->LoadFile(path / fntName, buf, size) < 0)
	{
		LOGERROR(L"Failed to open font file %ls", (path / fntName).string().c_str());
		return false;
	}
	std::istringstream FNTStream(std::string((const char*)buf.get(), size));

	int Version;
	FNTStream >> Version;
	if (Version != 101) // Make sure this is from a recent version of the font builder
	{
		LOGERROR(L"Font %ls has invalid version", fontName.c_str());
		return 0;
	}

	int TextureWidth, TextureHeight;
	FNTStream >> TextureWidth >> TextureHeight;

	std::string Format;
	FNTStream >> Format;
	if (Format == "rgba")
		font->m_HasRGB = true;
	else if (Format == "a")
		font->m_HasRGB = false;
	else
		debug_warn(L"Invalid .fnt format string");

	int NumGlyphs;
	FNTStream >> NumGlyphs;

	FNTStream >> font->m_LineSpacing;
	FNTStream >> font->m_Height;

	for (int i = 0; i < NumGlyphs; ++i)
	{
		int          Codepoint, TextureX, TextureY, Width, Height, OffsetX, OffsetY, Advance;
		FNTStream >> Codepoint>>TextureX>>TextureY>>Width>>Height>>OffsetX>>OffsetY>>Advance;

		if (Codepoint < 0 || Codepoint > 0xFFFF)
		{
			LOGWARNING(L"Font %ls has invalid codepoint 0x%x", fontName.c_str(), Codepoint);
			continue;
		}

		float u = (float)TextureX / (float)TextureWidth;
		float v = (float)TextureY / (float)TextureHeight;
		float w = (float)Width  / (float)TextureWidth;
		float h = (float)Height / (float)TextureHeight;

		CFont::GlyphData g = { u, -v, u+w, -v+h, (i16)OffsetX, (i16)-OffsetY, (i16)(OffsetX+Width), (i16)(-OffsetY+Height), (i16)Advance };
		font->m_Glyphs[(u16)Codepoint] = g;
	}

	ENSURE(font->m_Height); // Ensure the height has been found (which should always happen if the font includes an 'I')

	// Load glyph texture
	const VfsPath imgName(fontName + L".png");
	CTextureProperties textureProps(path / imgName);
	textureProps.SetFilter(GL_NEAREST);
	if (!font->m_HasRGB)
		textureProps.SetFormatOverride(GL_ALPHA);
	font->m_Texture = g_Renderer.GetTextureManager().CreateTexture(textureProps);

	return true;
}
