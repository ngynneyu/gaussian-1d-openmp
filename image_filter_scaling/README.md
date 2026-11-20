# Benchmark mở rộng V5 - 100 ảnh

Thư mục này là thực nghiệm mở rộng mục 3.4. V5 so sánh bản tuần tự với OpenMP chia hàng trong từng ảnh (`intra`, 16 luồng) trên 100 ảnh xám 2048×2048 cho Gaussian, Sobel và Laplacian. Ba lượt chính thức nằm trong `results/batch_v5_*`; bảng gộp và biểu đồ nằm ở `batch_report_v5/`.

## Cấu trúc và dữ liệu

- `benchmark.c`, `run.py`, `plot.py`: mã đo, tổng hợp và vẽ biểu đồ.
- `data/2048_100/dataset.json`, `manifest.csv`: mô tả bộ dữ liệu và nguồn ảnh, được giữ trong Git.
- `data/2048_100/images.f32`: cache đầu vào 100 ảnh (khoảng 1,56 GiB), giữ local và bị `.gitignore` loại khỏi Git.
- `data/README.md`: mô tả các cache và nguồn dữ liệu.
- `results/batch_v5_20261119_173042_808370/`, `batch_v5_20261119_174236_515234/`, `batch_v5_20261119_174444_749662/`: ba lượt chính thức. Chỉ CSV và JSON được đưa vào Git; executable bị loại.
- `batch_report_v5/`: bảng `batch_summary_3runs.csv`, `batch_by_run.csv` và hai biểu đồ cuối.

Các ảnh gốc không được lưu trong repository. 20 ảnh đầu lấy từ `../data/raw/`; 80 ảnh bổ sung cần đặt local trong `data/images_2k/`, hoặc chỉ định bằng biến môi trường `IMAGE_FILTER_SCALING_EXTRA_DATA`. Nếu cache `.f32` đã tồn tại và checksum hợp lệ, V5 có thể dùng lại cache cùng manifest mà không cần đọc lại ảnh gốc. Nếu thiếu cache, cần cung cấp đủ ảnh nguồn để tạo lại.

## Chạy lại

Cần Python 3.11+, GCC có OpenMP, NumPy, Pillow và Matplotlib 3.7 trở lên. Chạy từ thư mục gốc repository:

```powershell
python -m pip install numpy Pillow "matplotlib>=3.7"
python .\image_filter_scaling\run.py
```

Để chọn thư mục 80 ảnh bổ sung:

```powershell
$env:IMAGE_FILTER_SCALING_EXTRA_DATA = '<thu-muc-chua-80-anh>'
python .\image_filter_scaling\run.py
```

Mỗi lượt mới tạo thư mục `image_filter_scaling/results/batch_v5_*` với `run_config.json`, `manifest.csv`, `timer_info.csv`, `summary.csv`, `timings.csv` và `errors_per_image.csv`. Tệp `.exe` được tạo để chạy benchmark nhưng không lưu trong Git.

Để vẽ lại kết quả từ đúng ba lượt chính thức mà không chạy benchmark:

```powershell
python .\image_filter_scaling\plot.py `
  .\image_filter_scaling\results\batch_v5_20261119_173042_808370 `
  .\image_filter_scaling\results\batch_v5_20261119_174236_515234 `
  .\image_filter_scaling\results\batch_v5_20261119_174444_749662 `
  --out .\image_filter_scaling\batch_report_v5
```

Các lượt V1–V4, smoke test, tệp ZIP và benchmark per-image cũ không thuộc repository này; chúng không phải nguồn số liệu V5 cuối cùng.
