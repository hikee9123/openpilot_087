from cereal import car, log
from common.realtime import DT_CTRL
from selfdrive.car import apply_std_steer_torque_limits
from selfdrive.car.hyundai.hyundaican import create_lkas11, create_clu11, create_lfahda_mfc, create_mdps12
from selfdrive.car.hyundai.values import Buttons, CarControllerParams, CAR
from opendbc.can.packer import CANPacker

from selfdrive.car.hyundai.navicontrol  import NaviControl

VisualAlert = car.CarControl.HUDControl.VisualAlert
LaneChangeState = log.LateralPlan.LaneChangeState

def process_hud_alert(enabled, fingerprint, visual_alert, left_lane,
                      right_lane, left_lane_depart, right_lane_depart):
  sys_warning = (visual_alert in [VisualAlert.steerRequired, VisualAlert.ldw])

  # initialize to no line visible
  sys_state = 1
  if left_lane and right_lane or sys_warning:  # HUD alert only display when LKAS status is active
    sys_state = 3 if enabled or sys_warning else 4
  elif left_lane:
    sys_state = 5
  elif right_lane:
    sys_state = 6

  # initialize to no warnings
  left_lane_warning = 0
  right_lane_warning = 0
  if left_lane_depart:
    left_lane_warning = 1 if fingerprint in [CAR.GENESIS_G90, CAR.GENESIS_G80] else 2
  if right_lane_depart:
    right_lane_warning = 1 if fingerprint in [CAR.GENESIS_G90, CAR.GENESIS_G80] else 2

  return sys_warning, sys_state, left_lane_warning, right_lane_warning


class CarController():
  def __init__(self, dbc_name, CP, VM):
    self.p = CarControllerParams(CP)
    self.packer = CANPacker(dbc_name)

    self.apply_steer_last = 0
    self.car_fingerprint = CP.carFingerprint
    self.steer_rate_limited = False
    self.last_resume_frame = 0
    self.resume_cnt = 0
    self.last_lead_distance = 0
    self.lkas11_cnt = 0

    self.navi_ctrl = NaviControl(self.p)
    self.steerWarning_time = 0
    self.steer_torque_wait_timer = 0
    self.blinker_safety_timer = 0

  def lkas_active_control( self, enabled, CS, path_plan ):
    steerAngleDegAbs = abs(CS.out.steeringAngleDeg)

    
    if CS.out.steerWarning:
      self.steerWarning_time = 200
    elif self.steerWarning_time:
      if steerAngleDegAbs > 10:
        self.steerWarning_time = 200 
    elif self.steerWarning_time > 0:
      self.steerWarning_time -= 1


    if path_plan.laneChangeState != LaneChangeState.off:
      self.steer_torque_wait_timer = 0
      self.blinker_safety_timer = 0
    elif CS.out.leftBlinker or CS.out.rightBlinker:
      self.blinker_safety_timer += 1
      if self.blinker_safety_timer > 10:
        self.steer_torque_wait_timer = 200
  
    if self.steer_torque_wait_timer:
      if steerAngleDegAbs > 10:
        self.steer_torque_wait_timer = 200
    elif self.steer_torque_wait_timer > 0:
      self.steer_torque_wait_timer -= 1   
    
    
    lkas_active = enabled and not self.steerWarning_time and CS.out.vEgo >= CS.CP.minSteerSpeed and CS.out.cruiseState.enabled
    lkas_active = lkas_active and self.steer_torque_wait_timer == 0

    return lkas_active

  def update(self, c, CS, frame ):
    enabled = c.enabled
    actuators = c.actuators
    pcm_cancel_cmd = c.cruiseControl.cancel
    visual_alert = c.hudControl.visualAlert
    left_lane = c.hudControl.leftLaneVisible 
    right_lane = c.hudControl.rightLaneVisible 
    left_lane_depart = c.hudControl.leftLaneDepart 
    right_lane_depart = c.hudControl.rightLaneDepart
    
    # Steering Torque
    new_steer = int(round(actuators.steer * self.p.STEER_MAX))
    apply_steer = apply_std_steer_torque_limits(new_steer, self.apply_steer_last, CS.out.steeringTorque, self.p)
    self.steer_rate_limited = new_steer != apply_steer

    # disable when temp fault is active, or below LKA minimum speed
    #lkas_active = enabled and not CS.out.steerWarning and CS.out.vEgo >= CS.CP.minSteerSpeed and CS.out.cruiseState.enabled
    path_plan = self.navi_ctrl.update_lateralPlan()    
    lkas_active = self.lkas_active_control( enabled, CS, path_plan )

    if not lkas_active:
      apply_steer = 0

    self.apply_steer_last = apply_steer

    sys_warning, sys_state, left_lane_warning, right_lane_warning = \
      process_hud_alert(enabled, self.car_fingerprint, visual_alert,
                        left_lane, right_lane, left_lane_depart, right_lane_depart)


    if frame == 0: # initialize counts from last received count signals
      self.lkas11_cnt = CS.lkas11["CF_Lkas_MsgCount"] + 1

    self.lkas11_cnt %= 0x10

    can_sends = []
    can_sends.append(create_lkas11(self.packer, self.lkas11_cnt, self.car_fingerprint, apply_steer, lkas_active,
                                   CS.lkas11, sys_warning, sys_state, enabled,
                                   left_lane, right_lane,
                                   left_lane_warning, right_lane_warning))
    if apply_steer:
      can_sends.append( create_mdps12(self.packer, frame, CS.mdps12) )

    if pcm_cancel_cmd:
      can_sends.append(create_clu11(self.packer, frame, CS.clu11, Buttons.CANCEL))
    elif CS.out.cruiseState.standstill:
      # run only first time when the car stopped
      if self.last_lead_distance == 0:  
        # get the lead distance from the Radar
        self.last_lead_distance = CS.lead_distance
        self.resume_cnt = 0
      # when lead car starts moving, create 6 RES msgs
      elif CS.lead_distance != self.last_lead_distance and (frame - self.last_resume_frame) > 5:
        can_sends.append(create_clu11(self.packer, self.resume_cnt, CS.clu11, Buttons.RES_ACCEL))
        self.resume_cnt += 1
        # interval after 6 msgs
        if self.resume_cnt > 5:
          self.last_resume_frame = frame
          self.resume_cnt = 0
    # reset lead distnce after the car starts moving          
    elif self.last_lead_distance != 0:
      self.last_lead_distance = 0

    elif CS.out.cruiseState.accActive:
      btn_signal = self.navi_ctrl.update( CS )
      if btn_signal != None:
        can_sends.append(create_clu11(self.packer, self.resume_cnt, CS.clu11, btn_signal ))
        self.resume_cnt += 1
      else:
        self.resume_cnt = 0

    # 20 Hz LFA MFA message
    if frame % 5 == 0 and self.car_fingerprint in [CAR.SONATA, CAR.PALISADE, CAR.IONIQ, CAR.KIA_NIRO_EV, CAR.KONA_EV,
                                                   CAR.IONIQ_EV_2020, CAR.IONIQ_PHEV, CAR.KIA_CEED, CAR.KIA_SELTOS, CAR.ELANTRA_2021, CAR.ELANTRA_HEV_2021]:
      can_sends.append(create_lfahda_mfc(self.packer, enabled))

    self.lkas11_cnt += 1
    return can_sends
