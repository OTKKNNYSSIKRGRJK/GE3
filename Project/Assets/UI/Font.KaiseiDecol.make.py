# 設定画面（Game.KeyConfigScreen）などで使う文字アトラスを作る（PIL）。
#   出力: Font.KaiseiDecol.png（白文字・透明背景。α が文字の濃さ）と Font.KaiseiDecol.json（文字ごとの位置と送り幅）
#   使い方: python Font.KaiseiDecol.make.py <KaiseiDecol-Bold.ttf のパス>
#   フォント: Kaisei Decol Bold（SIL Open Font License 1.1。github.com/google/fonts の ofl/kaiseidecol）
#
# 入れる文字 = ASCII ＋ ひらがな・カタカナ・よく使う記号 ＋ EXTRA（画面の文字列に出てくる漢字など）。
# 画面に新しい文字を出すときは EXTRA に足して作り直す（入っていない文字は表示で飛ばされる）。
#
# JSON の Glyphs = [[コードポイント, x, y, w, h, xoff, yoff, advance], ...]
#   x, y, w, h   : アトラスの中の矩形（px、周りに PAD の余白を含む）
#   xoff, yoff   : ペン位置（行の上端 = アセンダの線）から矩形の左上までのずれ（px）
#   advance      : 次の文字までの送り幅（px）
import json
import sys
from PIL import Image, ImageDraw, ImageFont

FONT = sys.argv[1] if len(sys.argv) > 1 else "KaiseiDecol-Bold.ttf"
SIZE = 52           # 焼く大きさ（px）。画面では 18〜40px 程度に縮めて使う（mip が効く）
PAD = 4             # 文字の周りの余白（縮小したときに隣の文字がにじまないように）
WIDTH = 2048        # アトラスの幅

ASCII = "".join(chr(c) for c in range(0x20, 0x7F))
KANA = "".join(chr(c) for c in range(0x3041, 0x3097)) + "".join(chr(c) for c in range(0x30A1, 0x30FB))
SYMBOLS = "ー・、。「」（）：　！？…〜←↑→↓"
EXTRA = "一中二作使保入初前割効動右回存定左度弾当後戻押撃操攻替有期未段決点瞬移空視設選避間"

chars = []
for ch in ASCII + KANA + SYMBOLS + EXTRA:
    if ch not in chars:
        chars.append(ch)

font = ImageFont.truetype(FONT, SIZE)
ascent, descent = font.getmetrics()

# 1 文字ずつ焼いてから、左上から行に詰める
tiles = []
for ch in chars:
    advance = font.getlength(ch)
    l, t, r, b = font.getbbox(ch)       # 描く原点（左・アセンダの線）からの矩形
    w, h = r - l, b - t
    if w <= 0 or h <= 0:
        tiles.append((ch, None, 0, 0, advance))
        continue
    tile = Image.new("L", (w + PAD * 2, h + PAD * 2), 0)
    ImageDraw.Draw(tile).text((PAD - l, PAD - t), ch, font=font, fill=255)
    tiles.append((ch, tile, l - PAD, t - PAD, advance))

x, y, rowH = 0, 0, 0
placed = []
for ch, tile, xoff, yoff, advance in tiles:
    if tile is None:
        placed.append((ch, 0, 0, 0, 0, xoff, yoff, advance))
        continue
    if x + tile.width > WIDTH:
        x, y, rowH = 0, y + rowH, 0
    placed.append((ch, x, y, tile.width, tile.height, xoff, yoff, advance))
    x += tile.width
    rowH = max(rowH, tile.height)
height = y + rowH
height = 1 << (height - 1).bit_length()     # 2 のべき乗に

alpha = Image.new("L", (WIDTH, height), 0)
i = 0
for (ch, tile, _, _, _), (_, px, py, _, _, _, _, _) in zip(tiles, placed):
    if tile is not None:
        alpha.paste(tile, (px, py))
rgba = Image.new("RGBA", alpha.size, (255, 255, 255, 0))
rgba.putalpha(alpha)
rgba.save("Font.KaiseiDecol.png")

meta = {
    "Font": "Kaisei Decol Bold",
    "Size": SIZE,
    "Ascent": ascent,
    "Descent": descent,
    "Width": WIDTH,
    "Height": height,
    "Glyphs": [[ord(ch), px, py, w, h, xoff, yoff, round(adv, 3)] for ch, px, py, w, h, xoff, yoff, adv in placed],
}
with open("Font.KaiseiDecol.json", "w", encoding="utf-8") as f:
    json.dump(meta, f, ensure_ascii=False, separators=(",", ":"))
print("Font.KaiseiDecol.png", rgba.size, len(placed), "glyphs")
