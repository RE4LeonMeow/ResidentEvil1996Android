# -*- coding: utf-8 -*-
# jpn_font_table.py - glyph map of the Japanese Biohazard PC font (data\FONT.TIM)
#
# FONT.TIM is a 768x256 4bpp TIM. Two glyph regions hold the 14x14 game-text
# font, both laid out as 18 columns of 14x14 cells:
#
#   LEFT  page (VRAM u 0..251)   rows at v = 28 + r*14, r = 0..15  (16 rows)
#   RIGHT page (VRAM u 256..507) rows at v =      r*14, r = 0..17  (18 rows)
#
# The renderers (PrintFormattedText 0x004912c0, RenderTextBuffer 0x00491490,
# UpdateMessageDisplay's draw pass 0x00492360) reach them as:
#
#   plain byte b (0x0C..0xF7)  LEFT  row b/18,          col b%18
#   0xF8 nn                    LEFT  row nn/18 + 13,    col nn%18
#   0xF9 nn                    RIGHT row nn/18,         col nn%18
#   0xFA nn                    RIGHT row nn/18 + 14,    col nn%18
#
# `None` marks a cell that has no Unicode spelling (controller symbols, the
# blank cells, the two halves of the wide dash).
#
# Verification status. 341 of the 598 named cells are PROVEN: every one of the
# 63 global messages, 79 item descriptions and 128 item names in Biohazard.exe
# decodes through this table into correct Japanese AND re-encodes to the
# original bytes (`python tools/jpn_msg_decode.py verify`). The other 257 cells
# are not used by any table in the executable - they are read from this font by
# RDT room text - and were transcribed from the bitmap. Read them as good but not
# proven; the ones worth a second look are R[16][15] (potamos radical, 潟 or
# 滝) and R[13][7] (々 or a quote mark), neither of which any decoded string
# reaches. Three cells the bitmap and a first transcription disagreed on were
# settled by the decoded text: R[2][0] is 決 (not 洗), R[10][5] is 白 (白紙 -
# "all pages are blank"), R[10][12] is 必 (必要ない), R[14][16] is 厳
# (厳重にロックされている - "the door is tightly locked").

# Multi-codepoint cells are spelled out; they are single 14x14 glyphs.
LEFT = [
    # row 0 (indices 0-17)
    [' ', None, None, None, None, None, None, None, None, None, None, None,
     '0', '1', '2', '3', '4', '5'],
    # row 1 (18-35)
    ['6', '7', '8', '9', ':', '、', '。', '”', '!', '?', '‽',
     'A', 'B', 'C', 'D', 'E', 'F', 'G'],
    # row 2 (36-53)
    ['H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U',
     'V', 'W', 'X', 'Y'],
    # row 3 (54-71)
    # Cols 1 and 3 are '[' and ']' here, where fontus.tim has '(' and ')' at
    # the same indices; the round parentheses are L[14][4]/L[14][5], which is
    # also where PrintText8x14 remaps ASCII 40/41 to (texU 56/70, texV 224).
    ['Z', '[', '/', ']', "'", 'ー', '・', 'a', 'b', 'c', 'd', 'e',
     'f', 'g', 'h', 'i', 'j', 'k'],
    # row 4 (72-89)
    ['l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
     'z', 'あ', 'い', 'う'],
    # row 5 (90-107)
    list('えおかきくけこさしす'
         'せそたちつてとな'),
    # row 6 (108-125)
    list('にぬねのはひふへほま'
         'みむめもやゆよら'),
    # row 7 (126-143)
    list('りるれろわをんがぎぐ'
         'げござじずぜぞだ'),
    # row 8 (144-161)
    list('ぢづでどばびぶべぼぱ'
         'ぴぷぺぽぁぃぅぇ'),
    # row 9 (162-179)
    list('ぉゃゅょっアイウエオ'
         'カキクケコサシス'),
    # row 10 (180-197)
    list('セソタチツテトナニヌ'
         'ネノハヒフヘホマ'),
    # row 11 (198-215)
    list('ミムメモヤユヨラリル'
         'レロワヲンガギグ'),
    # row 12 (216-233)
    list('ゲゴザジズゼゾダヂヅ'
         'デドバビブベボパ'),
    # row 13 (234-251)
    ['ピ', 'プ', 'ペ', 'ポ', 'ァ', 'ィ', 'ゥ',
     'ェ', 'ォ', 'ャ', 'ュ', 'ョ', 'ッ', 'ヴ',
     None, None, '「', '」'],
    # row 14 (252-269)  reached as 0xF8 nn with nn = 18..35
    ['S.', 'T.', 'A.', 'R.', '(', ')', '『', '』', '“', '”',
     '.', '×', '上', '右', '下', '左', '悪', '安'],
    # row 15 (270-287)  reached as 0xF8 nn with nn = 36..53
    list('暗穴員遺育意違一炎役'
         '押俺奥応回拡兜火'),
]

RIGHT = [
    # row 0  - 0xF9 00..11
    list('壊楽何外確感関開間館'
         '方会怪家完救急機'),
    # row 1
    list('御寄究記器起況机気危'
         '許凶強供給空君具'),
    # row 2
    list('決血剣研係見形険撃元'
         '経験構後号古行合'),
    # row 3
    list('攻刻光今口効言向降酸'
         '剤査残最殺作縮小'),
    # row 4
    list('進信室宿舎盾書飼写真'
         '取指子射除紙状純'),
    # row 5
    list('仕手私食時出事銃術丈'
         '少死使実失持助身'),
    # row 6
    list('心自重消蛇緒守思者図'
         '水前清青赤制生成'),
    # row 7
    list('石先整性洗全声跡接切'
         '染草捜造装像槽存'),
    # row 8
    list('続族騒大退弾単誰棚台'
         '男他待体脱地調置'),
    # row 9
    list('中知通転定電庭動当踏'
         '逃毒倒得特日人入'),
    # row 10
    list('認任燃年配白敗反破発'
         '美備必物譜不風夫'),
    # row 11
    list('分部普聞並別変放宝本'
         '保報味無面迷滅目'),
    # row 12
    list('戻薬屋奴用鎧要熔様来'
         '頼落力硫料理流立'),
    # row 13
    ['裏', '令', '連', '練', '路', '話', '…',
     '々', '夕', '溶', '液', '絡', '四', '六',
     '角', '抜', '所', '同'],
    # row 14 - 0xFA nn with nn = 0..17
    list('久階計月告索女習準充'
         '社喋傷受冗製厳巣'),
    # row 15 - 0xFA nn with nn = 18..35
    list('多短対断談治仲丸点堂'
         '度内念判品怖基油'),
    # row 16 - 0xFA nn with nn = 36..53
    list('療送枯願玉遇固根好天'
         '紫星神情遣潟答緑'),
    # row 17 - 0xFA nn with nn = 54..71
    ['息', '二', '十', '西', '東', None, '未',
     '誌', '証', '番', '有', '化', '学', '長',
     '警', '資', '植', '絵'],
]


def left_index(row, col):
    return row * 18 + col


def encode_cell(page, row, col):
    """Return the byte sequence that selects LEFT/RIGHT[row][col]."""
    if page == 'L':
        if row < 14:
            return bytes([row * 18 + col])
        return bytes([0xF8, (row - 13) * 18 + col])
    if row < 14:
        return bytes([0xF9, row * 18 + col])
    return bytes([0xFA, (row - 14) * 18 + col])


# Characters the font draws in more than one cell. The game consistently uses
# one of them, so the encoder has to pick that one or a re-encoded message will
# not match the original bytes. Verified with `jpn_msg_decode.py verify`.
PREFER = {
    '”': ('L', 14, 9),    # closing quote: the pair is L14[8]/L14[9]
    '.': ('L', 14, 10),        # latin full stop (row 1 col 6 is the ideographic .)
}


def build_map():
    """char -> byte sequence.  First definition wins (LEFT before RIGHT)."""
    m = {}
    for r, rowv in enumerate(LEFT):
        for c, ch in enumerate(rowv):
            if ch and ch not in m:
                m[ch] = encode_cell('L', r, c)
    for r, rowv in enumerate(RIGHT):
        for c, ch in enumerate(rowv):
            if ch and ch not in m:
                m[ch] = encode_cell('R', r, c)
    for ch, (page, r, c) in PREFER.items():
        m[ch] = encode_cell(page, r, c)
    # ';' has no glyph of its own; the original spells a pause with the
    # ideographic comma, and the USA table's ',' index is the ideographic
    # full stop here, so neither latin mark can keep its fontus.tim index.
    m[','] = m['、']
    m[';'] = m['、']
    m['\\'] = m['/']
    return m
