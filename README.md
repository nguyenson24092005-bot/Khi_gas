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
