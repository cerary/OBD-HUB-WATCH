
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/cx_power_policy.h"
#include "app_obd_dsp/obd_data_cache.h"
#include "app_obd_dsp/vehicle_profiles.h"

static struct {char key[40]; unsigned char data[512]; size_t size;} store[20];
static unsigned count;
static bool fail_volume_write;
static unsigned volume_writes;
static int64_t now_us=1000000;
static void (*mile_cb)(void*);
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h){(void)ns;(void)mode;*h=1;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){
    (void)h;
    for(unsigned i=0;i<count;i++)if(strcmp(store[i].key,key)==0){
        if(!out){*size=store[i].size;return ESP_OK;}
        if(*size<store[i].size){*size=store[i].size;return ESP_ERR_NVS_INVALID_LENGTH;}
        memcpy(out,store[i].data,store[i].size);*size=store[i].size;return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size){
    (void)h; assert(size<=512);
    unsigned i;for(i=0;i<count;i++)if(strcmp(store[i].key,key)==0)break;
    if(i==count){assert(count<20);count++;strcpy(store[i].key,key);}
    memcpy(store[i].data,data,size);store[i].size=size;return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *v){size_t s=1;return nvs_get_blob(h,key,v,&s);}
esp_err_t nvs_set_u8(nvs_handle_t h,const char *key,uint8_t v){
    if (!strcmp(key,"soundvol")) {
        if (fail_volume_write) return ESP_FAIL;
        ++volume_writes;
    }
    return nvs_set_blob(h,key,&v,1);
}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;return ESP_OK;}
void nvs_close(nvs_handle_t h){(void)h;}
esp_err_t nvs_flash_init(void){return ESP_OK;}
esp_err_t nvs_flash_erase(void){count=0;return ESP_OK;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return (void*)1;}
int xSemaphoreTake(SemaphoreHandle_t h,int t){(void)h;(void)t;return 1;}
int xSemaphoreGive(SemaphoreHandle_t h){(void)h;return 1;}
TickType_t xTaskGetTickCount(void){return (TickType_t)(now_us/1000);}
int64_t esp_timer_get_time(void){return now_us;}
esp_err_t esp_timer_create(const esp_timer_create_args_t *args,esp_timer_handle_t *h){mile_cb=args->callback;*h=(void*)1;return ESP_OK;}
esp_err_t esp_timer_start_periodic(esp_timer_handle_t h,uint64_t period){(void)h;(void)period;return ESP_OK;}
static vehicle_profile_t profile;
const vehicle_profile_t *vehicle_profile_get_all(uint8_t *n){*n=1;return &profile;}
const vehicle_profile_t *vehicle_profile_get_active(void){return &profile;}

int main(int argc,char **argv) {
    const char *mode=argc>1?argv[1]:"cache";
    if (!strcmp(mode,"afr")) {
        nvs_user_cfg_t cfg=*nvs_cfg_get();cfg.needle_source_idx=11;cfg.chart_source_idx=11;
        nvs_set_blob(1,"settings",&cfg,sizeof(cfg));
    }
    if (!strcmp(mode,"legacy") || !strcmp(mode,"zero") || !strcmp(mode,"custom")) {
        int16_t alarm[12];for(int i=0;i<12;++i)alarm[i]=32767;
        alarm[0]=110;alarm[8]=80;alarm[9]=6000;
        alarm[11]=!strcmp(mode,"custom")?1600:0;
        nvs_set_blob(1,"chartalarm",alarm,!strcmp(mode,"legacy")?22:24);
    }
    if (!strcmp(mode,"temperature_migrate") || !strcmp(mode,"temperature_off")) {
        int16_t alarm[12];for(int i=0;i<12;++i)alarm[i]=32767;
        alarm[0]=118;alarm[8]=95;alarm[11]=1600;
        nvs_set_blob(1,"chartalarm",alarm,sizeof(alarm));
        if (!strcmp(mode,"temperature_off")) nvs_set_u8(1,"tempalert_v1",1);
    }
    assert(nvs_storage_init()==ESP_OK);
    if (!strcmp(mode,"sound_volume")) {
        nvs_user_cfg_t original=*nvs_cfg_get();
        status_ring_config_t ring=*nvs_status_ring_get();
        unsigned char old_blob[512];size_t old_size=sizeof(old_blob);
        assert(nvs_get_blob(1,"settings",old_blob,&old_size)==ESP_OK);
        assert(nvs_sound_volume_get()==60);
        assert(nvs_sound_volume_set(35)==ESP_OK);
        uint8_t saved=255;
        assert(nvs_get_u8(1,"soundvol",&saved)==ESP_OK&&saved==35);
        unsigned writes=volume_writes;
        assert(nvs_sound_volume_set(35)==ESP_OK&&volume_writes==writes);
        assert(nvs_storage_init()==ESP_OK&&nvs_sound_volume_get()==35);
        assert(nvs_sound_volume_set(0)==ESP_OK);
        assert(nvs_storage_init()==ESP_OK&&nvs_sound_volume_get()==0);
        assert(nvs_sound_volume_set(100)==ESP_OK);
        assert(nvs_storage_init()==ESP_OK&&nvs_sound_volume_get()==100);
        assert(nvs_sound_volume_set(101)==ESP_ERR_INVALID_ARG&&nvs_sound_volume_get()==100);
        fail_volume_write=true;
        assert(nvs_sound_volume_set(20)==ESP_FAIL&&nvs_sound_volume_get()==100);
        fail_volume_write=false;
        assert(nvs_get_u8(1,"soundvol",&saved)==ESP_OK&&saved==100);
        assert(nvs_set_u8(1,"soundvol",255)==ESP_OK);
        assert(nvs_storage_init()==ESP_OK&&nvs_sound_volume_get()==60);
        assert(!memcmp(&original,nvs_cfg_get(),sizeof(original)));
        assert(!memcmp(&ring,nvs_status_ring_get(),sizeof(ring)));
        unsigned char after_blob[512];size_t after_size=sizeof(after_blob);
        assert(nvs_get_blob(1,"settings",after_blob,&after_size)==ESP_OK);
        assert(after_size==old_size&&!memcmp(old_blob,after_blob,old_size));
        puts("PASS: cue volume defaults/0/100/reload/invalid/save failure; unchanged settings blob and ring; no duplicate writes");return 0;
    }
    if (!strcmp(mode,"temperature_migrate") || !strcmp(mode,"temperature_off")) {
        bool adopted=!strcmp(mode,"temperature_migrate");
        assert(nvs_chart_alarm_get(0)==118&&nvs_chart_alarm_get(8)==95&&nvs_chart_alarm_get(11)==1600);
        assert(nvs_chart_alarm_get(1)==(adopted?80:32767));
        assert(nvs_chart_alarm_get(2)==(adopted?135:32767));
        uint8_t marker=0;assert(nvs_get_u8(1,"tempalert_v1",&marker)==ESP_OK&&marker==1);
        puts("PASS: temperature adoption preserves custom/unrelated limits; explicit OFF remains OFF after adoption");return 0;
    }
    if (!strcmp(mode,"afr")) {
        assert(nvs_cfg_get()->needle_source_idx==11&&nvs_cfg_get()->chart_source_idx==11);
        puts("PASS: persisted AFR source survives init in both needle and chart");return 0;
    }
    if (!strcmp(mode,"legacy") || !strcmp(mode,"zero") || !strcmp(mode,"custom")) {
        assert(nvs_chart_alarm_get(0)==110&&nvs_chart_alarm_get(8)==80&&nvs_chart_alarm_get(9)==6000);
        assert(nvs_chart_alarm_get(11)==(!strcmp(mode,"custom")?1600:32767));
        if(!strcmp(mode,"zero")){int16_t saved[12];size_t size=sizeof(saved);assert(nvs_get_blob(1,"chartalarm",saved,&size)==ESP_OK&&saved[11]==32767);}
        printf("PASS: AFR alarm migration %s preserves other thresholds\n",mode);return 0;
    }
    assert(nvs_chart_alarm_get(11)==32767);
    assert(nvs_chart_alarm_get(0)==120&&nvs_chart_alarm_get(1)==80&&nvs_chart_alarm_get(2)==135);
    assert(nvs_cfg_get()->device_role==0&&nvs_cfg_get()->touch_haptic_enabled==0);
    puts("PASS: fresh AFR alarm OFF; deferred empty-NVS defaults unchanged");
    cx_power_policy_t p;cx_policy_reset(&p,0);
    cx_policy_rpm(&p,800,100);cx_policy_speed(&p,0,200);
    obd_data_set_speed(60);obd_data_set_rpm(6500);obd_data_set_coolant_temp(115);
    obd_data_set_intake_temp(60);obd_data_set_oil_temp(130);obd_data_set_bat_mv(14000);
    obd_data_set_load_pct(50);obd_data_set_tps(40);obd_data_set_oil_pressure_x10(20);
    obd_data_set_brake_temp_x10(800);obd_data_set_boost_x10(12);obd_data_set_afr_x100(1500);
    obd_data_set_gear(4);obd_data_rpm_override_set(true,7000);
    obd_data_invalidate_freshness();
    obd_data_snapshot_t snap;obd_data_freshness_t age;
    obd_data_get_fresh_snapshot(&snap,&age);
    assert(snap.rpm==0&&snap.speed==0&&snap.gear==127);
    assert(snap.coolant_temp==-40&&snap.intake_temp==-40&&snap.oil_temp==-100);
    assert(snap.load_pct==-1&&snap.tps==-1&&snap.bat_mv==-1&&snap.oil_pressure_x10==-1);
    assert(snap.boost_x10==-32768&&snap.brake_temp_x10==-1000&&snap.afr_x100==-1);
    for(int i=0;i<OBD_SAMPLE_COUNT;++i)assert(age.age_ms[i]==UINT32_MAX);
    vMileageDataStatisticTask();assert(mile_cb);
    for(int i=0;i<60;++i){now_us+=1000000;mile_cb(NULL);}
    assert(nvs_stat_get()->trip_m==0);
    assert(!cx_policy_tick(&p,61000)&&p.state==CX_RUNNING);
    for(unsigned t=62000;t<=1800000;t+=1000)assert(!cx_policy_tick(&p,t)&&p.state==CX_RUNNING);
    puts("PASS: disconnect clears all readings/override; 60 stale seconds add zero mileage; does not replace actual idle history with synthetic 0/0");
    // First sample of a new session is not blended with a previous car/session.
    obd_data_set_speed(30);obd_data_set_rpm(800);obd_data_set_coolant_temp(90);obd_data_set_gear(0);
    obd_data_get_fresh_snapshot(&snap,&age);assert(snap.speed==30&&snap.rpm==800);
    now_us+=5000000;obd_data_get_fresh_snapshot(&snap,&age);
    assert(obd_data_sample_is_fresh(&age,OBD_SAMPLE_RPM));
    now_us+=1000;obd_data_get_fresh_snapshot(&snap,&age);
    assert(!obd_data_sample_is_fresh(&age,OBD_SAMPLE_RPM)&&obd_data_sample_is_fresh(&age,OBD_SAMPLE_CLT));
    obd_data_apply_freshness(&snap,&age);
    assert(snap.rpm==0&&snap.speed==0&&snap.gear==127&&snap.coolant_temp==90);
    obd_data_get_snapshot(&snap);assert(snap.rpm==800&&snap.speed==30&&snap.gear==0);
    now_us+=10000000;obd_data_get_fresh_snapshot(&snap,&age);obd_data_apply_freshness(&snap,&age);
    assert(snap.coolant_temp==-40);
    puts("PASS: exact freshness boundaries, per-channel expiration, immutable raw cache, reconnect filter reset");
    obd_data_set_rpm(0);obd_data_set_speed(0);obd_data_get_fresh_snapshot(&snap,&age);
    assert(snap.rpm==0&&obd_data_sample_is_fresh(&age,OBD_SAMPLE_RPM)&&obd_data_sample_is_fresh(&age,OBD_SAMPLE_SPEED));
    cx_policy_reset(&p,0);cx_policy_rpm(&p,0,100);cx_policy_speed(&p,0,200);
    assert(!cx_policy_tick(&p,60199));assert(cx_policy_tick(&p,60200)&&p.state==CX_QUIET);
    puts("PASS: real zero remains valid; only actual 0/0 with missing replies enters existing parking quiet state");
    return 0;
}
