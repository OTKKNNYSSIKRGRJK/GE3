# VS バナーの文字（V / S）の仮画像を作る（PIL）。白文字・透明背景、1 文字 1 枚。
# 使い方: python VsBanner.Letters.make.py [フォントのパス]
# 既定のフォントは Inter Display Black Italic（SIL Open Font License）。手元に無ければ太いフォントを指定する。
# 本番の文字は手描きの画像に差し替える前提（白で塗った透明 PNG。α を形に使う）。
import sys
from PIL import Image, ImageDraw, ImageFont

FONT = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/opentype/inter/InterDisplay-BlackItalic.otf"
HEIGHT = 512      # 出力の高さ（幅は文字の形から）
PAD = 56          # 文字の周りの透明な余白（グローと SDF の距離の範囲のぶん）

def make(letter, path):
    font = ImageFont.truetype(FONT, 1000)
    canvas = Image.new("L", (1600, 1600), 0)
    ImageDraw.Draw(canvas).text((300, 100), letter, font=font, fill=255)
    box = canvas.getbbox()
    glyph = canvas.crop(box)
    inner = HEIGHT - PAD * 2
    w = max(1, round(glyph.width * inner / glyph.height))
    glyph = glyph.resize((w, inner), Image.LANCZOS)
    alpha = Image.new("L", (w + PAD * 2, HEIGHT), 0)
    alpha.paste(glyph, (PAD, PAD))
    rgba = Image.new("RGBA", alpha.size, (255, 255, 255, 0))
    rgba.putalpha(alpha)
    rgba.save(path)
    print(path, rgba.size)

make("V", "VsBanner.V.png")
make("S", "VsBanner.S.png")
