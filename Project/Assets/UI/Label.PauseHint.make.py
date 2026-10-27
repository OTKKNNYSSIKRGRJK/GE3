# 画面右下の操作ヒント「ESC : ポーズ」のラベル画像を作る（PIL）。白文字・透明背景、文字の周りに余白 PAD px。
# 使い方: python Label.PauseHint.make.py <KaiseiDecol-Bold.ttf のパス>
# フォント: Kaisei Decol Bold（SIL Open Font License 1.1。Google Fonts / github.com/google/fonts の ofl/kaiseidecol）
import sys
from PIL import Image, ImageDraw, ImageFont

FONT = sys.argv[1] if len(sys.argv) > 1 else "KaiseiDecol-Bold.ttf"
TEXT = "ESC : ポーズ"
SIZE = 96         # フォントの大きさ（px）。Label.Resume.png などの文字の高さに近い値
PAD = 20          # 文字の周りの透明な余白

font = ImageFont.truetype(FONT, SIZE)
canvas = Image.new("L", (SIZE * len(TEXT) * 2, SIZE * 3), 0)
ImageDraw.Draw(canvas).text((SIZE, SIZE), TEXT, font=font, fill=255)
glyphs = canvas.crop(canvas.getbbox())
alpha = Image.new("L", (glyphs.width + PAD * 2, glyphs.height + PAD * 2), 0)
alpha.paste(glyphs, (PAD, PAD))
rgba = Image.new("RGBA", alpha.size, (255, 255, 255, 0))
rgba.putalpha(alpha)
rgba.save("Label.PauseHint.png")
print("Label.PauseHint.png", rgba.size)
