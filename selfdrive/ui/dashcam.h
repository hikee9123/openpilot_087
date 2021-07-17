#include <time.h>
#include <dirent.h>

#define CAPTURE_STATE_NONE 0
#define CAPTURE_STATE_CAPTURING 1
#define CAPTURE_STATE_NOT_CAPTURING 2
#define CAPTURE_STATE_PAUSED 3
#define CLICK_TIME 0.2
#define RECORD_INTERVAL 180 // Time in seconds to rotate recordings; Max for screenrecord is 3 minutes
#define RECORD_FILES 40     // Number of files to create before looping over



long nCurrTimeSec = 0;
int captureState = CAPTURE_STATE_NOT_CAPTURING;
int captureNum = 0;
int start_time = 0;
int stop_time = 0;
int elapsed_time = 0; // Time of current recording
int click_elapsed_time = 0;
int click_time = 0;
char filenames[RECORD_FILES][50]; // Track the filenames so they can be deleted when rotating

bool lock_current_video = false; // If true save the current video before rotating
bool locked_files[RECORD_FILES]; // Track which files are locked
int lock_image;                  // Stores reference to the PNG
int files_created = 0;
int  capture_cnt = 0;
int  program_start = 1;


int get_time()
{
  // Get current time (in seconds)

  int iRet;
  struct timeval tv;
  int seconds = 0;

  iRet = gettimeofday(&tv, NULL);
  if (iRet == 0)
  {
    seconds = (int)tv.tv_sec;
  }
  return seconds;
}

struct tm get_time_struct()
{
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  return tm;
}

void remove_file(char *videos_dir, char *filename)
{
  if (filename[0] == '\0')
  {
    // Don't do anything if no filename is passed
    return;
  }

  int status;
  char fullpath[64];
  snprintf(fullpath, sizeof(fullpath), "%s/%s", videos_dir, filename);
  status = remove(fullpath);
  if (status == 0)
  {
    printf("Removed file: %s\n", fullpath);
  }
  else
  {
    printf("Unable to remove file: %s\n", fullpath);
    perror("Error message:");
  }
}

void save_file(char *videos_dir, char *filename)
{
  if (!strlen(filename))
  {
    return;
  }

  // Rename file to save it from being overwritten
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "mv %s/%s %s/saved_%s", videos_dir, filename, videos_dir, filename);
  printf("save: %s\n", cmd);
  system(cmd);
}

void stop_capture() {
  char videos_dir[50] = "/storage/emulated/0/videos";

  

  if (captureState == CAPTURE_STATE_CAPTURING)
  {
    printf("stop_capture()\n ");
    system("killall -SIGINT screenrecord");
    captureState = CAPTURE_STATE_NOT_CAPTURING;
    elapsed_time = nCurrTimeSec - start_time;
    if (elapsed_time < 3)
    {
      remove_file(videos_dir, filenames[captureNum]);
    }
    else
    {
      //printf("Stop capturing screen\n");
      captureNum++;

      if (captureNum > RECORD_FILES - 1)
      {
        captureNum = 0;
      }
    }
  }
}

void start_capture()
{
  captureState = CAPTURE_STATE_CAPTURING;
  char cmd[128] = "";
  char videos_dir[50] = "/storage/emulated/0/videos";

  printf("start_capture()\n ");

  //////////////////////////////////
  // NOTE: make sure videos_dir folder exists on the device!
  //////////////////////////////////
  struct stat st = {0};
  if (stat(videos_dir, &st) == -1)
  {
    mkdir(videos_dir, 0700);
  }

  if (strlen(filenames[captureNum]) && files_created >= RECORD_FILES)
  {
    if (locked_files[captureNum] > 0)
    {
      save_file(videos_dir, filenames[captureNum]);
    }
    else
    {
      // remove the old file
      remove_file(videos_dir, filenames[captureNum]);
    }
    locked_files[captureNum] = 0;
  }

  char filename[64];
  struct tm tm = get_time_struct();
  snprintf(filename, sizeof(filename), "%04d%02d%02d-%02d%02d%02d.mp4", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  snprintf(cmd, sizeof(cmd), "screenrecord --size 1280x720 --bit-rate 5000000 %s/%s&", videos_dir, filename);
  //snprintf(cmd,sizeof(cmd),"screenrecord --size 960x540 --bit-rate 5000000 %s/%s&",videos_dir,filename);
  strcpy(filenames[captureNum], filename);

  printf("Capturing to file: %s\n", cmd);
  start_time = nCurrTimeSec;
  system(cmd);

  if (lock_current_video)
  {
    // Lock is still on so mark this file for saving
    locked_files[captureNum] = 1;
  }
  else
  {
    locked_files[captureNum] = 0;
  }

  files_created++;
}


bool screen_button_clicked(int touch_x, int touch_y, int x, int y, int cx, int cy )
{
   int   cx_half = cx * 0.5;
   int   cy_half = cy * 0.5;

   int min_x = x - cx_half;
   int min_y = y - cy_half;
   int max_x = x + cx_half;
   int max_y = y + cy_half;

  if (touch_x >= min_x && touch_x <= max_x)
  {
    if (touch_y >= min_y && touch_y <= max_y)
    {
      return true;
    }
  }
  return false;
}

void draw_date_time(UIState *s)
{
  // Get local time to display
  char now[50];
  struct tm tm = get_time_struct();
  snprintf(now, sizeof(now), "%04d/%02d/%02d  %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);


  nvgFontSize(s->vg, 60);
  nvgFontFace(s->vg, "sans-semibold");
  nvgFillColor(s->vg, nvgRGBA(255, 255, 255, 200));

  const int bb_dmr_x = s->viz_rect.x + s->viz_rect.w - 200;   // 1602
  nvgText(s->vg, bb_dmr_x, 23, now, NULL);
}


static void rotate_video()
{
  // Overwrite the existing video (if needed)
  elapsed_time = 0;
  stop_capture();
  captureState = CAPTURE_STATE_CAPTURING;
  start_capture();
}


void screen_toggle_record_state()
{
  //if (captureState == CAPTURE_STATE_CAPTURING)
  if( lock_current_video == true )
  {
    stop_capture();
    lock_current_video = false;
  }
  else
  {
    // start_capture();
    lock_current_video = true;
  }
}

static void draw_button( UIState *s, const char* string, Rect rect, NVGcolor fillColor, NVGcolor txtColor ) 
{
    int btn_x = rect.x;
    int btn_y = rect.y;
    int btn_w = rect.w;
    int btn_h = rect.h;


    nvgBeginPath(s->vg);
    nvgRoundedRect(s->vg, btn_x, btn_y, btn_w, btn_h, 100);
    nvgStrokeColor(s->vg, nvgRGBA(0, 0, 0, 80));
    nvgStrokeWidth(s->vg, 6);
    nvgStroke(s->vg);

    //NVGcolor fillColor = nvgRGBA(255,0,0,150);
    nvgFillColor(s->vg, fillColor);
    nvgFill(s->vg);
    nvgFillColor(s->vg, txtColor );   // txtColor = nvgRGBA(255, 255, 255, 200)
    int btn_xc = rect.centerX();
    int btn_yc = rect.centerY();

    nvgFontSize(s->vg, 45);
    nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);    
    nvgText(s->vg, btn_xc, btn_yc, string, NULL);
}

static void screen_draw_button(UIState *s, int touch_x, int touch_y, int touched)
{
  draw_date_time(s);
  // Set button to bottom left of screen
  nvgTextAlign(s->vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);


    int btn_w = 150;
    int btn_h = 150;
    int bb_dmr_x = s->viz_rect.x + s->viz_rect.w + 100;
    int btn_x = bb_dmr_x - btn_w;
    int btn_y = 1080 - btn_h;    

  if ( touched && screen_button_clicked(touch_x, touch_y, btn_x, btn_y, btn_w, btn_h) )
  {
    click_elapsed_time = nCurrTimeSec - click_time;

    printf( "screen_button_clicked %d  captureState = %d \n", click_elapsed_time, captureState );
    if (click_elapsed_time > 0)
    {
      click_time = nCurrTimeSec + 1;
      screen_toggle_record_state();
    }
  }  


    nvgBeginPath(s->vg);
    nvgRoundedRect(s->vg, btn_x - 110, btn_y - 45, btn_w, btn_h, 100);
    nvgStrokeColor(s->vg, nvgRGBA(255, 255, 255, 80));
    nvgStrokeWidth(s->vg, 6);
    nvgStroke(s->vg);

    nvgFontSize(s->vg, 60);

    if ( lock_current_video == false )
    {
       nvgFillColor(s->vg, nvgRGBA( 50, 50, 100, 200));
    }
    else if (captureState == CAPTURE_STATE_CAPTURING)
    {
      NVGcolor fillColor = nvgRGBA(255, 0, 0, 150);
      nvgFillColor(s->vg, fillColor);
      nvgFill(s->vg);
      nvgFillColor(s->vg, nvgRGBA(255, 255, 255, 200));
    }
    else
    {
      nvgFillColor(s->vg, nvgRGBA(255, 150, 150, 200));
    }
    nvgText(s->vg, btn_x - 75, btn_y + 50, "REC", NULL);


  if (captureState == CAPTURE_STATE_CAPTURING)
  {

    elapsed_time = nCurrTimeSec - start_time;
    if (elapsed_time >= RECORD_INTERVAL)
    {
      capture_cnt++;
      if( capture_cnt > 10 )
      {
        stop_capture();
        lock_current_video = false;
      }
      else
      {
        rotate_video(); 
      }
    }    
  }
}





int get_param( const std::string &key )
{
    auto str = QString::fromStdString(Params().get( key ));
    int value = str.toInt();

    return value;
}



void update_dashcam(UIState *s, int draw_vision)
{
  nCurrTimeSec =  get_time();
  if (!s->awake) return;

  if ( program_start )
  {
    program_start = 0;

    s->scene.scr.autoFocus = get_param("OpkrAutoFocus");
    s->scene.scr.autoScreenOff = get_param("OpkrAutoScreenOff");
    s->scene.scr.brightness = get_param("OpkrUIBrightness");

    s->scene.scr.nTime = s->scene.scr.autoScreenOff * 60 * UI_FREQ;
  }
  else if ( touched  ) 
  {
    s->scene.mouse.touched = 0; 
  }


  if (!draw_vision) return;
  if (!s->scene.started) return;


  screen_draw_button(s, touch_x, touch_y, touched);


  if( lock_current_video == true  )
  {
    float v_ego = s->scene.car_state.getVEgo();
    int engaged = s->scene.controls_state.getEngageable();
    if(  (v_ego < 0.1 || !engaged) )
    {
      elapsed_time = nCurrTimeSec - stop_time;
      if( captureState == CAPTURE_STATE_CAPTURING && elapsed_time > 2 )
      {
        capture_cnt = 0;
        stop_capture();
      }
    }    
    else if( captureState != CAPTURE_STATE_CAPTURING )
    {
      capture_cnt = 0;
      start_capture();
    }
    else
    {
      stop_time = nCurrTimeSec;
    }
    
  }
  else  if( captureState == CAPTURE_STATE_CAPTURING )
  {
    capture_cnt = 0;
    stop_capture();
  }
}





