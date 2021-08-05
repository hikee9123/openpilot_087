#include <sys/time.h>
#include <sys/resource.h>

#include <android/log.h>
#include <log/logger.h>
#include <log/logprint.h>

#include "cereal/messaging/messaging.h"
#include "selfdrive/common/util.h"
#include "selfdrive/common/params.h"


typedef struct LiveNaviDataResult {
      float speedLimit;  // Float32;
      float speedLimitDistance;  // Float32;
      float safetySign;    // Float32;
      float roadCurvature;    // Float32;
      int   turnInfo;    // Int32;
      int   distanceToTurn;    // Int32;      
      bool  mapValid;    // bool;
      int   mapEnable;    // bool;
      long  tv_sec;
      long  tv_nsec;
} LiveNaviDataResult;


int traffic_camera( int nsignal_type )
{
    int ret_code = 0;

    switch( nsignal_type )
    {
      case  131:  // 단속(카메라, 신호위반)    
      case  248:  // 교통정보수집
      case  200:  // 단속구간(고정형 이동식)
      case  231:  // 단속(카메라, 신호위반)
            ret_code = 1;
            break;
    } 

    return ret_code;
}

int main() {
  setpriority(PRIO_PROCESS, 0, -15);
   long  nDelta = 0;
   long  nLastTime = 0, nDelta2 = 0;
   long  nDelta_nsec = 0;
   int   traffic_type;
  int     opkr =0;
  long    tv_nsec;
  float   tv_nsec2;

  ExitHandler do_exit;
  PubMaster pm({"liveNaviData"});
  SubMaster sm({"carState"});
  LiveNaviDataResult  res;

  log_time last_log_time = {};
  logger_list *logger_list = android_logger_list_alloc(ANDROID_LOG_RDONLY | ANDROID_LOG_NONBLOCK, 0, 0);

  while (!do_exit) {
    // setup android logging
    if (!logger_list) {
      logger_list = android_logger_list_alloc_time(ANDROID_LOG_RDONLY | ANDROID_LOG_NONBLOCK, last_log_time, 0);
    }
    assert(logger_list);

    struct logger *main_logger = android_logger_open(logger_list, LOG_ID_MAIN);
    assert(main_logger);


    while (!do_exit) {
      
      sm.update(0);
      const float dSpeed = sm["carState"].getCarState().getVEgo() * 3.6;
  
      log_msg log_msg;
      int err = android_logger_list_read(logger_list, &log_msg);
      if (err <= 0) break;

      AndroidLogEntry entry;
      err = android_log_processLogBuffer(&log_msg.entry_v1, &entry);
      if (err < 0) continue;

      

      last_log_time.tv_sec = entry.tv_sec;
      last_log_time.tv_nsec = entry.tv_nsec;


      tv_nsec2 = entry.tv_nsec / 1000000;
      tv_nsec =  entry.tv_sec * 1000ULL + long(tv_nsec2);


      nDelta2 = entry.tv_sec - nLastTime;
      if( nDelta2 >= 1 )
      {
        nLastTime = entry.tv_sec;
        res.mapEnable = Params().getInt("OpkrMapEnable");
      }
      

      
     // code based from atom
     nDelta_nsec = tv_nsec - res.tv_nsec;
     nDelta = entry.tv_sec - res.tv_sec;


      traffic_type = traffic_camera( res.safetySign );

      if( opkr && strcmp( entry.tag, "Connector" ) == 0 )
      {
         if( traffic_type && res.speedLimitDistance < 30 )
           opkr = 3;
         else if( opkr == 1 )
           opkr = 5;
      }     
      else if( strcmp( entry.tag, "opkrspdlimit" ) == 0 )
      {
        res.speedLimit = atoi( entry.message );
        opkr = 1;
      }
      else if( strcmp( entry.tag, "opkrspddist" ) == 0 )
      {
        res.speedLimitDistance = atoi( entry.message );
        opkr = 1;
      }
      else if( strcmp( entry.tag, "opkrsigntype" ) == 0 )
      {
        res.safetySign = atoi( entry.message );
        opkr = 1;
      }
      else if( strcmp( entry.tag, "opkrcurvangle" ) == 0 )  
      {
        res.roadCurvature = atoi( entry.message );
        opkr = 1;
      }
      else if( strcmp( entry.tag, "opkrturninfo" ) == 0 )
      {
        res.turnInfo = atoi( entry.message );
        opkr = 1;
      } 
      else if( strcmp( entry.tag, "opkrdistancetoturn" ) == 0 )
      {
        res.distanceToTurn = atoi( entry.message );
        opkr = 1;
      }      
      else if(  opkr && strcmp( entry.tag, "AudioFlinger" ) == 0 )  //   msm8974_platform
      {
        if( traffic_type && res.speedLimitDistance < 50 )
           opkr = 1000;
      }
      else if( strcmp( entry.tag, "GestureControl" ) == 0 )
      {
         opkr = 3;
      }      
      else if( opkr == 1 )
      {
         opkr = 10;
      }

      if ( opkr == 1000 )
      {
        if( dSpeed < 10 && nDelta_nsec > 500 ) opkr = 0;
        else if( nDelta_nsec > 100 ) opkr = 0;
      }
      else if ( opkr == 1 || dSpeed < 10  )
      {
        res.tv_sec = entry.tv_sec;
        res.tv_nsec = tv_nsec;
      }
      else if ( opkr )
      {
         if( nDelta > opkr ) opkr = 0;
      }

      if ( opkr )
         res.mapValid = 1;
      else
         res.mapValid = 0;

      MessageBuilder msg;
      auto framed = msg.initEvent().initLiveNaviData();
      framed.setId(log_msg.id());
      framed.setTs( res.tv_sec );
      framed.setSpeedLimit( res.speedLimit );  // Float32;
      framed.setSpeedLimitDistance( res.speedLimitDistance );  // raw_target_speed_map_dist Float32;
      framed.setSafetySign( res.safetySign ); // map_sign Float32;
      framed.setRoadCurvature( res.roadCurvature ); // road_curvature Float32;

      // Turn Info
      framed.setTurnInfo( res.turnInfo );
      framed.setDistanceToTurn( res.distanceToTurn );

      framed.setMapEnable( res.mapEnable );
      framed.setMapValid( res.mapValid );

      framed.setTrafficType( traffic_type );

     

      if( opkr )
      {
       printf("[%ld] logcat ID(%d) - PID=%d tag=%d.[%s] \n", tv_nsec, log_msg.id(),  entry.pid,  entry.tid, entry.tag);
       printf("entry.message=[%s]\n", entry.message);
      }

      pm.send("liveNaviData", msg);
    }

    android_logger_list_free(logger_list);
    logger_list = NULL;
    util::sleep_for(500);
  }

  if (logger_list) {
    android_logger_list_free(logger_list);
  }

  return 0;
}


/*
MAPPY
    signtype
    111 오른쪽 급커브
    112 왼쪽 급커브
    113 굽은도로
    118, 127 어린이보호구역
    122 : 좁아지는 도로
    124 : 과속방지턱
    129 : 주정차
    131 : 단속카메라(신호위반카메라)    
    165 : 구간단속    
    198 차선변경금지시작
    199 차선변경금지종료
    129 주정차금지구간
    123 철길건널목
    200 : 단속구간(고정형 이동식)
    231 : 단속(카메라, 신호위반)    
    246 버스전용차로단속
    247 과적단속
    248 교통정보수집
    249 추월금지구간
    250 갓길단속
    251 적재불량단속
*/

