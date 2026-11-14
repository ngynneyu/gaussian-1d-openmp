# Gaussian Blur 1D với C và OpenMP

Chương trình lọc ảnh xám theo chiều ngang rồi chiều dọc, mặc định kernel **19 × 19, σ = 3** và xử lý biên bằng replicate padding. Benchmark so sánh Sequential với OpenMP **2, 4, 8, 16 và 20 luồng**. [Chi tiết triển khai](gaussian_1d/README.md)

## Chuẩn bị dữ liệu

Mở terminal Ubuntu trong VS Code tại thư mục gốc của repo. Cần GCC có OpenMP, Python 3 và Pillow. Bước tiền xử lý cần thêm NumPy. Biểu đồ cần Matplotlib. Nếu Ubuntu chưa có trình biên dịch hoặc `venv`, cài bằng `sudo apt update && sudo apt install build-essential python3-venv`.

Repo có sẵn 20 ảnh gốc trong [`data/raw/`](data/raw/). Script tiền xử lý tạo ảnh xám ở các kích thước 512 × 512, 1024 × 1024 và 2048 × 2048.

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install numpy pillow matplotlib
python3 scripts/preprocess.py --input data/raw --output data/processed_raw_names --preview
```

Nếu đã có các thư mục 512, 1024 và 2048 trong `data/processed_raw_names`, có thể chạy thẳng phần demo. Script cũng nhận `data/processed` nếu dự án dùng cấu trúc dữ liệu của repo nhóm. Tệp đầu ra giữ tên ảnh gốc, ví dụ `image_2.jpg` thành `image_2.png`.

Khi tạo lại dữ liệu, truyền `--output` với một thư mục mới, rồi dùng `DATASET_DIR` để chọn bộ dữ liệu đó khi chạy benchmark. Script công bố thư mục đầu ra sau khi đã ghi xong toàn bộ ảnh và `metadata.csv`.

## Chạy một ảnh

```bash
bash gaussian_1d/test_one.sh 1024 image_1.bin
```

Script tự biên dịch, đo Sequential và OpenMP trên ảnh đã chọn rồi xuất PNG. Mở `gaussian_1d/results/single/1024/run_…/previews/1024/image_1_preview.png` để xem ảnh xám đầu vào và ảnh sau lọc đặt cạnh nhau ở cùng độ phân giải. Đường dẫn chính xác được in khi script chạy xong.

## Benchmark toàn bộ

```bash
bash gaussian_1d/run_full.sh
```

Mỗi lượt có một thư mục mới dưới `results/full/` của bản tương ứng. Có thể truyền đường dẫn bằng `bash gaussian_1d/run_full.sh <dataset_dir> <results_dir>` hoặc đặt `DATASET_DIR` và `RESULTS_DIR` trước lệnh. Mặc định script xuất PGM, PNG và ảnh đối chiếu. `EXPORT_IMAGES=0` chỉ bỏ ảnh đầu vào và đầu ra. Nếu có Matplotlib, script vẫn vẽ ba biểu đồ từ số liệu đo.

| Tệp hoặc thư mục | Nội dung |
| --- | --- |
| `timings_raw.csv` | Thời gian của từng lượt đo. |
| `metrics.csv` | Median, speedup, hiệu suất và sai số cho từng ảnh. |
| `summary.csv` | Median qua các ảnh cùng kích thước và cấu hình, kèm MaxAE và RMSE lớn nhất. |
| `run_config.txt` | Kernel, sigma, kích thước, số luồng và môi trường OpenMP. |
| `images/`, `previews/` | Ảnh đã lọc theo số luồng và ảnh đối chiếu trước/sau. |

Mỗi cấu hình chạy một lần warm-up rồi đo 10 lần liên tiếp bằng `omp_get_wtime()`. Cấu hình bắt đầu thay đổi sau mỗi ảnh. Việc đọc ảnh, ghi tệp và tạo preview diễn ra ngoài phạm vi tính giờ. Dữ liệu tiền xử lý, thư mục build và kết quả nằm trong `.gitignore`.
