# VS バナーの仮の形（4000x600、透明 PNG、白で形を塗る）。右端 = 進行方向の先頭（矢じり）、左端 = 燕尾の切り込み
import sys
from PIL import Image, ImageDraw
W,H,S=4000,600,2
img=Image.new('RGBA',(W*S,H*S),(255,255,255,0))
d=ImageDraw.Draw(img)
pts=[(80,80),(3500,80),(3920,300),(3500,520),(80,520),(380,300)]
d.polygon([(x*S,y*S) for x,y in pts],fill=(255,255,255,255))
img=img.resize((W,H),Image.LANCZOS)
img.save(sys.argv[1])
