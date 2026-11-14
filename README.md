# Gaussian Blur 1D với C và OpenMP

Chương trình lọc ảnh xám theo chiều ngang rồi chiều dọc, dùng kernel mặc định **19 × 19, σ = 3** và xử lý biên bằng replicate padding. Benchmark so sánh Sequential với OpenMP **2, 4, 8, 16 và 20 luồng**. [Chi tiết triển khai](gaussian_1d/README.md)

Repo có **20 ảnh JPG gốc** trong [`data/raw/`](data/raw/). Bước tiền xử lý tạo **60 tệp `.bin`** trong `data/processed_raw_names/`, gồm 20 ảnh cho mỗi kích thước **512 × 512**, **1024 × 1024** và **2048 × 2048**. Ubuntu và Windows dùng chung bộ `.bin` này. Nếu đã có đủ 60 tệp, bỏ qua bước tiền xử lý.

### Cấu trúc thư mục

```text
gaussian-1d-openmp/
├── data/
│   ├── raw/                    # Ảnh JPG gốc
│   └── processed_raw_names/    # Tệp .bin tạo sau khi tiền xử lý
├── scripts/
│   └── preprocess.py          # Chuyển ảnh gốc sang ảnh xám .bin
└── gaussian_1d/
    ├── src/                   # Mã C11 cho thuật toán, đọc/ghi ảnh và benchmark
    ├── scripts/               # Điều phối benchmark và xử lý CSV, PNG
    ├── tests/                 # Kiểm tra kết quả Sequential và OpenMP
    ├── build_gcc.sh           # Biên dịch trên Ubuntu
    ├── build_gcc.ps1          # Biên dịch trên Windows
    ├── test_one.sh            # Chạy thử một ảnh trên Ubuntu
    ├── run_full.sh            # Chạy toàn bộ trên Ubuntu
    ├── run_full.ps1           # Chạy toàn bộ trên Windows
    ├── build/                 # Chương trình tạo sau khi biên dịch
    └── results/               # Kết quả tạo khi chạy benchmark
        ├── single/            # Lượt thử một ảnh trên Ubuntu
        ├── full/              # Lượt chạy toàn bộ trên Ubuntu
        └── windows_full/      # Lượt chạy toàn bộ trên Windows
```

## 1. Chạy trên Ubuntu (WSL)

Mở terminal **Ubuntu** trong VS Code tại thư mục gốc của repo.

### Chuẩn bị lần đầu

```bash
sudo apt update
sudo apt install -y build-essential python3-venv
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install numpy pillow matplotlib
```

Nếu chưa có bộ `.bin`, chạy một lần.

```bash
python3 scripts/preprocess.py --preview
```

### Chạy benchmark

Mỗi khi mở terminal mới, chạy `source .venv/bin/activate`. Để thử một ảnh và xuất PNG, dùng:

```bash
bash gaussian_1d/test_one.sh 1024 image_1.bin
```

Để chạy toàn bộ 60 ảnh đầu vào, dùng:

```bash
bash gaussian_1d/run_full.sh
```

Script tự biên dịch và in đường dẫn kết quả khi chạy xong. Lượt thử nằm trong `gaussian_1d/results/single/1024/run_…/`. Lượt chạy toàn bộ nằm trong `gaussian_1d/results/full/run_…/` và có thêm biểu đồ nếu đã cài Matplotlib. Dùng `EXPORT_IMAGES=0 bash gaussian_1d/run_full.sh` để bỏ bước xuất ảnh.

## 2. Chạy trên Windows (PowerShell)

Mở terminal **PowerShell** trong VS Code tại thư mục gốc của repo.

### Chuẩn bị lần đầu

Cần Python 3 bản Windows. Nếu đã có `.venv-windows` với NumPy và Pillow, dùng môi trường đó. Nếu chưa có, chạy:

```powershell
python -m venv .venv-windows
.\.venv-windows\Scripts\python.exe -m pip install numpy pillow
```

Nếu chưa có bộ `.bin`, chạy một lần.

```powershell
.\.venv-windows\Scripts\python.exe .\scripts\preprocess.py --preview
```

### Chạy benchmark

```powershell
.\gaussian_1d\run_full.ps1
```

Script dùng `gaussian_bench.exe` có sẵn. Nếu cần build, script gọi `build_gcc.ps1` và tìm GCC hỗ trợ OpenMP trong `PATH`. Nếu GCC nằm ngoài `PATH` hoặc bạn muốn biên dịch lại, truyền `-Compiler` với đường dẫn đến `gcc.exe`. Dùng `.\gaussian_1d\run_full.ps1 -NoImages` khi chỉ cần CSV.

Mỗi lượt lưu vào **`gaussian_1d/results/windows_full/run_…/`** và mở thư mục đó khi hoàn tất.

Các lượt chạy trên cả hai hệ điều hành có cùng cách đọc kết quả.

| Tệp hoặc thư mục | Nội dung |
| --- | --- |
| `timings_raw.csv` | Thời gian của từng lượt đo. |
| `metrics.csv` | Median, speedup, hiệu suất và sai số cho từng ảnh. |
| `summary.csv` | Median qua các ảnh cùng kích thước và cấu hình, kèm MaxAE và RMSE lớn nhất. |
| `run_config.txt` | Kernel, sigma, kích thước, số luồng và môi trường OpenMP. |
| `mismatches.csv` | Các pixel sai khác giữa Sequential và OpenMP. Tệp chỉ có tiêu đề khi kết quả khớp. |
| `images/`, `previews/` | Ảnh đã lọc theo số luồng và ảnh đối chiếu trước/sau. |

Mỗi cấu hình chạy một lần warm-up rồi đo 10 lần liên tiếp. Ubuntu WSL dùng `omp_get_wtime()`, Windows dùng `QueryPerformanceCounter`. Cấu hình bắt đầu thay đổi sau mỗi ảnh. Việc đọc ảnh, ghi tệp và tạo preview diễn ra ngoài phạm vi tính giờ. Dữ liệu tiền xử lý, thư mục build và kết quả nằm trong `.gitignore`.
