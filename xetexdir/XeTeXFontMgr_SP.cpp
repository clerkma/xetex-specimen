/*
 Copyright (c) 2022 Clerk Ma

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/
#include <w2c/config.h>

#include "XeTeXFontMgr_SP.h"

XeTeXFontMgr::NameCollection*
XeTeXFontMgr_SP::readNames(specimen_font_t * spec_font)
{
    NameCollection* names = new NameCollection;
    int fullname_count = specimen_font_get_name_count(spec_font, SPECIMEN_KEY_FULLNAME);
    int prefer_family_count = specimen_font_get_name_count(spec_font, SPECIMEN_KEY_PREFER_FAMILY);
    int prefer_style_count = specimen_font_get_name_count(spec_font, SPECIMEN_KEY_PREFER_STYLE);
    int family_count = specimen_font_get_name_count(spec_font, SPECIMEN_KEY_FAMILY);
    int style_count = specimen_font_get_name_count(spec_font, SPECIMEN_KEY_STYLE);
    int postscript_count = specimen_font_get_name_count(spec_font, SPECIMEN_KEY_POSTSCRIPT);
    int i = 0;

    if (prefer_family_count > 0 && prefer_style_count > 0)
    {
        for (i = 0; i < prefer_family_count; i++)
            appendToList(&names->m_familyNames, specimen_font_get_name(spec_font, SPECIMEN_KEY_PREFER_FAMILY, i));
        for (i = 0; i < prefer_style_count; i++)
            appendToList(&names->m_styleNames, specimen_font_get_name(spec_font, SPECIMEN_KEY_PREFER_STYLE, i));
    }
    else
    {
        for (i = 0; i < family_count; i++)
            appendToList(&names->m_familyNames, specimen_font_get_name(spec_font, SPECIMEN_KEY_FAMILY, i));
        for (i = 0; i < style_count; i++)
            appendToList(&names->m_styleNames, specimen_font_get_name(spec_font, SPECIMEN_KEY_STYLE, i));
    }

    for (i = 0; i < fullname_count; i++)
        appendToList(&names->m_fullNames, specimen_font_get_name(spec_font, SPECIMEN_KEY_FULLNAME, i));
    
    if (postscript_count > 0)
        names->m_psName = specimen_font_get_name(spec_font, SPECIMEN_KEY_POSTSCRIPT, 0);

    return names;
}

void
XeTeXFontMgr_SP::cacheFamilyMembers(const std::list<std::string>& familyNames)
{
    for (std::list<std::string>::const_iterator j = familyNames.begin(); j != familyNames.end(); ++j)
    {
        specimen_fontset_t * fontset = specimen_search_family(spec, j->c_str());
        if (fontset != NULL)
        {
            int count = specimen_fontset_get_count(fontset);
            int i = 0;
            for (i = 0; i < count; i++)
            {
                specimen_font_t * font = specimen_fontset_get_font(spec, fontset, i);
                NameCollection* names = readNames(font);
                addToMaps(font, names);
                delete names;
            }
        }
    }
}

void
XeTeXFontMgr_SP::searchForHostPlatformFonts(const std::string& name)
{
    specimen_font_t * font = specimen_search_name(spec, name.c_str());
    if (font != NULL)
    {
        NameCollection* names = readNames(font);
        addToMaps(font, names);
        cacheFamilyMembers(names->m_familyNames);
        delete names;
    }
}

void
XeTeXFontMgr_SP::initialize()
{
    spec = specimen_init();
}

void
XeTeXFontMgr_SP::terminate()
{
    specimen_tini(spec);
}

std::string
XeTeXFontMgr_SP::getPlatformFontDesc(PlatformFontRef font) const
{
    if (font == NULL)
        return "[unknown]";
    else
    {
        return specimen_font_get_path(font);
    }
}
