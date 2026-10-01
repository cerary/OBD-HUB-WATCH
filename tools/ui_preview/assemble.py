"""Convert native LVGL PPM frames and assemble README galleries; requires Pillow."""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageFont
parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,default=Path('docs/images'));args=parser.parse_args();out=args.output
for p in out.glob('*.ppm'):Image.open(p).save(p.with_suffix('.png'))
font=ImageFont.load_default(size=18)
def sheet(filename,entries,columns=3):
    side=330;gap=14;caption=36;rows=(len(entries)+columns-1)//columns
    im=Image.new('RGB',(columns*(side+gap)+gap,rows*(side+caption+gap)+gap),(19,20,22));d=ImageDraw.Draw(im)
    for i,(name,label) in enumerate(entries):
        x=gap+(i%columns)*(side+gap);y=gap+(i//columns)*(side+caption+gap)
        im.paste(Image.open(out/(name+'.png')).resize((side,side),Image.Resampling.LANCZOS),(x,y))
        d.text((x+side/2,y+side+10),label,fill=(235,235,239),font=font,anchor='mt')
    im.save(out/filename)
sheet('all-dials.png',[('status','01 STATUS'),('gear','02 GEAR'),('rpm','03 RPM'),('speed','04 SPEED'),('temperature','05 TEMPERATURE'),('info','06 FIVE METRICS'),('g-force','07 G FORCE'),('needle','08 NEEDLE'),('chart','09 TREND'),('expression','10 EXPRESSION'),('settings','11 SETTINGS'),('cx-standby','12 CX STANDBY')])
sheet('ring-states.png',[('rpm-connecting','CONNECTING'),('rpm-idle','IDLE 800 RPM'),('rpm-green','3000 RPM'),('rpm-red','5500 RPM'),('rpm-stale','LINK / DATA LOST'),('rpm-cx-sleep','CX SLEEP')])
print('Saved native-resolution PNGs and two README galleries')
