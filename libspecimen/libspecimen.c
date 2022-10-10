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
#include <stdio.h>
#include <stdlib.h>
#include "parson.h"

typedef struct specimen {
    JSON_Value * root_value;
    JSON_Array * spec_file;
    JSON_Array * spec_link;
    JSON_Array * spec_fontset;
} specimen;

typedef struct {
    JSON_Object * font;
} specimen_font;

typedef struct {
    JSON_Array * fontset;
} specimen_fontset;

typedef struct specimen specimen_t;
typedef struct specimen_font specimen_font_t;
typedef struct specimen_fontset specimen_fontset_t;

#define SPECIMEN_KEY_FAMILY        0
#define SPECIMEN_KEY_STYLE         1
#define SPECIMEN_KEY_FULLNAME      2
#define SPECIMEN_KEY_PREFER_FAMILY 3
#define SPECIMEN_KEY_PREFER_STYLE  4
#define SPECIMEN_KEY_POSTSCRIPT    5

#define SPECIMEN_DATABASE_FILE "xetex-fontdb.json"

static const char * specimen_database_path(void)
{
    char * appdata = getenv("APPDATA");
    size_t appdata_len = strlen(appdata);
    if (appdata_len)
    {
        char last = appdata[appdata_len - 1];
        char * path;
        if (last == '\\' || last == '/')
        {
            path = calloc(appdata_len + 1 + 17, sizeof(char));
            sprintf(path, "%s%s", appdata, SPECIMEN_DATABASE_FILE);
        }
        else
        {
            path = calloc(appdata_len + 2 + 17, sizeof(char));
            sprintf(path, "%s\\%s", appdata, SPECIMEN_DATABASE_FILE);
        }
        return path;
    }
    return NULL;
}

specimen_t * specimen_init(void)
{
    JSON_Value * root_value = NULL;
    JSON_Object * root_object;
    char * fontdb_path = specimen_database_path();
    root_value = json_parse_file(fontdb_path);
    specimen_t * spec = NULL;
    
    if (root_value)
    {
        root_object = json_value_get_object(root_value);
        spec = calloc(1, sizeof(struct specimen));
        spec->spec_file = json_object_get_array(root_object, "file");
        spec->spec_link = json_object_get_array(root_object, "link");
        spec->spec_fontset = json_object_get_array(root_object, "fontset");
    }

    if (fontdb_path)
        free(fontdb_path);

    return spec;
}

void specimen_tini(specimen_t * spec)
{
    if (spec)
    {
        json_value_free(spec->root_value);
        free(spec);
    }
}

specimen_font_t * specimen_search_name(specimen_t * spec, const char * name)
{
    if (spec && spec->spec_link)
    {
        int i = 0;
        int count = json_array_get_count(spec->spec_link);
        for (i = 0; i < count; i++)
        {
            const JSON_Object * font_object = json_array_get_object(spec->spec_link, i);
            const char * font_name = json_object_get_string(font_object, "name");
            JSON_Array * font_inst = json_object_get_array(font_object, "inst");
            if (strcmp(font_name, name) == 0)
            {
                int font_value = (int) json_array_get_number(font_inst, 0);
                return (specimen_font_t *) json_array_get_object(spec->spec_file, font_value);
            }
        }
    }
    return NULL;
}

specimen_fontset_t * specimen_search_family(specimen_t * spec, const char * name)
{
    if (spec)
    {
        int i = 0;
        int count = json_array_get_count(spec->spec_fontset);
        for (i = 0; i < count; i++)
        {
            JSON_Object * font_object = json_array_get_object(spec->spec_fontset, i);
            const char * font_name = json_object_get_string(font_object, "name");
            JSON_Array * font_inst = json_object_get_array(font_object, "inst");
            if (strcmp(font_name, name) == 0)
            {
                return (specimen_fontset_t *) font_inst;
            }
        }
    }
    return NULL;
}

static JSON_Array * specimen_get_array_of_key(specimen_font_t * spec_font, int key)
{
    JSON_Array * key_array = NULL;
    switch (key)
    {
        case SPECIMEN_KEY_FAMILY:
            key_array = json_object_get_array((JSON_Object *) spec_font, "family");
            break;
        case SPECIMEN_KEY_STYLE:
            key_array = json_object_get_array((JSON_Object *) spec_font, "style");
            break;
        case SPECIMEN_KEY_FULLNAME:
            key_array = json_object_get_array((JSON_Object *) spec_font, "full");
            break;
        case SPECIMEN_KEY_PREFER_FAMILY:
            key_array = json_object_get_array((JSON_Object *) spec_font, "prefer_family");
            break;
        case SPECIMEN_KEY_PREFER_STYLE:
            key_array = json_object_get_array((JSON_Object *) spec_font, "prefer_style");
            break;
        case SPECIMEN_KEY_POSTSCRIPT:
            key_array = json_object_get_array((JSON_Object *) spec_font, "postscript");
            break;
    }
    return key_array;
}

/* font APIs */
int specimen_font_get_name_count(specimen_font_t * spec_font, int key)
{
    JSON_Array * key_array = specimen_get_array_of_key(spec_font, key);
    if (key_array)
        return json_array_get_count(key_array);
    return 0;
}

const char * specimen_font_get_name(specimen_font_t * spec_font, int key, int index)
{
    JSON_Array * key_array = specimen_get_array_of_key(spec_font, key);
    if (key_array)
        return json_array_get_string(key_array, index);
    return NULL;
}

const char * specimen_font_get_path(specimen_font_t * spec_font)
{
    return json_object_get_string((JSON_Object *) spec_font, "path");
}

int specimen_font_get_index(specimen_font_t * spec_font)
{
    return (int) json_object_get_number((JSON_Object *) spec_font, "index");
}

int specimen_fontset_get_count(specimen_fontset_t * spec_set)
{
    return json_array_get_count((JSON_Array *) spec_set);
}

specimen_font_t * specimen_fontset_get_font(specimen_t * spec, specimen_fontset_t * spec_set, int index)
{
    int font_key = (int) json_array_get_number((JSON_Array *) spec_set, index);
    return (specimen_font_t *) json_array_get_object(spec->spec_file, font_key);
}
