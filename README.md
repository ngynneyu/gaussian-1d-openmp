# Parallel Image Filtering in C with OpenMP

## Cấu trúc thư mục

```text
data/
├── raw/                  20 ảnh gốc
└── processed/            Ảnh grayscale ở ba kích thước và metadata.csv
src/
├── gaussian/             Mã Gaussian và adapter cho benchmark
├── sobel/                Sobel tuần tự, OpenMP và header dùng chung
└── laplacian/            Mã Laplacian và adapter cho benchmark
scripts/
└── data/                 Script tiền xử lý và chương trình đọc ảnh .bin
benchmark/                Biên dịch, đo và tổng hợp cả ba thuật toán
results/
└── benchmark/            Excel, CSV, cấu hình lần đo và samples/
```

Sobel dùng kernel 3×3 và tính biên độ gradient `sqrt(Gx² + Gy²)` kiểu `float32`. Viền ngoài một pixel bằng 0. Bản OpenMP chia hàng bằng `schedule(static)`. Mã Sobel nằm trong `sobel_seq.c`, `sobel_omp.c` và `sobel.h`.

- Gaussian tách 1D dùng kernel `19×19`, `σ = 3`, replicate padding và đầu ra `uint8`. [Mã Gaussian của Người 1](https://github.com/ngynneyu/gaussian-1d-openmp/tree/2f639af25dd36171978e106c9f467e8a4263101d)
- Laplacian 4 lân cận dùng zero padding và đầu ra `float32` có dấu. [Mã Laplacian của Người 3](https://github.com/vietquang8071/ttss/tree/91e2ed2ac05a56f2dc8bc258f60028242777b977)

Các tệp nguồn Gaussian và Laplacian từ hai commit trên nằm trực tiếp trong `src/gaussian/` và `src/laplacian/`. `benchmark/run.py` biên dịch các tệp có sẵn trong repo và lưu bản build vào `.bench-cache/build/`.

## Chạy lại

Cần Python 3 và GCC hỗ trợ C11 và OpenMP. Chạy các lệnh từ thư mục gốc. Nếu đã có đủ `image_01.bin` đến `image_20.bin` ở mỗi thư mục kích thước trong `data/processed/`, chạy thẳng hai lệnh benchmark. Mỗi lần chạy dùng một thư mục `--out` mới.

```sh
python benchmark/run.py --out results/run-02
python benchmark/summarize.py results/run-02
```

Trên Windows có thể truyền đường dẫn tới `gcc.exe` bằng `--cc`.

Để tạo ảnh đầu vào từ `data/raw/`, cài NumPy và Pillow rồi chạy lại bước tiền xử lý. `--numbered` đánh số theo thứ tự tên tệp. `metadata.csv` ghi tên ảnh gốc tương ứng.

```sh
python -m pip install numpy Pillow
python scripts/data/preprocess.py --numbered
```

Nếu clone mới và chưa có ảnh gốc, lấy dữ liệu của Người 3 rồi chỉ định `--input`.

```sh
git clone https://github.com/vietquang8071/ttss.git dataset-source
git -C dataset-source checkout --detach 91e2ed2ac05a56f2dc8bc258f60028242777b977
python scripts/data/preprocess.py --input dataset-source/data/raw --numbered
```

Để đọc thử ảnh `.bin`, biên dịch `scripts/data/read_image.c` bằng GCC rồi chạy chương trình từ thư mục gốc. Chương trình nhận đường dẫn ảnh và kích thước cạnh, mặc định dùng `data/processed/512/image_01.bin`.

## Phương pháp đo

- Dùng cùng 20 ảnh grayscale ở mỗi kích thước 512×512, 1024×1024 và 2048×2048. Mỗi ảnh gốc có hai chiều tối thiểu 2048 px.
- Với mỗi ảnh và mỗi cấu hình, warm-up một lần rồi đo 10 lần, lấy median. Các cấu hình gồm bản tuần tự và OpenMP với 4, 8, 12, 16, 20 luồng. Thứ tự các mốc OpenMP xoay vòng theo ảnh.
- Benchmark C11 dùng `QueryPerformanceCounter()` trên Windows và `omp_get_wtime()` trên các hệ điều hành khác. Thời gian đo gồm lời gọi hàm lọc và chi phí vùng OpenMP. Đọc ảnh, cấp phát, khởi tạo kernel, kiểm tra sai số và lưu kết quả nằm ngoài khoảng đo.
- Bảng tổng hợp lấy median của 20 median theo ảnh cho từng cấu hình. `Speedup = T_seq / T_omp`, `Efficiency (%) = 100 × Speedup / P`. Max Absolute Error và RMSE được tính theo từng pixel, rồi lấy giá trị lớn nhất trên 20 ảnh ở mỗi dòng tổng hợp.
- Lượt đo đã bàn giao chạy trên Intel Core i7-1255U với Windows 11, GCC 16.2.0 và 12 CPU logic. Hai mốc 16 và 20 luồng vượt số CPU logic. `run_config.json` lưu cờ biên dịch, biến môi trường OpenMP và SHA-256 của bộ dữ liệu đã đo.

## Tệp kết quả

| Tệp | Nội dung |
| --- | --- |
| [Excel](results/benchmark/Ket%20qua%20benchmark%20OpenMP.xlsx) | Bảng metrics và 9 biểu đồ cho cả ba thuật toán |
| [summary.csv](results/benchmark/summary.csv) | 45 dòng tổng hợp cho ba thuật toán |
| [metrics_per_image.csv](results/benchmark/metrics_per_image.csv) | 900 cặp so sánh với median, speedup, efficiency và correctness |
| [timings.csv](results/benchmark/timings.csv) | 10 thời gian tuần tự và 10 thời gian OpenMP cho từng cặp ảnh/cấu hình, kèm sai số pixel |
| [run_config.json](results/benchmark/run_config.json) | Thông tin máy, trình biên dịch, cấu hình OpenMP và SHA-256 của dữ liệu |
| [samples/](results/benchmark/samples) | Đầu ra mẫu tuần tự và OpenMP 4 luồng của `image_01`, kích thước 512×512. Gaussian dùng `uint8`, Sobel và Laplacian dùng `float32` little-endian. |
