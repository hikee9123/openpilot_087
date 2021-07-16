#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

static void ui_print(UIState *s, int x, int y,  const char* fmt, ... )
{
  char* msg_buf = NULL;
  va_list args;
  va_start(args, fmt);
  vasprintf( &msg_buf, fmt, args);
  va_end(args);

  nvgText(s->vg, x, y, msg_buf, NULL);
}


static void ui_draw_traffic_sign(UIState *s, float map_sign, float speedLimit,  float speedLimitAheadDistance ) 
{
    const char *traffic_sign = NULL;
    const char *name_sped[] = {"speed_var","speed_30","speed_40","speed_50","speed_60","speed_70","speed_80","speed_90","speed_100","speed_110","traf_turn"};

    int  nTrafficSign = int( map_sign );

    if( nTrafficSign == 113 ) traffic_sign = name_sped[10];  // 굽은도로
    else if( nTrafficSign == 195 ) traffic_sign = name_sped[0];  // 가변 단속. ( by opkr)
    else if( speedLimit <= 10 )  traffic_sign = NULL;
    else if( speedLimit <= 30 )  traffic_sign = name_sped[1];
    else if( speedLimit <= 40 )  traffic_sign = name_sped[2];
    else if( speedLimit <= 50 )  traffic_sign = name_sped[3];
    else if( speedLimit <= 60 )  traffic_sign = name_sped[4];
    else if( speedLimit <= 70 )  traffic_sign = name_sped[5];
    else if( speedLimit <= 80 )  traffic_sign = name_sped[6];
    else if( speedLimit <= 90 )  traffic_sign = name_sped[7];
    else if( speedLimit <= 100 )  traffic_sign = name_sped[8];
    else if( speedLimit <= 110 )  traffic_sign = name_sped[9];
  
    if( traffic_sign ) 
    {
      int img_speedlimit_size = 350;   // 472
      int img_speedlimit_x = s->viz_rect.centerX() - img_speedlimit_size/2;
      int img_speedlimit_y = s->viz_rect.centerY() - img_speedlimit_size/2;
      float img_speedlimit_alpha = 0.3f;
      ui_draw_image(s, {img_speedlimit_x, img_speedlimit_y, img_speedlimit_size, img_speedlimit_size}, traffic_sign, img_speedlimit_alpha);


      nvgFontFace(s->vg, "sans-regular");
      nvgFontSize(s->vg, 90);
      nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
      img_speedlimit_y += 470;
      img_speedlimit_x += img_speedlimit_size/2;
      
      nvgFillColor(s->vg, nvgRGBA(255, 255, 255, 255));
      if( speedLimitAheadDistance >= 1000 )
        ui_print( s, img_speedlimit_x, img_speedlimit_y,  "%.1fkm", speedLimitAheadDistance * 0.001 );
      else
        ui_print( s, img_speedlimit_x, img_speedlimit_y,  "%.0fm", speedLimitAheadDistance );
    }
}

static void ui_draw_navi(UIState *s) 
{
  UIScene &scene = s->scene;


  float speedLimit = scene.liveNaviData.getSpeedLimit();  
  float speedLimitAheadDistance = scene.liveNaviData.getSpeedLimitDistance();  
  float map_sign = scene.liveNaviData.getSafetySign();
 // float  roadCurvature = scene.liveNaviData.getRoadCurvature();
 // int   opkrturninfo = scene.liveNaviData.getTurnInfo();
 //int   opkrdisttoturn = scene.liveNaviData.getDistanceToTurn();

  int  mapValid = scene.liveNaviData.getMapValid();
 // int  map_enabled = scene.liveNaviData.getMapEnable();



  if( mapValid )
    ui_draw_traffic_sign( s, map_sign, speedLimit, speedLimitAheadDistance );
}

static void ui_draw_debug1(UIState *s) 
{
  UIScene &scene = s->scene;
 
  nvgFontSize(s->vg, 36*2);
  nvgTextAlign(s->vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);

  ui_print(s, 0, 30, scene.alert.alertTextMsg1.c_str()  );
  ui_print(s, 0, 1040, scene.alert.alertTextMsg2.c_str() );
  ui_print(s, 0, 1078, scene.alert.alertTextMsg3.c_str() );
}



void ui_main_navi(UIState *s) 
{
  //UIScene &scene = s->scene;

  //ui_draw_navi( s );
  ui_draw_debug1( s );
}
