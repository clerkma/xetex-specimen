# Copyright (c) 2022, 2023 Clerk Ma
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


def parse_name(data, inst):
    if not data:
        return None
    style_dict = {}
    psname_dict = {}
    user_tuple_list = []
    if inst:
        version = unpack_from(">2H", inst, 0)
        if version == (1, 0):
            offset, axis_count, axis_size, inst_count, inst_size = unpack_from(">H2x4H", inst, 4)
            offset += axis_count * axis_size
            user_tuple_size = axis_count * 4
            inst_name_size = inst_size - user_tuple_size
            psname_exist = inst_name_size == 6
            for i in range(inst_count):
                style = unpack_from(">H", inst, offset)[0]
                user_tuple = unpack_from(">%dL" % axis_count, inst, offset + 2)
                user_tuple_list.append(user_tuple)
                style_dict[style] = i
                if psname_exist:
                    psname = unpack_from(">H", inst, offset + 4 + user_tuple_size)[0]
                    psname_dict[psname] = i
                offset += inst_size
    version, count, offset = unpack_from(">3H", data, 0)
    name_list = []
    if version in [0, 1]:
        pos = 6
        for _ in range(count):
            one_name = Name._make(unpack_from(">6H", data, pos))
            id_list = [1, 2, 4, 6, 16, 17] + list(style_dict.keys()) + list(psname_dict.keys())
            if one_name.name in id_list:
                spos = offset + one_name.offset
                epos = spos + one_name.length
                one_blob = data[spos:epos]
                text = ""
                if one_name[:2] == (1, 0) and one_name.language == 0:
                    text = one_blob.decode("mac_roman")
                elif one_name.platform in [0, 3]:
                    text = one_blob.decode("utf_16_be")
                if text and "\x00" in text:
                    text = text.replace("\x00", "")
                if text:
                    name_list.append((*one_name[:4], text))
            pos += 12
    return name_list, style_dict, psname_dict, user_tuple_list


def parse_ot_name(data, offset=0):
    pos = offset + 4
    table_count = unpack_from(">H", data, pos)[0]
    pos = offset + 12
    name_blob = b""
    fvar_blob = b""
    for _ in range(table_count):
        one_rec = Record._make(unpack_from(">4s3L", data, pos))
        pos += 16
        if one_rec.tag == b"name":
            name_blob = data[one_rec.offset:one_rec.offset+one_rec.length]
        if one_rec.tag == b"fvar":
            fvar_blob = data[one_rec.offset:one_rec.offset+one_rec.length]
    return parse_name(name_blob, fvar_blob)


def prepare_data(data, offset):
    name_list, style_dict, psname_list, user_tuple_list = parse_ot_name(data, offset)
    key_map = {1: "family", 2: "style", 4: "full", 6: "postscript", 16: "prefer_family", 17: "prefer_style"}
    name_dict = {
        "family": [], "style": [],
        "full": [], "postscript": [],
        "prefer_family": [], "prefer_style": [],
        "inst_style": [], "inst_postscript": [],
        "inst_tuple": user_tuple_list
    }
    for one in name_list:
        name_id, name_str = one[3:5]
        if name_id in key_map:
            entry = name_dict[key_map[name_id]]
            if name_str not in entry:
                entry.append(name_str)
        elif name_id in style_dict: # name, instance_index
            name_dict["inst_style"].append((name_str, style_dict[name_id]))
        elif name_id in psname_list:
            name_dict["inst_postscript"].append((name_str, psname_list[name_id]))
    return name_dict


def store_data(context, data, path, index):
    context["file"].append({"path": path, "index": index, **data})


def parse_opentype_file(context, path):
    with open(path, "rb") as src:
        file_data = src.read()
        file_magic = file_data[:4]
        if file_magic in [b"OTTO", b"\x00\x01\x00\x00"]:
            name_data = prepare_data(file_data, 0)
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
        for r, _, fs in os.walk(one_path):
            for f in fs:
                one_name = os.path.normpath(os.path.join(r, f))
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
        if one_file["prefer_family"]:
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
    out += fl
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
    isl = entry["inst_style"]
    p = []
    if len(pfl) and len(psl):
        p = join_name(pfl, psl)
    n = []
    if len(nfl) and len(nsl):
        n = join_name(nfl, nsl)
    i = []
    if len(nfl) and len(isl):
        i = join_name(nfl, [x[0] for x in isl])
    debug = False
    if len(p) > 0 and debug:
        print("path:", entry["path"])
        print("full:", entry["full"])
        print("prefer:", ", ".join(p))
        print("normal:", ", ".join(n))
        print("\n")
    for one in entry["inst_postscript"]:
        name, inst_idx = one
        store_prelink(context, name, index)
    for one in entry["postscript"]:
        store_prelink(context, one, index)
    for one in entry["full"]:
        store_prelink(context, one, index)
    for one in i:
        store_prelink(context, one, index)
    if p:
        for one in p:
            store_prelink(context, one, index)
    else:
        for one in n:
            store_prelink(context, one, index) 


if __name__ == "__main__":
    user_path_list = [
        "C:\\texlive\\2023\\texmf-dist\\fonts\\opentype",
        "C:\\texlive\\2023\\texmf-dist\\fonts\\truetype",
        "C:\\windows\\fonts"
    ]
    parse(user_path_list)
