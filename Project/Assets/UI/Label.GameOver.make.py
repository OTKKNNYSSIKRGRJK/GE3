# ゲームオーバーのラベル画像を作る（PIL）。白文字・透明背景、文字の周りに余白 PAD px。
#   Label.GameOver.png   : 「Game Over」
#   Label.PressSpace.png : 「Press Space to Continue」
# 使い方: python Label.GameOver.make.py <KaiseiDecol-Bold.ttf のパス>
# フォント: Kaisei Decol Bold（SIL Open Font License 1.1。Google Fonts / github.com/google/fonts の ofl/kaiseidecol）
# 画面に出す大きさは、画像の px × 倍率（Director の GameOver scale / Prompt scale。既定 0.5）なので、画像は 2 倍の解像度で作ってある
import sys
from PIL import Image, ImageDraw, ImageFont

FONT = sys.argv[1] if len(sys.argv) > 1 else "KaiseiDecol-Bold.ttf"
PAD = 24          # 文字の周りの透明な余白

LABELS = (
    ("Label.GameOver.png", "Game Over", 240),
    ("Label.PressSpace.png", "Press Space to Continue", 88),
)

for path, text, size in LABELS:
    font = ImageFont.truetype(FONT, size)
    canvas = Image.new("L", (size * len(text) * 2, size * 3), 0)
    ImageDraw.Draw(canvas).text((size, size), text, font=font, fill=255)
    glyphs = canvas.crop(canvas.getbbox())
    alpha = Image.new("L", (glyphs.width + PAD * 2, glyphs.height + PAD * 2), 0)
    alpha.paste(glyphs, (PAD, PAD))
    rgba = Image.new("RGBA", alpha.size, (255, 255, 255, 0))
    rgba.putalpha(alpha)
    rgba.save(path)
    print(path, rgba.size)
