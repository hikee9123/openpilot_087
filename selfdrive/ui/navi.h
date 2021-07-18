#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
this is navigation code by OPKR, and thank you to the OPKR developer.
I love OPKR code.
*/

static void ui_text(const UIState *s, float x, float y, const char *string, float size, NVGcolor color, const char *font_name) {
  nvgFontFace(s->vg, font_name);
  nvgFontSize(s->vg, size);
  nvgFillColor(s->vg, color);
  nvgText(s->vg, x, y, string, NULL);
}

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
    const char *name_sped[] = {"speed_var","traf_turn", "img_space","speed_30","speed_40","speed_50","speed_60","speed_70","speed_80","speed_90","speed_100","speed_110"};


    const char  *szSign = NULL;

    int  nTrafficSign = int( map_sign );

    if( nTrafficSign == 113 ) traffic_sign = name_sped[1];  // 굽은도로
    else if( nTrafficSign == 195 ) traffic_sign = name_sped[0];  // 가변 단속. ( by opkr)
    else if( speedLimit <= 10 )  traffic_sign = NULL;
    else if( speedLimit <= 30 )  traffic_sign = name_sped[3];
    else if( speedLimit <= 40 )  traffic_sign = name_sped[4];
    else if( speedLimit <= 50 )  traffic_sign = name_sped[5];
    else if( speedLimit <= 60 )  traffic_sign = name_sped[6];
    else if( speedLimit <= 70 )  traffic_sign = name_sped[7];
    else if( speedLimit <= 80 )  traffic_sign = name_sped[8];
    else if( speedLimit <= 90 )  traffic_sign = name_sped[9];
    else if( speedLimit <= 100 )  traffic_sign = name_sped[10];
    else if( speedLimit <= 110 )  traffic_sign = name_sped[11];

  
    if( nTrafficSign && traffic_sign == NULL )
    {
      traffic_sign = name_sped[2];
    }


    int img_size = 200;   // 472
    int img_xpos = s->viz_rect.x + bdr_s + 184 + 20;
    int img_ypos = s->viz_rect.y + bdr_s - 20;

    nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
      // 1. text  Distance
    if( speedLimitAheadDistance >= 5 )
    {
 
      char  szSLD[50];
      if( speedLimitAheadDistance >= 1000 )
        sprintf(szSLD,"%.1fkm", speedLimitAheadDistance * 0.001 );
      else
        sprintf(szSLD,"%.0fm", speedLimitAheadDistance );

      int txt_size = int(img_size*0.8);
      int txt_xpos = img_xpos + 20;  
      int txt_ypos = img_ypos + img_size - 20;
      const Rect rect = { txt_xpos, txt_ypos, txt_size, 60};
      ui_fill_rect(s->vg, rect, COLOR_BLACK_ALPHA(100), 30.);
      ui_draw_rect(s->vg, rect, COLOR_WHITE_ALPHA(100), 5, 20.);        

      nvgFillColor(s->vg, nvgRGBA(255, 255, 255, 255));
      ui_text(s, rect.centerX(), rect.centerY()+15, szSLD, 40, COLOR_WHITE, "sans-bold");
    }

    // 2. image
    if( traffic_sign  )  
    {
      float img_alpha = 0.3f;       
      ui_draw_image(s, {img_xpos, img_ypos, img_size, img_size}, traffic_sign, img_alpha);
    }



  /*
   111 : 우측 커브 
   112 : 윈쪽 커브
   113 : 굽은도로
   118, 127 : 어린이보호구역
   122 : 좁아지는 도로
   124 : 과속방지턱
   129 : 주정차
   195 : 가변단속구간.

   131 : 단속카메라(신호위반카메라)
   165 : 구간단속
   200 : 단속구간(고정형 이동식)
   231 : 단속(카메라, 신호위반)
   248 : 교통정보수집
*/



    if( nTrafficSign == 195 ) szSign = "가변구간";
    else if( nTrafficSign == 165 ) szSign = "구간단속";
    else if( nTrafficSign == 131 ) szSign = "신호위반";
    else if( nTrafficSign == 248 ) szSign = "교통정보";
    else if( nTrafficSign == 200 ) szSign = "이동식";


      if( szSign )
      {
        ui_text(s, img_xpos + int(img_size*0.5), img_ypos+25, szSign, 26, COLOR_WHITE, "sans-bold"); 
      }
      else
      {
        char  szSignal[50];
        sprintf(szSignal,"%d", nTrafficSign );
        ui_text(s, img_xpos + int(img_size*0.5), img_ypos + int(img_size*0.7), szSignal, 80, COLOR_WHITE, "sans-bold"); 
      }

}

static void ui_draw_navi(UIState *s) 
{
  UIScene &scene = s->scene;


 // float  roadCurvature = scene.liveNaviData.getRoadCurvature();
 // int   opkrturninfo = scene.liveNaviData.getTurnInfo();
 //int   opkrdisttoturn = scene.liveNaviData.getDistanceToTurn();

 
  float speedLimit =  scene.liveNaviData.getSpeedLimit();  
  float speedLimitAheadDistance =  scene.liveNaviData.getSpeedLimitDistance();  
  float map_sign = 300; // scene.liveNaviData.getSafetySign();
  int  mapValid = 1;// scene.liveNaviData.getMapValid();


  //  printf("ui_draw_navi %d  %.1f  %d \n", mapValid, speedLimit, opkrturninfo);
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
