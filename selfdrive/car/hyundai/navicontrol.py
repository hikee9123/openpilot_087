import math
import numpy as np



from selfdrive.config import Conversions as CV
from selfdrive.car.hyundai.values import Buttons
from selfdrive.controls.lib.lane_planner import TRAJECTORY_SIZE
from common.numpy_fast import clip, interp
import cereal.messaging as messaging

import common.log as trace1



MAX_SPEED = 255.0
MIN_CURVE_SPEED = 30.


class NaviControl():
  def __init__(self, p = None ):
    self.p = p
    self.accel_steady = 0
    self.scc12_cnt = 0
    
    self.btn_cnt = 0
    self.seq_command = 0
    self.target_speed = 0
    self.set_point = 0
    self.wait_timer2 = 0

    self.prev_clu_CruiseSwState = 0       
    self.prev_VSetDis  = 0
    self.curise_set_first = 0
    self.curise_sw_check = False
    self.cruise_set_mode = 1      # 초기 선택 모델.
    self.cruise_set_speed_kph = 30

    self.curve_speed = 0
    self.curvature_gain = 1

    self.sm = messaging.SubMaster(['liveNaviData'])




  def update_btn(self, CS ): 
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


  def update_ascc( self, CS, set_speed ):
    self.set_point = max(30,set_speed)
    self.curr_speed = CS.out.vEgo * CV.MS_TO_KPH
    self.VSetDis   = CS.VSetDis
    btn_signal = self.switch( self.seq_command )

    return btn_signal


  def update_navi(self, sm, CS ):
    v_ego_kph = CS.out.vEgo * CV.MS_TO_KPH    
    cruise_set_speed_kph = self.cruise_set_speed_kph
    self.liveNaviData = sm['liveNaviData']    
    speedLimit = self.liveNaviData.speedLimit
    speedLimitDistance = self.liveNaviData.speedLimitDistance
    safetySign  = self.liveNaviData.safetySign
    mapValid = self.liveNaviData.mapValid
    trafficType = self.liveNaviData.trafficType
    
    if not mapValid or trafficType == 0:
      return  cruise_set_speed_kph

    elif CS.is_highway:
      return  cruise_set_speed_kph
    else:
      if speedLimit <= 50  and cruise_set_speed_kph <= 80:
        spdTarget = interp( speedLimitDistance, [50,300], [ speedLimit, cruise_set_speed_kph ] )
      elif  cruise_set_speed_kph <= 100:
        nCenter300 = min( cruise_set_speed_kph, speedLimit + 10 )
        spdTarget = interp( speedLimitDistance, [50, 250, 600], [ speedLimit,  nCenter300, cruise_set_speed_kph ] )      
      else:
        nCenter300 = min( cruise_set_speed_kph, speedLimit + 10 )
        spdTarget = interp( speedLimitDistance, [100, 300, 600], [ speedLimit,  nCenter300, cruise_set_speed_kph ] )
    
    if v_ego_kph < speedLimit:
      v_ego_kph = speedLimit

    cruise_set_speed_kph = min( spdTarget, v_ego_kph )

    return  cruise_set_speed_kph

  def update_main(self,  CS ):  
    # send scc to car if longcontrol enabled and SCC not on bus 0 or ont live
    # atom
    self.sm.update(0) 

    btn_signal = None
    self.cruise_set_speed_kph = CS.out.cruiseState.speed * CV.MS_TO_KPH
    if self.update_btn( CS  ) == 0:
      pass
    elif CS.acc_active:
      kph_set_vEgo = self.update_navi(  self.sm , CS )
      self.ctrl_speed = min( self.cruise_set_speed_kph, kph_set_vEgo)
      btn_signal = self.update_ascc( CS, self.ctrl_speed )      
 

    return btn_signal