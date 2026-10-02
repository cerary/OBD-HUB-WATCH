"""Run production policy, OBD cache and NVS regressions with host stubs (GCC)."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1];MAIN=ROOT/'main';HOST=ROOT/'tools/host_test';BUILD=ROOT/'.cache/host-tests';BUILD.mkdir(parents=True,exist_ok=True)
def run(args):subprocess.run(list(map(str,args)),check=True)
exe=BUILD/'test_charge_led'
run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(MAIN),ROOT/'tools/test_charge_led.c','-o',exe]);run([exe])
for name,source,include in (('cx_power','bsp_obd_dsp/cx_power_policy.c',MAIN/'bsp_obd_dsp'),('status_ring','app_obd_dsp/status_ring_policy.c',MAIN),('peak_marker','app_obd_dsp/peak_marker_policy.c',MAIN),('temperature_alert','app_obd_dsp/temperature_alert_policy.c',MAIN)):
 exe=BUILD/('test_'+name)
 run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(include),ROOT/('tools/test_'+name+'.c'),MAIN/source,'-lm','-o',exe]);run([exe])
exe=BUILD/'regression'
run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-DCONFIG_OBD_HW_VERSION_M5STOPWATCH=1','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(HOST),'-I'+str(MAIN),HOST/'regression.c',MAIN/'bsp_obd_dsp/nvs_storage.c',MAIN/'app_obd_dsp/obd_data_cache.c',MAIN/'app_obd_dsp/status_ring_policy.c',MAIN/'bsp_obd_dsp/cx_power_policy.c','-lm','-o',exe])
for mode in ('cache','afr','legacy','zero','custom','temperature_migrate','temperature_off'):run([exe,mode])
