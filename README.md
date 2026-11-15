# Parallel Image Filtering in C with OpenMP

## Liên kết GitHub

| Mục | Repository |
| --- | --- |
| Nhóm | [ngynneyu/parallel-image-filtering-openmp](https://github.com/ngynneyu/parallel-image-filtering-openmp) |
| Người 2 · Sobel | [nhungnt795/Parallel-Image-Filtering, nhánh sobel-openmp](https://github.com/nhungnt795/Parallel-Image-Filtering/tree/sobel-openmp) |
| Người 3 · Laplacian và dữ liệu | [vietquang8071/ttss](https://github.com/vietquang8071/ttss) |

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
├── benchmark/            Báo cáo và dữ liệu của lượt đo đã lưu
├── windows_.../          Kết quả từ PowerShell
└── wsl_.../              Kết quả từ Ubuntu hoặc WSL2
run_all.ps1               Chạy toàn bộ trên Windows
run_all.sh                Chạy toàn bộ trên Ubuntu hoặc WSL2
```

- Sobel dùng kernel 3×3 và tính biên độ gradient `sqrt(Gx² + Gy²)` kiểu `float32`. Viền ngoài một pixel bằng 0. Bản OpenMP chia hàng bằng `schedule(static)`. Mã Sobel nằm trong `sobel_seq.c`, `sobel_omp.c` và `sobel.h`.

- Gaussian tách 1D mặc định dùng kernel `13×13`, `σ = 2`, replicate padding và đầu ra `uint8`. [Mã Gaussian của Người 1](https://github.com/ngynneyu/parallel-image-filtering-openmp/tree/2f639af25dd36171978e106c9f467e8a4263101d)
- Laplacian 4 lân cận dùng zero padding và đầu ra `float32` có dấu. [Mã Laplacian của Người 3](https://github.com/vietquang8071/ttss/tree/91e2ed2ac05a56f2dc8bc258f60028242777b977)

Các tệp nguồn Gaussian và Laplacian từ hai commit trên nằm trực tiếp trong `src/gaussian/` và `src/laplacian/`. Danh sách luồng OpenMP trong hai bộ mã đã được đồng bộ với benchmark chung, và cấu hình Gaussian mặc định được đặt trong `src/gaussian/gaussian.h`. `benchmark/run.py` biên dịch các tệp có sẵn trong repo và lưu bản build vào `.bench-cache/build/`.

## Chạy lại

Cần Python 3 và GCC hỗ trợ C11 và OpenMP. Nếu đã có đủ `image_01.bin` đến `image_20.bin` ở mỗi thư mục kích thước trong `data/processed/`, chạy script tương ứng từ thư mục gốc. Mỗi lần chạy tự biên dịch, đo cả ba bộ lọc và tổng hợp CSV trong một thư mục kết quả mới.

### Chạy trên Windows và xuất PNG

Mở PowerShell trong thư mục gốc của repo. Cài Pillow một lần vào bản Python dùng để chạy benchmark.

```powershell
python -m pip install "Pillow>=10.3"
```

Chạy benchmark trực tiếp trên Windows và xuất toàn bộ ảnh PNG.

```powershell
.\run_all.ps1 -Images
```

Khi chạy xong, terminal in đường dẫn `results\windows_...`. Mở `summary.csv` để xem bảng thời gian và `images\` để xem ảnh. Ví dụ, ảnh Gaussian 512×512 với OpenMP 4 luồng nằm ở `images\Gaussian\512\omp4\image_2.png`. Dùng `.\run_all.ps1` khi chỉ cần chạy benchmark và tổng hợp CSV.

Nếu GCC chưa có trong `PATH`, truyền đường dẫn đến `gcc.exe`. Dùng `-Python "C:\duong-dan\python.exe"` nếu cần chỉ định đúng bản Python đã cài Pillow.

```powershell
.\run_all.ps1 -Images -Compiler "C:\duong-dan\gcc.exe"
```

### Chạy trên Ubuntu hoặc WSL2

Mở terminal Ubuntu tại thư mục chứa `run_all.sh`. Dùng `bash run_all.sh` để đo và tổng hợp. Để xuất PNG cùng lượt đo, chạy lệnh sau.

```bash
bash run_all.sh --images
```

Kết quả nằm trong `results/wsl_...`. Có thể chỉ định GCC bằng biến môi trường `CC`. Chế độ xuất PNG cần Pillow từ 10.3. Dùng gói `python3-pil` nếu phiên bản đáp ứng yêu cầu, hoặc chạy `python3 -m pip install "Pillow>=10.3"` trong môi trường ảo.

### Đọc kết quả của lượt chạy

Benchmark in `Gaussian kernel: 13x13, sigma: 2` khi bắt đầu. Thư mục của mỗi lượt chứa `summary.csv`, `metrics_per_image.csv`, `timings.csv`, `run_config.json` và ảnh mẫu `.bin`. Báo cáo Excel được xuất ở bước riêng.

Khi bật xuất PNG, mỗi lượt chạy thành công lưu 1.080 ảnh tại `images/<bộ lọc>/<kích thước>/<seq hoặc ompN>/`. Tên PNG lấy từ tên ảnh gốc trong `data/processed/metadata.csv` khi ánh xạ duy nhất. Nếu thiếu ánh xạ, ảnh dùng mã `image_XX` của bộ dữ liệu. Ảnh PNG ở thang xám. Gaussian dùng trực tiếp dữ liệu 8 bit. Sobel và Laplacian lấy độ lớn, chia cho 4 và đưa về khoảng hiển thị 0–255.

Ảnh của từng mốc luồng được xuất sau khi OpenMP đã khớp từng pixel với bản tuần tự. Việc lưu tệp và mã hóa PNG nằm ngoài khoảng đo hàm lọc.

### Chuẩn bị dữ liệu

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
- Với mỗi ảnh và mỗi cấu hình, warm-up một lần rồi đo 10 lần, lấy median. Các cấu hình gồm bản tuần tự và OpenMP với 2, 4, 8, 12, 16 luồng. Thứ tự các mốc OpenMP xoay vòng theo ảnh.
- Benchmark C11 dùng `QueryPerformanceCounter()` trên Windows và `omp_get_wtime()` trên các hệ điều hành khác. Thời gian đo gồm lời gọi hàm lọc và chi phí vùng OpenMP. Đọc ảnh, cấp phát, khởi tạo kernel, kiểm tra sai số và lưu kết quả nằm ngoài khoảng đo.
- Bảng tổng hợp lấy median của 20 median theo ảnh cho từng cấu hình. `Speedup = T_seq / T_omp`, `Efficiency (%) = 100 × Speedup / P`. Max Absolute Error và RMSE được tính theo từng pixel, rồi lấy giá trị lớn nhất trên 20 ảnh ở mỗi dòng tổng hợp.
- Lượt đo lưu tại `results/benchmark/` chạy trên Ubuntu qua WSL2, với AMD Ryzen AI 7 H 450, 16 CPU logic và GCC 15.2.0. Lượt đo đó dùng Gaussian `19×19` với `σ = 3`. `run_config.json` lưu cờ biên dịch, biến môi trường OpenMP và SHA-256 của bộ dữ liệu đã đo.

## Môi trường thực nghiệm

Hai lượt dưới đây chạy trên cùng máy dùng **AMD Ryzen AI 7 H 450** với **16 CPU logic**. Cả hai dùng cùng bộ 60 ảnh grayscale, bản tuần tự và các mốc OpenMP **2, 4, 8, 12, 16 luồng**. Gaussian dùng kernel `13×13` với `σ = 2`. Mỗi ảnh và cấu hình có một lượt warm-up và 10 lượt đo.

| Thành phần | Windows | Ubuntu WSL2 |
| --- | --- | --- |
| Hệ điều hành | Windows 11, build 26300, x64 | Ubuntu 26.04.1 LTS, kernel `6.18.33.2-microsoft-standard-WSL2`, x64 |
| Trình biên dịch | GCC 16.2.0, w64devkit | GCC 15.2.0, Ubuntu |
| Python và Pillow | Python 3.14.6, Pillow 13.0.0 | Python 3.14.4, Pillow 12.1.1 |
| Bộ đếm thời gian | `QueryPerformanceCounter()` | `omp_get_wtime()` |
| Tệp cấu hình | [run_config.json](results/windows_20261115_215609_5029443/run_config.json) | [run_config.json](results/wsl_20261115_204823_469249545/run_config.json) |
| Kết quả | [CSV và PNG trên Windows](results/windows_20261115_215609_5029443/) | [CSV và PNG trên WSL2](results/wsl_20261115_204823_469249545/) |

Cờ biên dịch chung gồm `-std=gnu11 -O3 -fopenmp -ffp-contract=off -Wall -Wextra`. Cấu hình OpenMP của hai lượt đặt `OMP_DYNAMIC=FALSE`, `OMP_THREAD_LIMIT=16`, `OMP_WAIT_POLICY=PASSIVE`, `OMP_PROC_BIND=FALSE` và `GOMP_SPINCOUNT=0`.

## Tệp kết quả tham khảo

Các liên kết dưới đây trỏ tới lượt đo WSL2 đã lưu trong `results/benchmark/`.

| Tệp | Nội dung |
| --- | --- |
| [Excel](results/benchmark/Ket%20qua%20benchmark%20OpenMP.xlsx) | Bảng metrics và 9 biểu đồ cho cả ba thuật toán |
| [summary.csv](results/benchmark/summary.csv) | 45 dòng tổng hợp cho ba thuật toán |
| [metrics_per_image.csv](results/benchmark/metrics_per_image.csv) | 900 cặp so sánh với median, speedup, efficiency và correctness |
| [timings.csv](results/benchmark/timings.csv) | 10 thời gian tuần tự và 10 thời gian OpenMP cho từng cặp ảnh/cấu hình, kèm sai số pixel |
| [run_config.json](results/benchmark/run_config.json) | Thông tin máy, trình biên dịch, cấu hình OpenMP và SHA-256 của dữ liệu |
| [samples/](results/benchmark/samples) | Đầu ra mẫu tuần tự và OpenMP 4 luồng của `image_01`, kích thước 512×512. Gaussian dùng `uint8`, Sobel và Laplacian dùng `float32` little-endian. |

## Xử lý WinError 4551

Nếu Windows báo `WinError 4551`, Application Control đã chặn tệp `.bench-cache/build/image_benchmark.exe`. Để tiếp tục chạy qua Ubuntu WSL2, mở PowerShell tại thư mục gốc của repo rồi dùng lệnh sau.

```powershell
wsl -d Ubuntu -- bash run_all.sh --images
```

Lệnh này đo trong WSL2 và lưu kết quả trong `results/wsl_.../`.
