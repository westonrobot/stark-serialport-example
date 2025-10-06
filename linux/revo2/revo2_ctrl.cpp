#include <stdio.h>
#include "stark-sdk.h"
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>

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
  uint8_t slave_id = cfg->slave_id;
  auto handle = modbus_open(cfg->port_name, cfg->baudrate);
  free_device_config(cfg);

  get_device_info(handle, slave_id);

  // 设置手指控制参数的单位模式
  stark_set_finger_unit_mode(handle, slave_id, FINGER_UNIT_MODE_NORMALIZED);
  // stark_set_finger_unit_mode(handle, slave_id, FINGER_UNIT_MODE_PHYSICAL);

  auto mode = stark_get_finger_unit_mode(handle, slave_id);
  if (mode == FINGER_UNIT_MODE_NORMALIZED) {
    printf("Finger unit mode: Normalized\n");
  } else if (mode == FINGER_UNIT_MODE_PHYSICAL) {
    printf("Finger unit mode: Physical\n");
  } else {
    printf("Finger unit mode: Unknown\n");
  }

  // Enable touch sensor feedback
  // stark_enable_touch_sensor(handle, slave_id, 0b11111);  // 启用全部触觉传感器

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

  while (true) {
    for (int number = 0; number <= 9; number++) {
      printf("Starting counting sequence from 0 to 9...\n");
      printf("Displaying number: %d\n", number);

      // Set finger positions for the current number
      stark_set_finger_positions_and_durations(
          handle, slave_id, number_positions[number], durations, 6);

      // Wait for fingers to reach target position
      usleep(delay);

      // Get and display current finger status
      auto finger_status = stark_get_motor_status(handle, slave_id);
      if (finger_status != NULL) {
        printf("Number %d - Positions: %hu, %hu, %hu, %hu, %hu, %hu\n", number,
               finger_status->positions[0], finger_status->positions[1],
               finger_status->positions[2], finger_status->positions[3],
               finger_status->positions[4], finger_status->positions[5]);
        free_motor_status_data(finger_status);
      }
    }

    usleep(delay * 3);  // Pause before starting the sequence

    for (int action_id = ACTION_SEQUENCE_ID_DEFAULT_GESTURE_OPEN;
         action_id <= ACTION_SEQUENCE_ID_DEFAULT_GESTURE_POINT; action_id++) {
      printf("Running action sequence ID: %d\n", action_id);
      // 运行预定义的动作序列
      stark_run_action_sequence(handle, slave_id, (ActionSequenceId)action_id);
      usleep(delay * 5);  // Wait for the action sequence to complete
    }

    // usleep(delay * 3);  // Pause before reading the touch sensor data

    // 读取并显示触觉传感器数据
    // auto touch_data = stark_get_touch_status(handle, slave_id);
    // if (touch_data != NULL) {
    //   for (int i = 0; i < 5; i++) {
    //     printf(
    //         "Finger %d - Normal Forces: %hu, %hu, %hu | Tangential Forces: "
    //         "%hu, %hu, %hu | Tangential Directions: %hu, %hu, %hu | Self "
    //         "Proximity: %u, %u | Mutual Proximity: %u | Status: %hu\n",
    //         i, touch_data->items[i].normal_force1,
    //         touch_data->items[i].normal_force2,
    //         touch_data->items[i].normal_force3,
    //         touch_data->items[i].tangential_force1,
    //         touch_data->items[i].tangential_force2,
    //         touch_data->items[i].tangential_force3,
    //         touch_data->items[i].tangential_direction1,
    //         touch_data->items[i].tangential_direction2,
    //         touch_data->items[i].tangential_direction3,
    //         touch_data->items[i].self_proximity1,
    //         touch_data->items[i].self_proximity2,
    //         touch_data->items[i].mutual_proximity, touch_data->items[i].status);
    //   }
    //   free_touch_finger_data(touch_data);
    // }

    printf("Demo cycle completed. Restarting...\n");
    usleep(delay * 15);  // Pause before restarting the sequence
  }
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
