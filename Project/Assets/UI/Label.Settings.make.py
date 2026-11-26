# 「設定」のラベル画像を作る（PIL）。白文字・透明背景。
#   Label.Settings.png       … タイトル用。Label.Start.png / Label.Exit.png と同じ 720x180、右寄せ（文字の右端 692、縦の中心 99）、108px
#   Label.Settings.Pause.png … ポーズメニュー用。Label.Restart.png などと同じく文字に合わせて切り抜き、周りに PAD px、100px
#   Label.HP.Player.png / Label.HP.Boss.png … HP バーの「Player」「Boss」。切り抜き、96px
# 使い方: python Label.Settings.make.py <KaiseiDecol-Bold.ttf のパス>
# フォント: Kaisei Decol Bold（SIL Open Font License 1.1。github.com/google/fonts の ofl/kaiseidecol）
import sys
from PIL import Image, ImageDraw, ImageFont

FONT = sys.argv[1] if len(sys.argv) > 1 else "KaiseiDecol-Bold.ttf"


def glyphs(text, size):
    font = ImageFont.truetype(FONT, size)
    canvas = Image.new("L", (size * len(text) * 2, size * 3), 0)
    ImageDraw.Draw(canvas).text((size, size), text, font=font, fill=255)
    return canvas.crop(canvas.getbbox())


def save(alpha, path):
    rgba = Image.new("RGBA", alpha.size, (255, 255, 255, 0))
    rgba.putalpha(alpha)
    rgba.save(path)
    print(path, rgba.size)


def cropped(text, size, pad, path):
    g = glyphs(text, size)
    alpha = Image.new("L", (g.width + pad * 2, g.height + pad * 2), 0)
    alpha.paste(g, (pad, pad))
    save(alpha, path)


def title_style(text, path):
    g = glyphs(text, 108)
    alpha = Image.new("L", (720, 180), 0)
    alpha.paste(g, (692 - g.width, 99 - g.height // 2))
    save(alpha, path)


title_style("設定", "Label.Settings.png")
cropped("設定", 100, 12, "Label.Settings.Pause.png")
cropped("Player", 96, 12, "Label.HP.Player.png")
cropped("Boss", 96, 12, "Label.HP.Boss.png")
