import math
import numpy as np



from selfdrive.config import Conversions as CV
from selfdrive.car.hyundai.values import Buttons
from selfdrive.controls.lib.lane_planner import TRAJECTORY_SIZE
from common.numpy_fast import clip, interp
import cereal.messaging as messaging

import common.log as trace1





class NaviControl():
  def __init__(self, p = None ):
    self.p = p
    
    self.sm = messaging.SubMaster(['liveNaviData','lateralPlan']) 

    self.btn_cnt = 0
    self.seq_command = 0
    self.target_speed = 0
    self.set_point = 0
    self.wait_timer2 = 0



  def update_lateralPlan( self ):
    self.sm.update(0)
    path_plan = self.sm['lateralPlan']
    return path_plan



  def button_status(self, CS ): 
    if not CS.acc_active or CS.cruise_buttons != Buttons.NONE: 
      self.wait_timer2 = 50 
    elif self.wait_timer2: 
      self.wait_timer2 -= 1
    else:
      return 1
    return 0




  # buttn acc,dec control
  def switch(self, seq_cmd):
      self.case_name = "case_" + str(seq_cmd)
      self.case_func = getattr( self, self.case_name, lambda:"default")
      return self.case_func()

  def reset_btn(self):
      if self.seq_command != 3:
        self.seq_command = 0


  def case_default(self):
      self.seq_command = 0
      return None

  def case_0(self):
      self.btn_cnt = 0
      self.target_speed = self.set_point
      delta_speed = self.target_speed - self.VSetDis
      if delta_speed > 1:
        self.seq_command = 1
      elif delta_speed < -1:
        self.seq_command = 2
      return None

  def case_1(self):  # acc
      btn_signal = Buttons.RES_ACCEL
      self.btn_cnt += 1
      if self.target_speed == self.VSetDis:
        self.btn_cnt = 0
        self.seq_command = 3            
      elif self.btn_cnt > 10:
        self.btn_cnt = 0
        self.seq_command = 3
      return btn_signal


  def case_2(self):  # dec
      btn_signal = Buttons.SET_DECEL
      self.btn_cnt += 1
      if self.target_speed == self.VSetDis:
        self.btn_cnt = 0
        self.seq_command = 3            
      elif self.btn_cnt > 10:
        self.btn_cnt = 0
        self.seq_command = 3
      return btn_signal

  def case_3(self):  # None
      btn_signal = None  # Buttons.NONE
      
      self.btn_cnt += 1
      #if self.btn_cnt == 1:
      #  btn_signal = Buttons.NONE
      if self.btn_cnt > 5: 
        self.seq_command = 0
      return btn_signal


  def ascc_button_control( self, CS, set_speed ):
    self.set_point = max(30,set_speed)
    self.curr_speed = CS.out.vEgo * CV.MS_TO_KPH
    self.VSetDis   = CS.VSetDis
    btn_signal = self.switch( self.seq_command )

    return btn_signal


  def get_navi_speed(self, sm, CS, cruiseState_speed ):
    cruise_set_speed_kph = cruiseState_speed
    v_ego_kph = CS.out.vEgo * CV.MS_TO_KPH    
    self.liveNaviData = sm['liveNaviData']    
    speedLimit = self.liveNaviData.speedLimit
    speedLimitDistance = self.liveNaviData.speedLimitDistance
    safetySign  = self.liveNaviData.safetySign
    mapValid = self.liveNaviData.mapValid
    trafficType = self.liveNaviData.trafficType
    
    if not mapValid or trafficType == 0:
      return  cruise_set_speed_kph

    elif CS.is_highway or speedLimit < 30:
      return  cruise_set_speed_kph
    elif speedLimitDistance >= 10:
      if speedLimit <= 50:
        spdTarget = interp( speedLimitDistance, [20, 600], [ speedLimit, speedLimit + 50 ] )
      else:
        spdTarget = interp( speedLimitDistance, [50, 700], [ speedLimit, speedLimit + 50 ] )
    else:
      spdTarget = speedLimit

    str_log1 = 'VD={:5.0f} SL={:5.0f} TG={:5.0f} DC={:.0f}'.format( CS.VSetDis, speedLimit, spdTarget, speedLimitDistance  )
    trace1.printf3( '  {}'.format( str_log1 ) )

    if v_ego_kph < speedLimit:
      v_ego_kph = speedLimit

    cruise_set_speed_kph = min( spdTarget, v_ego_kph )
    return  cruise_set_speed_kph

  def update(self,  CS ):  
    # send scc to car if longcontrol enabled and SCC not on bus 0 or ont live
    # atom
    #self.sm.update(0)

    btn_signal = None
    if not self.button_status( CS  ):
      pass
    elif CS.acc_active:
      cruiseState_speed = CS.out.cruiseState.speed * CV.MS_TO_KPH      
      kph_set_vEgo = self.get_navi_speed(  self.sm , CS, cruiseState_speed )
      self.ctrl_speed = min( cruiseState_speed, kph_set_vEgo)
      btn_signal = self.ascc_button_control( CS, self.ctrl_speed )
 

    return btn_signal
