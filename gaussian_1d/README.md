# Gaussian Blur tách 1D

Trong terminal Ubuntu, chạy `bash test_one.sh 1024 image_1.bin` để build, đo và tạo ảnh trước/sau. Chạy `bash run_full.sh` để đo cả bộ dữ liệu và xuất `summary.csv`. [Hướng dẫn từ thư mục dự án](../README.md)

Kernel Gaussian 2D có thể biểu diễn bằng tích của hai kernel 1D. Bản này dùng cùng trọng số đã chuẩn hóa cho lượt ngang và lượt dọc, giảm số hạng nhân–cộng mỗi pixel từ `k²` xuống `2k`. Cả hai lượt dùng replicate padding. Bộ đệm trung gian lưu `double` và chương trình làm tròn một lần khi ghi pixel cuối cùng.

Cấu hình mặc định dùng kernel **19 × 19, σ = 3**, với 19 hệ số cho mỗi lượt. `run_full.sh` lưu mỗi lượt chạy vào một thư mục mới dưới `results/full/` và in đường dẫn khi hoàn tất.

Bản OpenMP chia các hàng bằng `schedule(static)` trong cùng một vùng song song. Hàng rào đồng bộ sau lượt ngang bảo đảm lượt dọc đọc được cả các hàng trung gian do luồng khác xử lý. Benchmark đo Sequential và OpenMP với **2, 4, 8, 16, 20 luồng**. Chương trình cấp phát bộ đệm ngoài khoảng đo và tính thời gian cho cả hai lượt lọc. Mỗi cấu hình chạy một lần warm-up rồi đo 10 lần liên tiếp để lấy median. Cấu hình bắt đầu thay đổi sau mỗi ảnh. `run_config.txt` ghi số CPU logic mà OpenMP thấy lúc chạy.

Thư mục này chứa mã lọc 1D, trình đọc ảnh, chương trình benchmark và các script xuất kết quả.

## Chạy trong Ubuntu WSL2

Mở terminal Ubuntu trong VS Code rồi chạy các lệnh sau để build và kiểm tra cả Sequential và OpenMP.

```bash
cd gaussian_1d
bash build_gcc.sh
python3 test_gaussian.py build/gaussian_seq build/gaussian_omp --threads 2 4 8 16 20
```

Lệnh sau đo `image_1` ở cả ba kích thước với kernel **19 × 19, σ = 3**. Nó xuất ảnh PNG nếu Python đã có Pillow. Chọn tên thư mục kết quả mới cho mỗi lần chạy.

```bash
mkdir -p results
out="results/demo_k19_s3_$(date +%Y%m%d_%H%M%S)"
./build/gaussian_bench ../data/processed_raw_names "$out" 19 3 image_1.bin &&
python3 export_png.py "$out"
```

Ảnh nằm trong `$out/images/<kích thước>/<chế độ>/`. Ví dụ, ảnh 1024 × 1024 của bản OpenMP 8 luồng nằm ở `$out/images/1024/omp8/image_1.png`.

Chạy lệnh sau để benchmark đủ 20 ảnh ở mỗi kích thước với cấu hình mặc định **19 × 19, σ = 3**.

```bash
mkdir -p results
out="results/full_k19_s3_$(date +%Y%m%d_%H%M%S)"
./build/gaussian_bench ../data/processed_raw_names "$out"
```

`metrics.csv` chứa median, speedup so với Sequential 1D, hiệu suất song song, MaxAE và RMSE. Khi so sánh 1D với 2D, dùng thời gian chạy ở cùng kích thước ảnh, kernel, sigma và số luồng. Để xuất PNG cho lượt này, chạy `python3 export_png.py "$out"`. Để tạo biểu đồ, chạy `python3 plot_results.py "$out"` trong môi trường đã có Matplotlib.
