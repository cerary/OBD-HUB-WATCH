"""Compare saved AFR choices and fresh alarm defaults using original/fixed NVS C files.
Pass a checkout of upstream steveEcode/obd_brz_gauge with --upstream.
This compiles source unchanged; only NVS/hardware APIs are host stubs.
"""
from pathlib import Path
import argparse,subprocess
parser=argparse.ArgumentParser();parser.add_argument('--upstream',type=Path,required=True);args=parser.parse_args()
ROOT=Path(__file__).resolve().parents[1];MAIN=ROOT/'main';HOST=ROOT/'tools/host_test';BUILD=ROOT/'.cache/afr-compare';BUILD.mkdir(parents=True,exist_ok=True)
for label,base in (('upstream',args.upstream.resolve()),('fixed',ROOT)):
 exe=BUILD/label
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(HOST),'-I'+str(base/'main'),'-I'+str(MAIN),HOST/'afr_probe.c',base/'main/bsp_obd_dsp/nvs_storage.c',MAIN/'app_obd_dsp/status_ring_policy.c','-lm','-o',exe],check=True)
 for mode in ('fresh','saved'):
  text=subprocess.check_output([exe,mode],text=True).strip();print(label+': '+text,flush=True)
  if label=='upstream':assert ('AFR_alarm=0' in text and (mode!='saved' or 'needle=0 chart=8' in text)),text
  else:assert ('AFR_alarm=32767' in text and (mode!='saved' or 'needle=11 chart=11' in text)),text
