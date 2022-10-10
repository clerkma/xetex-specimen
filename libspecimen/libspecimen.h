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
#ifndef libspecimen_h
#define libspecimen_h

#ifdef __cplusplus
extern "C"
{
#endif

#define SPECIMEN_VERSION "20221009"

#define SPECIMEN_KEY_FAMILY        0
#define SPECIMEN_KEY_STYLE         1
#define SPECIMEN_KEY_FULLNAME      2
#define SPECIMEN_KEY_PREFER_FAMILY 3
#define SPECIMEN_KEY_PREFER_STYLE  4
#define SPECIMEN_KEY_POSTSCRIPT    5

typedef struct specimen specimen_t;
typedef struct specimen_font specimen_font_t;
typedef struct specimen_fontset specimen_fontset_t;

specimen_t * specimen_init(void);
void specimen_tini(specimen_t * spec);

specimen_font_t * specimen_search_name(specimen_t * spec, const char * name);
specimen_fontset_t * specimen_search_family(specimen_t * spec, const char * name);

/* font APIs */
int specimen_font_get_name_count(specimen_font_t * spec_font, int key);
const char * specimen_font_get_name(specimen_font_t * spec_font, int key, int index);
const char * specimen_font_get_path(specimen_font_t * spec_font);
int specimen_font_get_index(specimen_font_t * spec_font);

int specimen_fontset_get_count(specimen_fontset_t * spec_set);
specimen_font_t * specimen_fontset_get_font(specimen_t * spec, specimen_fontset_t * spec_set, int index);

#ifdef __cplusplus
}
#endif

#endif