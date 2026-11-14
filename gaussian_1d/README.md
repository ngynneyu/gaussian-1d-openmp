# Gaussian Blur tách 1D

Các lệnh dưới đây chạy từ thư mục gốc của repo sau khi đã chuẩn bị công cụ và dữ liệu. [Hướng dẫn cài đặt](../README.md)

Kernel Gaussian 2D có thể biểu diễn bằng tích của hai kernel 1D. Bản này dùng cùng trọng số đã chuẩn hóa cho lượt ngang và lượt dọc, giảm số hạng nhân–cộng mỗi pixel từ `k²` xuống `2k`. Cả hai lượt dùng replicate padding. Bộ đệm trung gian lưu `double` và chương trình làm tròn một lần khi ghi pixel cuối cùng.

Cấu hình mặc định dùng kernel **19 × 19, σ = 3**, với 19 hệ số cho mỗi lượt.

Bản OpenMP chia các hàng bằng `schedule(static)` trong cùng một vùng song song. Hàng rào đồng bộ sau lượt ngang bảo đảm lượt dọc đọc được cả các hàng trung gian do luồng khác xử lý. Benchmark đo Sequential và OpenMP với **2, 4, 8, 16, 20 luồng**. Chương trình cấp phát bộ đệm ngoài khoảng đo và tính thời gian cho cả hai lượt lọc. Mỗi cấu hình chạy một lần warm-up rồi đo 10 lần liên tiếp để lấy median. Cấu hình bắt đầu thay đổi sau mỗi ảnh. `run_config.txt` ghi số CPU logic mà OpenMP thấy lúc chạy.

### Các tệp trong `gaussian_1d/`

| Tệp | Vai trò |
| --- | --- |
| `src/gaussian.c` | Tạo kernel 1D, tính hai lượt lọc theo hàng và chạy bản Sequential. |
| `src/gaussian_omp.c` | Chia hàng cho các luồng OpenMP và đồng bộ giữa lượt ngang với lượt dọc. |
| `src/gaussian.h` | Khai báo kiểu ảnh, kernel và các hàm dùng chung. |
| `src/image_io.c` | Đọc ảnh `.bin` float32, ghi ảnh `.pgm` và kiểm tra tham số. |
| `src/filter_main.c` | Điểm vào của hai chương trình lọc độc lập `gaussian_seq` và `gaussian_omp`. |
| `src/benchmark.c` | Điểm vào của `gaussian_bench`. Đo thời gian, so sánh pixel và ghi CSV. |
| `scripts/run_benchmark.sh` | Quy trình chạy và xuất kết quả dùng chung cho `test_one.sh` và `run_full.sh` trên Ubuntu. |
| `scripts/summarize_results.py` | Tổng hợp số liệu thành `summary.csv`. |
| `scripts/export_png.py` | Chuyển ảnh PGM sang PNG và tạo ảnh đối chiếu trước/sau. |
| `scripts/plot_results.py` | Đọc số liệu benchmark và vẽ biểu đồ khi có Matplotlib. |
| `tests/test_gaussian.py` | Kiểm tra đầu ra Sequential và OpenMP với các ảnh mẫu. |

Benchmark đo thời gian bằng `omp_get_wtime()` trên Ubuntu WSL và `QueryPerformanceCounter` trên Windows.

## 1. Chạy trên Ubuntu (WSL)

```bash
source .venv/bin/activate
bash gaussian_1d/test_one.sh 1024 image_1.bin
bash gaussian_1d/run_full.sh
```

Lượt chạy toàn bộ lưu vào `gaussian_1d/results/full/run_…/`. Bạn có thể truyền đường dẫn bằng `bash gaussian_1d/run_full.sh <dataset_dir> <results_dir>` hoặc đặt `DATASET_DIR` và `RESULTS_DIR`. Dùng `EXPORT_IMAGES=0` để bỏ bước xuất ảnh.

Để chạy bộ kiểm thử Sequential và OpenMP:

```bash
bash gaussian_1d/build_gcc.sh
python3 gaussian_1d/tests/test_gaussian.py gaussian_1d/build/gaussian_seq gaussian_1d/build/gaussian_omp
```

## 2. Chạy trên Windows (PowerShell)

```powershell
.\gaussian_1d\run_full.ps1
```

Lượt chạy lưu vào `gaussian_1d/results/windows_full/run_…/`. Dùng `-Compiler` để chỉ định `gcc.exe` và biên dịch lại, `-Python` để chọn trình Python khác, hoặc `-NoImages` khi chỉ cần CSV. Dùng `-DatasetDir` và `-ResultsDir` nếu muốn chọn thư mục dữ liệu hoặc kết quả khác.
