# Thiết kế Mạch báo cháy gia đình sử dụng ESP32, FreeRTOS và MQTT  
Họ và tên: Nguyễn Lê Ngọc Sơn  
Mã sinh viên: 231432337  
Lớp: Điện Tử và Tin Học Công Nghiệp 2 - K64  
Học phần: Hệ thống Nhúng  
Đề tài: Thiết kế Mạch báo cháy gia đình sử dụng ESP32, FreeRTOS và MQTT  
Link Project Wokwi: https://wokwi.com/projects/477917058855337985  

## Đề 3. 
Thiết kế Mạch báo cháy gia đình bằng việc phát hiện khí gas/khói và nhiệt độ. Mạch 
báo cháy sử dụng ESP32 đọc dữ liệu nồng độ khí gas/khói từ cảm biến MQ2 và nhiệt 
độ từ cảm biến nhiệt, hiển thị dữ liệu qua giao tiếp serial. Đồng thời ESP32 gửi dữ liệu 
lên MQTT Broker qua Wifi và hiển thị dữ liệu trên Web/App. Yêu cầu thực hiện:    
• Vẽ mạch kết nối trong Wokwi.    
• Thiết kế giao diện trên Web/App với các dữ liệu đọc được từ Broker hiển thị trên   
thẻ html "class" và nút “button” để điều khiển cơ cấu chấp hành:    
+ 2 thẻ html hiển thị dữ liệu Nồng độ khí ga và Nhiệt độ dạng số.    
+ 2 biểu đồ cập nhật dữ liệu Nồng độ khí ga và Nhiệt độ trực tiếp theo thời gian   
thực (tùy chọn).    
+ 2 nút button để điều khiển trạng thái Máy bơm và Quạt (chế độ manual).   
• Xây dựng hệ RTOS cho hoạt động của ESP32, với các yêu cầu:   
+ “Task truyền data”: kết nối Wifi chạy Core 0, nếu thành công, đọc dữ liệu từ 
hàng đợi “data Queue”, và gửi lên MQTT broker.     
+ “Task đọc data”, chạy Core 1: đọc dữ liệu cảm biến MQ2 và nhiệt độ từ cảm 
biến nhiệt, hiển thị dữ liệu qua giao tiếp serial, gửi dữ liệu đầy đủ vào hàng đợi 
“data Queue” và vào hàng đợi “data Alert”     
+ “Tast cảnh báo”, chạy Core 1: đọc dữ liệu tử hàng đợi “data Alert” , và thực 
hiện điều khiển cơ cấu chấp hành là quạt và máy bơm. Quạt bật khi nồng độ khí 
gas vượt ngưỡng, Máy bơm bật khi nhiệt độ vượt ngưỡng.

## 1. Giới thiệu

Dự án xây dựng hệ thống cảnh báo cháy và giám sát môi trường sử dụng ESP32, kết hợp FreeRTOS và giao thức MQTT.  
Hệ thống sử dụng cảm biến MQ-2 để phát hiện khí gas, khói và cảm biến DHT22 để đo nhiệt độ, độ ẩm. Dữ liệu được ESP32 xử lý, đồng thời gửi lên MQTT Broker để Website giám sát và điều khiển thiết bị từ xa.  
Các chức năng chính:
- Giám sát nồng độ khí gas thông qua giá trị ADC từ MQ-2.
- Đo nhiệt độ và độ ẩm bằng cảm biến DHT22.
- Tự động kích hoạt quạt và bơm khi phát hiện điều kiện cảnh báo.
- Hỗ trợ hai chế độ AUTO và MANUAL.
- Giám sát dữ liệu và điều khiển thiết bị thông qua Website.
- Sử dụng FreeRTOS để phân chia các nhiệm vụ xử lý độc lập
## 2. Công nghệ sử dụng

| Công nghệ | Chức năng |
|---|---|
| ESP32 | Vi điều khiển trung tâm |
| FreeRTOS | Quản lý và thực thi các Task |
| MQTT | Giao thức truyền nhận dữ liệu |
| HiveMQ | MQTT Broker trung gian |
| WiFi | Kết nối mạng không dây |
| HTML | Xây dựng cấu trúc Website |
| CSS | Thiết kế giao diện Website |
| JavaScript | Xử lý dữ liệu và điều khiển MQTT |
| Arduino Framework | Lập trình ESP32 |

## 3. Phần cứng sử dụng

| STT | Linh kiện | Chức năng | Chân ESP32 |
|:---:|---|---|:---:|
| 1 | ESP32 | Bộ điều khiển trung tâm | - |
| 2 | MQ-2 | Phát hiện khí gas, khói | GPIO 34 |
| 3 | DHT22 | Đo nhiệt độ, độ ẩm | GPIO 4 |
| 4 | Quạt | Thiết bị xử lý khí gas | GPIO 25 |
| 5 | Bơm | Thiết bị xử lý khi nhiệt độ cao | GPIO 26 |

*Trong mô hình mô phỏng, LED được sử dụng để biểu diễn trạng thái hoạt động của quạt và bơm.*

## 4. Kiến trúc hệ thống

Hệ thống được chia thành ba nhiệm vụ chính sử dụng FreeRTOS:

| Task | Core | Priority | Stack | Nhiệm vụ |
|---|:---:|:---:|:---:|---|
| SensorTask | 1 | 2 | 4096 | Đọc cảm biến và gửi dữ liệu qua Queue |
| AlarmTask | 1 | 3 | 4096 | Xử lý cảnh báo và điều khiển thiết bị |
| NetworkTask | 0 | 1 | 6144 | Kết nối WiFi, MQTT và truyền nhận dữ liệu |

### Cơ chế đồng bộ và truyền dữ liệu

- **Queue:** Truyền dữ liệu cảm biến giữa SensorTask, AlarmTask và NetworkTask.
- **Mutex:** Bảo vệ dữ liệu dùng chung khi nhiều Task cùng truy cập.
- **FreeRTOS Task:** Cho phép các nhiệm vụ hoạt động độc lập.

## 5. Nguyên lý hoạt động

### 5.1. Thu thập dữ liệu cảm biến

- MQ-2 đọc giá trị khí gas thông qua ADC của ESP32.
- DHT22 đo nhiệt độ và độ ẩm định kỳ 2 giây.
- SensorTask cập nhật dữ liệu và gửi đến các Task xử lý thông qua Queue.
- Khi DHT22 đọc dữ liệu không thành công, hệ thống giữ lại giá trị hợp lệ gần nhất.

### 5.2. Xử lý cảnh báo

AlarmTask nhận dữ liệu từ Queue, kiểm tra ngưỡng cảnh báo và điều khiển thiết bị tương ứng.

| Thông số | Ngưỡng bật cảnh báo | Ngưỡng tắt cảnh báo |
|---|:---:|:---:|
| Khí gas (ADC) | >= 2000 | <= 1700 |
| Nhiệt độ | >= 50°C | <= 45°C |

Hệ thống sử dụng kỹ thuật Hysteresis nhằm tạo khoảng chênh lệch giữa ngưỡng bật và ngưỡng tắt, hạn chế hiện tượng thiết bị bật/tắt liên tục khi giá trị cảm biến dao động quanh ngưỡng.

**Nguyên tắc điều khiển:**

- Khí gas vượt ngưỡng: kích hoạt quạt.
- Nhiệt độ vượt ngưỡng: kích hoạt bơm.
- Khi có cảnh báo, hệ thống ưu tiên kích hoạt thiết bị tương ứng ngay cả khi đang ở chế độ MANUAL.

### 5.3. Chế độ hoạt động

| Chế độ | Nguyên lý |
|---|---|
| AUTO | ESP32 tự động điều khiển quạt và bơm dựa trên dữ liệu cảm biến và điều kiện cảnh báo |
| MANUAL | Người dùng điều khiển quạt và bơm thông qua Website |

### 5.4. Giám sát và điều khiển qua MQTT

ESP32 sử dụng HiveMQ Broker để trao đổi dữ liệu với Website.

- **ESP32 → MQTT Broker → Website:** Gửi dữ liệu cảm biến và trạng thái hệ thống.
- **Website → MQTT Broker → ESP32:** Gửi lệnh điều khiển chế độ hoạt động, quạt và bơm.

| Loại dữ liệu | Chu kỳ gửi |
|---|:---:|
| Dữ liệu cảm biến | 2 giây |
| Trạng thái hệ thống | 1 giây |

## 6. Cấu hình MQTT

### Thông tin MQTT Broker

| Thông số | Giá trị |
|---|---|
| Broker | `broker.hivemq.com` |
| Port | `1883` |
| Protocol | MQTT |

### Danh sách MQTT Topic

| Topic | Hướng truyền | Chức năng |
|---|---|---|
| `lab3_khi_gas/son_31432337/sensor` | ESP32 → Website | Gửi dữ liệu cảm biến |
| `lab3_khi_gas/son_31432337/status` | ESP32 → Website | Gửi trạng thái hệ thống |
| `lab3_khi_gas/son_31432337/mode/set` | Website → ESP32 | Điều khiển chế độ AUTO/MANUAL |
| `lab3_khi_gas/son_31432337/fan/set` | Website → ESP32 | Điều khiển quạt |
| `lab3_khi_gas/son_31432337/pump/set` | Website → ESP32 | Điều khiển bơm |

### Định dạng dữ liệu JSON

**Dữ liệu cảm biến:**

```json
{
  "gas_adc": 843,
  "temperature": 39.40,
  "humidity": 65.20
}
```

**Trạng thái hệ thống:**

```json
{
  "fan": true,
  "pump": false,
  "mode": "AUTO",
  "gas_alarm": true,
  "temp_alarm": false
}
```

**Lệnh điều khiển:**

| Topic | Nội dung gửi |
|---|---|
| `mode/set` | `AUTO` hoặc `MANUAL` |
| `fan/set` | `ON` hoặc `OFF` |
| `pump/set` | `ON` hoặc `OFF` |

## 7. Hướng dẫn cài đặt và sử dụng

### 7.1. Chuẩn bị môi trường

- Arduino IDE hoặc PlatformIO.
- Board ESP32.
- Các thư viện cần thiết:
  - DHT sensor library.
  - PubSubClient.
  - WiFi (ESP32 Arduino Core).

### 7.2. Cấu hình WiFi

Cập nhật thông tin WiFi trong chương trình ESP32:

```cpp
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
```

### 7.3. Cấu hình MQTT

```cpp
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
```

### 7.4. Nạp chương trình cho ESP32

1. Kết nối ESP32 với máy tính.
2. Mở project bằng Arduino IDE hoặc PlatformIO.
3. Chọn đúng Board ESP32 và cổng COM.
4. Kiểm tra cấu hình WiFi và MQTT.
5. Biên dịch và nạp chương trình.
6. Mở Serial Monitor để theo dõi hoạt động.

### 7.5. Khởi chạy Website

1. Mở thư mục Website của dự án.
2. Khởi chạy giao diện Website.
3. Đảm bảo Website sử dụng đúng MQTT Broker và các Topic đã cấu hình.
4. Theo dõi dữ liệu cảm biến và trạng thái thiết bị.
5. Thử chuyển đổi giữa chế độ AUTO và MANUAL.

## 8. Hình ảnh minh họa

### Sơ đồ kiến trúc hệ thống
<img width="975" height="545" alt="image" src="https://github.com/user-attachments/assets/fcd0094d-d86b-4184-a3a5-ddba5f0a582a" />

### Sơ đồ kết nối phần cứng
<img width="975" height="598" alt="image" src="https://github.com/user-attachments/assets/5ffdccb5-3ae6-4cd7-9a53-16341dcf4858" />

### Giao diện Website
<img width="975" height="1149" alt="image" src="https://github.com/user-attachments/assets/ce02d61f-e120-447a-812d-1fd60a37606d" />


## 9. Hạn chế và hướng phát triển

### Hạn chế

- Giá trị MQ-2 hiện được sử dụng dưới dạng ADC thô, chưa hiệu chuẩn để chuyển đổi chính xác sang đơn vị ppm.
- Hệ thống sử dụng MQTT Broker công cộng và kết nối cổng 1883 chưa được mã hóa.
- Chưa triển khai cơ chế lưu trữ và khôi phục lịch sử dữ liệu khi mất kết nối mạng.
- Ngưỡng cảnh báo hiện được thiết lập cố định trong chương trình.

### Hướng phát triển

- Hiệu chuẩn cảm biến MQ-2 để cải thiện khả năng đo lường.
- Bổ sung xác thực và mã hóa kết nối MQTT.
- Xây dựng cơ sở dữ liệu lưu trữ lịch sử cảm biến.
- Phát triển chức năng thống kê và cảnh báo theo thời gian thực.
- Triển khai mô hình trên phần cứng thực tế.



**ESP32 | FreeRTOS | MQTT | IoT**
