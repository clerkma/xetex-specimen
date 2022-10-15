# Copyright (c) 2022 Clerk Ma
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
from collections import namedtuple
from struct import unpack_from
import os
import json

HASH_SIZE = 2100
HASH_PRIME = 1777
Record = namedtuple("Record", "tag, checksum, offset, length")
Name = namedtuple("Name", "platform, encoding, language, name, length, offset")


def calc_hash_code(key):
    key_u8 = key.encode("U8")
    h = key_u8[0]
    for b in key_u8[1:]:
        h = h + h + b
        while h >= HASH_PRIME:
            h = h - HASH_PRIME
    return h


def parse_name(data):
    if len(data) == 0:
        return None
    version, count, offset = unpack_from(">3H", data, 0)
    name_list = []
    if version in [0, 1]:
        pos = 6
        for i in range(count):
            one_name = Name._make(unpack_from(">6H", data, pos))
            if one_name.name in [1, 2, 4, 6, 16, 17]:
                spos = offset + one_name.offset
                epos = spos + one_name.length
                one_blob = data[spos:epos]
                text = ""
                enc_tuple = (one_name.platform, one_name.encoding)
                if enc_tuple == (1, 0) and one_name.language == 0:
                    text = one_blob.decode("mac_roman")
                elif one_name.platform in [0, 3]:
                    text = one_blob.decode("utf_16_be")
                if text != "":
                    if "\x00" in text:
                        text = text.replace("\x00", "")
                    name_list.append((one_name.platform,
                                      one_name.encoding,
                                      one_name.language,
                                      one_name.name, text))
            pos += 12
    return name_list


def parse_ot_name(data, offset=0):
    pos = offset + 4
    table_count = unpack_from(">H", data, pos)[0]
    pos = offset + 12
    name_blob = b""
    for x in range(table_count):
        one_rec = Record._make(unpack_from(">4s3L", data, pos))
        pos += 16
        if one_rec.tag == b"name":
            name_blob = data[one_rec.offset:one_rec.offset+one_rec.length]
    return parse_name(name_blob)


def prepare_data(data, offset):
    name_list = parse_ot_name(data, offset)
    key_map = {1: "family", 2: "style", 4: "full", 6: "postscript", 16: "prefer_family", 17: "prefer_style"}
    name_dict = {"family": [], "style": [], "full": [], "postscript": [], "prefer_family": [], "prefer_style": []}
    for one in name_list:
        name_id = one[3]
        name_str = one[4]
        if name_id in key_map:
            entry = name_dict[key_map[name_id]]
            if name_str not in entry:
                entry.append(name_str)
    return name_dict


def store_data(context, data, path, index):
    context["file"].append({"path": path, "index": index, **data})


def parse_opentype_file(context, path):
    with open(path, "rb") as src:
        file_data = src.read()
        file_magic = file_data[:4]
        if file_magic in [b"OTTO", b"\x00\x01\x00\x00"]:
            name_data = prepare_data(file_data, 0)
            for x in name_data:
                if "\x00" in x[-1]:
                    print(path, x)
            store_data(context, name_data, path, 0)
        elif file_magic == b"ttcf":
            directory_count = unpack_from(">L", file_data, 8)[0]
            directory_offset_list = unpack_from(">%dL" % directory_count, file_data, 12)
            for one_idx, one_offset in enumerate(directory_offset_list):
                name_data = prepare_data(file_data, one_offset)
                store_data(context, name_data, path, one_idx)


def parse(path_list):
    file_list = []
    for one_path in path_list:
        for r, ds, fs in os.walk(one_path):
            for f in fs:
                one_name = os.path.join(r, f)
                if one_name not in file_list:
                    file_list.append(one_name)
    context = {"file": [], "link": {}, "fontset": {}}
    for one_name in file_list:
        parse_opentype_file(context, one_name)

    for one_idx, one_file in enumerate(context["file"]):
        calc_prelink(context, one_file, one_idx)

    context["file_count"] = len(context["file"])
    context["link_count"] = len(context["link"])
    for one_idx, one_file in enumerate(context["file"]):
        if len(one_file["prefer_family"]) > 0:
            run = one_file["prefer_family"]
        else:
            run = one_file["family"]
        for x in run:
            if x not in context["fontset"]:
                context["fontset"][x] = []
            if one_idx not in context["fontset"][x]:
                context["fontset"][x].append(one_idx)
    mono = []
    for k, v in context["fontset"].items():
        if len(v) == 1:
            mono.append(k)
    for k in mono:
        del context["fontset"][k]
    link_list = []
    for k, v in context["link"].items():
        link_list.append({"name": k, "inst": v})
    set_list = []
    for k, v in context["fontset"].items():
        set_list.append({"name": k, "inst": v})
    context["link"] = link_list
    context["fontset"] = set_list

    context["link_hash"] = store_to_hash_table(link_list)
    context["fontset_hash"] = store_to_hash_table(set_list)

    font_database_path = os.path.join(os.getenv("APPDATA"), "xetex-fontdb.json")
    print("font_database @ '%s'" % font_database_path)
    print("flushed %d fonts." % len(file_list))
    with open(font_database_path, "w", encoding="U8") as out:
        out.write(json.dumps(context))


def store_to_hash_table(src):
    hash = {}
    for idx, ent in enumerate(src):
        k = calc_hash_code(ent["name"])
        if k not in hash:
            hash[k] = []
        hash[k].append(idx)
    hash_list = []
    for k in sorted(hash.keys()):
        hash_list.append({"key": k, "vals": hash[k]})
    return hash_list


def join_name(fl, sl):
    out = []
    for f in fl:
        for s in sl:
            n = "%s-%s" % (f, s)
            if n not in out:
                out.append(n)
    return out


def store_prelink(context, key, index):
    prelink = context["link"]
    if key not in prelink:
        prelink[key] = []
    if index not in prelink[key]:
        prelink[key].append(index)


def calc_prelink(context, entry, index):
    nfl, nsl = entry["family"], entry["style"]
    pfl, psl = entry["prefer_family"], entry["prefer_style"]
    p = []
    if len(pfl) > 0 and len(psl) > 0:
        p = join_name(pfl, psl)
    n = []
    if len(nfl) > 0 and len(nsl) > 0:
        n = join_name(nfl, nsl)
    debug = False
    if len(p) > 0 and debug:
        print("path:", entry["path"])
        print("full:", entry["full"])
        print("prefer:", ", ".join(p))
        print("normal:", ", ".join(n))
        print("\n")
    for one in entry["postscript"]:
        store_prelink(context, one, index)
    for one in entry["full"]:
        store_prelink(context, one, index)
    if len(p) > 0:
        for one in p:
            store_prelink(context, one, index)
    else:
        for one in n:
            store_prelink(context, one, index) 


if __name__ == "__main__":
    user_path_list = [
        r"C:\texlive\2022\texmf-dist\fonts\opentype",
        r"C:\texlive\2022\texmf-dist\fonts\truetype",
        r"C:\windows\fonts"
    ]
    parse(user_path_list)

