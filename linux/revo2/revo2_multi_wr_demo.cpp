#include <stdio.h>
#include "stark-sdk.h"
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>

// Comment out if not needed
#define LEFT_HAND_PRESENT
#define RIGHT_HAND_PRESENT

// 声明函数
void get_device_info(DeviceHandler *handleint, uint8_t slave_id);
void get_info(DeviceHandler *handle, uint8_t slave_id);

void handler(int sig) {
  void *array[10];
  size_t size;

  // 获取堆栈帧
  size = backtrace(array, 10);

  // 打印所有堆栈帧到 stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

int main(int argc, char const *argv[]) {
  signal(SIGSEGV,
         handler);  // Install our handler for SIGSEGV (segmentation fault)
  signal(SIGABRT, handler);  // Install our handler for SIGABRT (abort signal)

  init_cfg(STARK_PROTOCOL_TYPE_MODBUS, LOG_LEVEL_DEBUG);  // 初始化配置
  auto cfg = auto_detect_modbus_revo2(
      "/dev/ttyUSB0", true);  // 替换为实际的串口名称, 传None会尝试自动检测
  if (cfg == NULL) {
    fprintf(stderr, "Failed to auto-detect Modbus device configuration.\n");
    return -1;
  }

  auto handle = modbus_open(cfg->port_name, cfg->baudrate);
  free_device_config(cfg);

  // 方式1：单个串口连接多个设备（需要配置不同的设备ID）

#ifdef LEFT_HAND_PRESENT
  uint8_t slave_id_left = 0x7e;
  get_device_info(handle, slave_id_left);

  stark_set_finger_unit_mode(handle, slave_id_left,
                             FINGER_UNIT_MODE_NORMALIZED);

  auto mode_left = stark_get_finger_unit_mode(handle, slave_id_left);
  if (mode_left == FINGER_UNIT_MODE_NORMALIZED) {
    printf("Left Finger unit mode: Normalized\n");
  } else if (mode_left == FINGER_UNIT_MODE_PHYSICAL) {
    printf("Left Finger unit mode: Physical\n");
  } else {
    printf("Left Finger unit mode: Unknown\n");
  }

  stark_run_action_sequence(handle, slave_id_left,
                            ACTION_SEQUENCE_ID_DEFAULT_GESTURE_OPEN);
#endif

#ifdef RIGHT_HAND_PRESENT
  uint8_t slave_id_right = 0x7f;
  get_device_info(handle, slave_id_right);

  // 设置手指控制参数的单位模式
  stark_set_finger_unit_mode(handle, slave_id_right,
                             FINGER_UNIT_MODE_NORMALIZED);

  auto mode_right = stark_get_finger_unit_mode(handle, slave_id_right);
  if (mode_right == FINGER_UNIT_MODE_NORMALIZED) {
    printf("Right Finger unit mode: Normalized\n");
  } else if (mode_right == FINGER_UNIT_MODE_PHYSICAL) {
    printf("Right Finger unit mode: Physical\n");
  } else {
    printf("Right Finger unit mode: Unknown\n");
  }

  stark_run_action_sequence(handle, slave_id_right,
                            ACTION_SEQUENCE_ID_DEFAULT_GESTURE_OPEN);
#endif

  useconds_t delay = 1000 * 1000;  // 1000ms delay between numbers

  // Define finger positions for numbers 0-9
  // Array format: [thumb, thumb_aux, index, middle, ring, pinky]
  // 0 = fully open, 1000 = fully closed
  uint16_t number_positions[10][6] = {
      // 0: Closed fist
      {700, 400, 1000, 1000, 1000, 1000},
      // 1: Only index finger extended
      {700, 400, 0, 1000, 1000, 1000},
      // 2: Index and middle fingers extended
      {700, 400, 0, 0, 1000, 1000},
      // 3: Index, middle, and ring fingers extended
      {700, 400, 0, 0, 0, 1000},
      // 4: All fingers except thumb extended
      {700, 400, 0, 0, 0, 0},
      // 5: All fingers extended (open hand)
      {0, 0, 0, 0, 0, 0},
      // 6: Thumb and pinky extended (like "hang loose" gesture)
      {0, 0, 1000, 1000, 1000, 0},
      // 7: Thumb, index, and middle extended
      {0, 0, 0, 1000, 1000, 1000},
      // 8: All fingers except pinky extended
      {0, 0, 0, 0, 1000, 1000},
      // 9: All fingers except ring and pinky extended
      {0, 0, 0, 0, 0, 1000}};
  uint16_t durations[6] = {1000, 1000, 1000, 1000, 1000, 1000};

  printf(
      "Starting parallel demo: Left hand counting 0-9, Right hand performing "
      "action sequences...\n");

  int number = 0;
  int action_id = ACTION_SEQUENCE_ID_DEFAULT_GESTURE_OPEN;

  while (true) {
#ifdef LEFT_HAND_PRESENT
    // Left hand: counting sequence
    printf("Left hand displaying number: %d\n", number);
    stark_set_finger_positions_and_durations(
        handle, slave_id_left, number_positions[number], durations, 6);
    usleep(delay);
#endif

#ifdef RIGHT_HAND_PRESENT
    // Right hand: action sequences
    printf("Right hand running action sequence ID: %d\n", action_id);
    stark_run_action_sequence(handle, slave_id_right,
                              (ActionSequenceId)action_id);
    usleep(delay);
#endif

#ifdef LEFT_HAND_PRESENT
    // Get and display status for both hands
    auto finger_status_left = stark_get_motor_status(handle, slave_id_left);
    if (finger_status_left != NULL) {
      printf("Left hand number %d - Positions: %hu, %hu, %hu, %hu, %hu, %hu\n",
             number, finger_status_left->positions[0],
             finger_status_left->positions[1], finger_status_left->positions[2],
             finger_status_left->positions[3], finger_status_left->positions[4],
             finger_status_left->positions[5]);
      free_motor_status_data(finger_status_left);
    }
    usleep(delay);
#endif

#ifdef RIGHT_HAND_PRESENT
    auto finger_status_right = stark_get_motor_status(handle, slave_id_right);
    if (finger_status_right != NULL) {
      printf(
          "Right hand action %d - Positions: %hu, %hu, %hu, %hu, %hu, %hu\n",
          action_id, finger_status_right->positions[0],
          finger_status_right->positions[1], finger_status_right->positions[2],
          finger_status_right->positions[3], finger_status_right->positions[4],
          finger_status_right->positions[5]);
      free_motor_status_data(finger_status_right);
    }
    usleep(delay);
#endif

    // Increment counters
    number++;
    if (number > 9) {
      number = 0;
      printf("Left hand counting cycle completed. Restarting from 0...\n");
    }

    action_id++;
    if (action_id > ACTION_SEQUENCE_ID_DEFAULT_GESTURE_POINT) {
      action_id = ACTION_SEQUENCE_ID_DEFAULT_GESTURE_OPEN;
      printf("Right hand action sequence cycle completed. Restarting...\n");
    }
  }

  modbus_close(handle);
  return 0;
}

// 获取设备序列号、固件版本等信息
void get_device_info(DeviceHandler *handle, uint8_t slave_id) {
  auto info = stark_get_device_info(handle, slave_id);
  if (info != NULL) {
    printf(
        "Slave[%hhu] SKU Type: %hhu, Serial Number: %s, Firmware Version: %s\n",
        slave_id, (uint8_t)info->sku_type, info->serial_number,
        info->firmware_version);
    if (info->hardware_type == STARK_HARDWARE_TYPE_REVO1_TOUCH ||
        info->hardware_type == STARK_HARDWARE_TYPE_REVO2_TOUCH) {
      // 启用全部触觉传感器
      stark_enable_touch_sensor(handle, slave_id, 0x1F);
      usleep(1000 * 1000);  // wait for touch sensor to be ready
    }
    free_device_info(info);
  }
}

// 获取设备信息, 波特率, LED信息, 按键事件
void get_info(DeviceHandler *handle, uint8_t slave_id) {
  // RS485串口波特率
  auto baudrate = stark_get_rs485_baudrate(handle, slave_id);
  printf("Slave[%hhu] Baudrate: %d\n", slave_id, baudrate);

  // CANFD波特率
  auto canfd_baudrate = stark_get_canfd_baudrate(handle, slave_id);
  printf("Slave[%hhu] CANFD Baudrate: %d\n", slave_id, canfd_baudrate);

  auto led_info = stark_get_led_info(handle, slave_id);
  if (led_info != NULL) {
    printf("Slave[%hhu] LED Info: %hhu, %hhu\n", slave_id, led_info->mode,
           led_info->color);
    free_led_info(led_info);
  }

  auto button_event = stark_get_button_event(handle, slave_id);
  if (button_event != NULL) {
    printf("Slave[%hhu] Button Event: %d, %d, %hhu\n", slave_id,
           button_event->timestamp, button_event->button_id,
           button_event->press_state);
    free_button_event(button_event);
  }
}
