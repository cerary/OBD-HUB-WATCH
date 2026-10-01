
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
esp_err_t nvs_set_u8(nvs_handle_t h,const char *key,uint8_t v){return nvs_set_blob(h,key,&v,1);}
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
 const char *mode=argc>1?argv[1]:"fresh";
 if(!strcmp(mode,"saved")) {
   nvs_user_cfg_t cfg=*nvs_cfg_get();cfg.needle_source_idx=11;cfg.chart_source_idx=11;
   nvs_set_blob(1,"settings",&cfg,sizeof(cfg));
 }
 assert(nvs_storage_init()==ESP_OK);
 printf("%s needle=%u chart=%u AFR_alarm=%d\n",mode,nvs_cfg_get()->needle_source_idx,nvs_cfg_get()->chart_source_idx,nvs_chart_alarm_get(11));
 return 0;
}
