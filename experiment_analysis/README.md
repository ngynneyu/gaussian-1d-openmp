# Tổng hợp thực nghiệm chính

Thư mục này chứa các CSV và 9 biểu đồ tổng hợp từ ba lượt chính thức trong `../results/`:

- `windows_20261119_152801_1982409/`
- `windows_20261119_153808_5914934/`
- `windows_20261119_154648_2619652/`

`main_summary_3runs.csv` là bảng gộp; `main_by_run.csv` giữ số liệu từng lượt; `main_image_medians_by_run.csv` giữ kết quả theo từng ảnh; `validation.csv` ghi số dòng và kiểm tra tính đúng đắn. Các biểu đồ Gaussian, Sobel và Laplacian được giữ để đối chiếu và hoàn thiện báo cáo.

Để tái tạo bảng và biểu đồ từ ba thư mục kết quả, chạy tại repository root sau khi cài các gói trong `requirements.txt`:

```powershell
python .\experiment_analysis\tong_hop.py
```

Script chỉ đọc ba lượt thực nghiệm chính; benchmark mở rộng V5 được tổng hợp riêng trong `image_filter_scaling/`.
