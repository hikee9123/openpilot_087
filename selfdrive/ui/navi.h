#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
this is navigation code by OPKR, and thank you to the OPKR developer.
I love OPKR code.
*/

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
      int img_size = 200;   // 472
      int img_xpos = s->viz_rect.x + bdr_s*2 + 184 + 20;
      int img_ypos = s->viz_rect.y + bdr_s;
      float img_alpha = 0.3f;

      // 1. text

      int txt_xpos = img_xpos + img_size;
      int txt_ypos = img_ypos + img_size;

      nvgFontFace(s->vg, "sans-regular");
      nvgFontSize(s->vg, 25);
      nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
      nvgFillColor(s->vg, nvgRGBA(255, 255, 255, 255));
      if( speedLimitAheadDistance >= 1000 )
        ui_print( s, txt_xpos, txt_ypos,  "%.1fkm", speedLimitAheadDistance * 0.001 );
      else
        ui_print( s, txt_xpos, txt_ypos,  "%.0fm", speedLimitAheadDistance );

      // 2. image
      ui_draw_image(s, {img_xpos, img_ypos, img_size, img_size}, traffic_sign, img_alpha);



    }
}

static void ui_draw_navi(UIState *s) 
{
  UIScene &scene = s->scene;


 // float  roadCurvature = scene.liveNaviData.getRoadCurvature();
  int   opkrturninfo = scene.liveNaviData.getTurnInfo();
 //int   opkrdisttoturn = scene.liveNaviData.getDistanceToTurn();

 
  float speedLimit = 90; // scene.liveNaviData.getSpeedLimit();  
  float speedLimitAheadDistance = 100;// scene.liveNaviData.getSpeedLimitDistance();  
  float map_sign = 200; // scene.liveNaviData.getSafetySign();
  int  mapValid = 1;// scene.liveNaviData.getMapValid();


    printf("ui_draw_navi %d  %.1f  %d \n", mapValid, speedLimit, opkrturninfo);


  if( mapValid )
    ui_draw_traffic_sign( s, map_sign, speedLimit, speedLimitAheadDistance );
}

static void ui_draw_debug1(UIState *s) 
{
  UIScene &scene = s->scene;
 
  nvgFontSize(s->vg, 36);
  nvgTextAlign(s->vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);

  ui_print(s, 0, 30, scene.alert.alertTextMsg1.c_str()  );
  ui_print(s, 0, 1040, scene.alert.alertTextMsg2.c_str() );
  ui_print(s, 0, 1078, scene.alert.alertTextMsg3.c_str() );
}



void ui_main_navi(UIState *s) 
{
  //UIScene &scene = s->scene;

  //if( 0 )
    ui_draw_navi( s );

  ui_draw_debug1( s );
}
