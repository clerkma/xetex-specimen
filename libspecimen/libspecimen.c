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
#include <string.h>
#include <stdint.h>
#include "parson.h"

typedef struct {
    uint32_t size;
    uint32_t * vals;
} specimen_entry_t;

typedef struct {
    uint32_t size;
    specimen_entry_t * table;
} specimen_hash_t;

typedef struct specimen {
    JSON_Value * root_value;
    JSON_Array * spec_file;
    JSON_Array * spec_link;
    JSON_Array * spec_fontset;
    specimen_hash_t * link_hash;
    specimen_hash_t * fontset_hash;
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

#define HASH_SIZE 2100
#define HASH_PRIME 1777

static specimen_hash_t * specimen_hash_init()
{
    specimen_hash_t * table = NULL;
    table = calloc(1, sizeof(specimen_hash_t));
    if (table)
    {
        table->size = HASH_SIZE;
        table->table = calloc(HASH_SIZE, sizeof(specimen_entry_t));
    }

    return table;
}

static void specimen_hash_tini(specimen_hash_t * table)
{
    int i = 0;
    if (table)
    {
        for (i = 0; i < HASH_SIZE; i++)
        {
            if(table->table[i].vals)
                free(table->table[i].vals);
        }
        free(table->table);
        free(table);
    }
}

static void specimen_hash_insert(specimen_hash_t * table, JSON_Object * entry)
{
    int key = (int) json_object_get_number(entry, "key");
    JSON_Array * vals = json_object_get_array(entry, "vals");
    int idx;

    if (table)
    {
        int count = json_array_get_count(vals);
        table->table[key].size = count;
        table->table[key].vals = calloc(count, sizeof(uint32_t));
        for (idx = 0; idx < count; idx++)
        {
            table->table[key].vals[idx] = (int) json_array_get_number(vals, idx);
        }
    }
}

static specimen_entry_t * specimen_hash_lookup(specimen_hash_t * table, char * key)
{
    if (key == NULL || strlen(key) == 0)
        return NULL;
    else
    {
        int code = calc_hash_code(key);
        return table->table + code;
    }
}

static char * specimen_database_path(void)
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

static int calc_hash_code(char * text)
{
    int h = (unsigned char) text[0];
    for (int i = 1; i < strlen(text); i++)
    {
        h = h + h + (unsigned char) text[i];
        while (h >= HASH_PRIME)
            h = h - HASH_PRIME;
    }

    return h;
}

specimen_t * specimen_init(void)
{
    JSON_Value * root_value = NULL;
    JSON_Object * root_object;
    char * fontdb_path = specimen_database_path();
    root_value = json_parse_file(fontdb_path);
    specimen_t * spec = NULL;
    int i;
    
    if (root_value)
    {
        root_object = json_value_get_object(root_value);
        spec = calloc(1, sizeof(struct specimen));
        spec->spec_file = json_object_get_array(root_object, "file");
        spec->spec_link = json_object_get_array(root_object, "link");
        spec->spec_fontset = json_object_get_array(root_object, "fontset");
        spec->link_hash = specimen_hash_init();
        spec->fontset_hash = specimen_hash_init();

        JSON_Array * link_hash = json_object_get_array(root_object, "link_hash");
        for (i = 0; i < json_array_get_count(link_hash); i++)
        {
            JSON_Object * entry = json_array_get_object(link_hash, i);
            specimen_hash_insert(spec->link_hash, entry);
        }

        JSON_Array * fontset_hash = json_object_get_array(root_object, "fontset_hash");
        for (i = 0; i < json_array_get_count(fontset_hash); i++)
        {
            JSON_Object * entry = json_array_get_object(fontset_hash, i);
            specimen_hash_insert(spec->fontset_hash, entry);
        }
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
        specimen_hash_tini(spec->link_hash);
        specimen_hash_tini(spec->fontset_hash);
        free(spec);
    }
}

specimen_font_t * specimen_search_name(specimen_t * spec, char * name)
{
    if (spec)
    {
        specimen_entry_t * entry = specimen_hash_lookup(spec->link_hash, name);
        if (entry)
        {
            JSON_Array * spec_link = spec->spec_link;
            int idx = 0;

            for (idx = 0; idx < entry->size; idx++)
            {
                JSON_Object * spec_link_entry = json_array_get_object(spec_link, entry->vals[idx]);
                char * entry_name = (char *) json_object_get_string(spec_link_entry, "name");
                if (strcmp(name, entry_name) == 0)
                {
                    JSON_Array * entry_inst = json_object_get_array(spec_link_entry, "inst");
                    int inst0 = json_array_get_number(entry_inst, 0);
                    return (specimen_font_t *) json_array_get_object(spec->spec_file, inst0);
                }
            }
        }
    }
    return NULL;
}

specimen_fontset_t * specimen_search_family(specimen_t * spec, char * name)
{
    if (spec)
    {
        specimen_entry_t * entry = specimen_hash_lookup(spec->fontset_hash, name);
        if (entry)
        {
            JSON_Array * spec_fontset = spec->spec_fontset;
            int idx = 0;

            for (idx = 0; idx < entry->size; idx++)
            {
                JSON_Object * spec_fontset_entry = json_array_get_object(spec_fontset, entry->vals[idx]);
                char * entry_name = json_object_get_string(spec_fontset_entry, "name");
                if (strcmp(name, entry_name) == 0)
                {
                    JSON_Array * entry_inst = json_object_get_array(spec_fontset_entry, "inst");
                    return (specimen_fontset_t *) entry_inst;
                }
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
