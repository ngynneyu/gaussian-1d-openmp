# Parallel Image Filtering in C with OpenMP

So sánh phiên bản tuần tự và OpenMP của ba bộ lọc ảnh mức xám Gaussian, Sobel và Laplacian. Benchmark dùng `omp_get_wtime()` và xuất CSV để phân tích thời gian, hệ số tăng tốc, hiệu suất song song và sai số điểm ảnh.

## Liên kết GitHub

| Mục | Repository |
| --- | --- |
| Nhóm | [ngynneyu/parallel-image-filtering-openmp](https://github.com/ngynneyu/parallel-image-filtering-openmp) |
| Người 2 · Sobel | [nhungnt795/Parallel-Image-Filtering, commit 1bbc4f0](https://github.com/nhungnt795/Parallel-Image-Filtering/tree/1bbc4f05f057a7dbbb98dfe18840e43ee4b67178) |
| Người 3 · Laplacian và dữ liệu | [vietquang8071/ttss, commit 91e2ed2](https://github.com/vietquang8071/ttss/tree/91e2ed2ac05a56f2dc8bc258f60028242777b977) |

## Cấu trúc thư mục

```text
data/
├── raw/                  20 ảnh gốc
├── processed/            Ảnh grayscale dạng .bin và metadata.csv
└── processed_png/        60 ảnh đầu vào dạng PNG
src/
├── gaussian/             Kernel, bản tuần tự và OpenMP của Gaussian
├── sobel/                Sobel tuần tự, OpenMP và header dùng chung
└── laplacian/            Mã Laplacian và adapter cho benchmark
scripts/
└── data/                 Script tiền xử lý và chương trình đọc ảnh .bin
benchmark/                Biên dịch, đo, tổng hợp và vẽ biểu đồ
results/
├── windows_.../          Kết quả từ PowerShell
├── wsl_.../              Kết quả từ Ubuntu hoặc WSL2
└── charts/               Biểu đồ PNG tạo từ kết quả đã đo
run_all.ps1               Chạy toàn bộ trên Windows
run_all.sh                Chạy toàn bộ trên Ubuntu hoặc WSL2
```

- Sobel dùng kernel 3×3 và tính biên độ gradient `sqrt(Gx² + Gy²)` kiểu `float32`. Viền ngoài một pixel bằng 0. Bản OpenMP chia hàng bằng `schedule(static)`. Mã Sobel nằm trong `sobel_seq.c`, `sobel_omp.c` và `sobel.h`.

- Gaussian tách 1D mặc định dùng kernel `13×13`, `σ = 2`, replicate padding và đầu ra `uint8`. `gaussian.c` chứa kernel, bộ đệm và bản tuần tự, `gaussian_omp.c` chứa bản song song, còn `gaussian.h` khai báo giao diện.
- Laplacian 4 lân cận dùng zero padding và đầu ra `float32` có dấu.

Gaussian được duy trì trong `src/gaussian/` của repo nhóm. Benchmark ghi các commit nguồn của Sobel và Laplacian vào mục `sources` trong `run_config.json`. `benchmark/run.py` biên dịch mã có sẵn trong repo và lưu bản build vào `.bench-cache/build/`.

## Chạy lại

Cần Python 3 và GCC hỗ trợ C11 và OpenMP. Repo có sẵn 60 ảnh đầu vào `.bin` trong [data/processed](data/processed), gồm 20 ảnh ở mỗi kích thước 512×512, 1024×1024 và 2048×2048. Chạy script tương ứng từ thư mục gốc sau khi clone. Mỗi lần chạy tự biên dịch, đo cả ba bộ lọc và tổng hợp CSV trong một thư mục kết quả mới.

Có thể xem các ảnh đầu vào ở dạng PNG trong [data/processed_png](data/processed_png).

### Chạy trên Windows và xuất PNG

Mở PowerShell trong thư mục gốc của repo. Cài Pillow một lần vào bản Python dùng để chạy benchmark.

```powershell
python -m pip install "Pillow>=10.3"
```

Chạy benchmark và xuất ảnh sau lọc dạng PNG. Tùy chọn `-ExecutionPolicy RemoteSigned` áp dụng cho phiên PowerShell chạy lệnh này.

```powershell
powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File .\run_all.ps1 -Images
```

Khi chạy xong, dòng `Results:` in đường dẫn thư mục `results\windows_...`. Mở `summary.csv` để xem bảng thời gian và `images\` để xem ảnh sau lọc. Bỏ `-Images` nếu chỉ cần số liệu.

Nếu GCC chưa có trong `PATH`, truyền đường dẫn đến `gcc.exe`. Dùng `-Python "C:\duong-dan\python.exe"` nếu cần chỉ định đúng bản Python đã cài Pillow.

```powershell
powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File .\run_all.ps1 -Images -Compiler "C:\duong-dan\gcc.exe"
```

### Chạy trên Ubuntu hoặc WSL2

Mở terminal Ubuntu tại thư mục chứa `run_all.sh`. Dùng `bash run_all.sh` để đo và tổng hợp. Để xuất PNG cùng lượt đo, chạy lệnh sau.

```bash
bash run_all.sh --images
```

Kết quả nằm trong `results/wsl_...`. Có thể chỉ định GCC bằng biến môi trường `CC`. Chế độ xuất PNG cần Pillow từ 10.3. Dùng gói `python3-pil` nếu phiên bản đáp ứng yêu cầu, hoặc chạy `python3 -m pip install "Pillow>=10.3"` trong môi trường ảo.

### Đọc kết quả của lượt chạy

Benchmark in `Gaussian kernel: 13x13, sigma: 2` khi bắt đầu. Mỗi lượt tạo `run_config.json`, ba tệp CSV và ảnh mẫu `.bin` trong `samples/`.

Khi bật xuất PNG, mỗi lượt chạy thành công lưu 1.080 ảnh tại `images/<bộ lọc>/<kích thước>/<seq hoặc ompN>/`. Tên PNG lấy từ tên ảnh gốc trong `data/processed/metadata.csv` khi ánh xạ duy nhất. Nếu thiếu ánh xạ, ảnh dùng mã `image_XX` của bộ dữ liệu. Ảnh PNG ở thang xám. Gaussian dùng trực tiếp dữ liệu 8 bit. Sobel và Laplacian lấy độ lớn, chia cho 4 và đưa về khoảng hiển thị 0–255.

Ảnh của từng mốc luồng được xuất sau khi OpenMP đã khớp từng pixel với bản tuần tự. Việc lưu tệp và mã hóa PNG nằm ngoài khoảng đo hàm lọc.

### Vẽ lại biểu đồ từ kết quả đã đo

Cài Matplotlib rồi chạy từ thư mục gốc của repo:

```sh
python -m pip install matplotlib
python benchmark/plot.py results/windows_20261116_152359_7285082 --out results/charts
```

Thay thư mục đầu vào bằng đường dẫn ở dòng `Results:` để vẽ cho một lượt đo khác. Script đọc `summary.csv` và `run_config.json`, rồi ghi 9 biểu đồ PNG vào `results/charts/<tên lượt đo>/`. Mỗi bộ lọc có biểu đồ thời gian, hệ số tăng tốc và hiệu suất song song. Mỗi đường tương ứng một kích thước ảnh. Mốc 1 trên biểu đồ thời gian biểu diễn bản tuần tự. Các lượt truyền trong cùng một lệnh dùng chung thang trục dọc cho từng bộ lọc và chỉ số.

### Tạo lại dữ liệu đầu vào

Để tạo một bản dữ liệu riêng từ `data/raw/`, cài NumPy và Pillow rồi chạy bước tiền xử lý vào thư mục mới. `--numbered` đánh số theo thứ tự tên tệp. `metadata.csv` ghi tên ảnh gốc tương ứng.

```sh
python -m pip install numpy Pillow
python scripts/data/preprocess.py --numbered --output data/processed_new
```

Dùng `--input` để chọn thư mục ảnh gốc khác. Khi chạy `benchmark/run.py` trực tiếp, dùng `--dataset data/processed_new` để chọn bộ dữ liệu vừa tạo.

Để đọc thử ảnh `.bin`, biên dịch `scripts/data/read_image.c` bằng GCC rồi chạy chương trình từ thư mục gốc. Chương trình nhận đường dẫn ảnh và kích thước cạnh, mặc định dùng `data/processed/512/image_01.bin`.

## Phương pháp đo

- Dùng cùng 20 ảnh grayscale ở mỗi kích thước 512×512, 1024×1024 và 2048×2048. Mỗi ảnh gốc có hai chiều tối thiểu 2048 px.
- Với mỗi ảnh và mỗi cấu hình, warm-up một lần rồi lấy 10 mẫu thời gian để tính median. Các cấu hình gồm bản tuần tự và OpenMP với 2, 4, 8, 12, 16 luồng. Thứ tự các mốc OpenMP xoay vòng theo ảnh.
- Benchmark C11 dùng `omp_get_wtime()` để đo cả bản tuần tự và bản OpenMP trên Windows và WSL2. Thời gian được lưu bằng mili giây. Khoảng đo gồm lời gọi hàm lọc và chi phí vùng OpenMP. Đọc ảnh, cấp phát, khởi tạo kernel, kiểm tra sai số và lưu kết quả nằm ngoài khoảng đo.
- Nếu một lời gọi quá ngắn so với độ phân giải của bộ đếm, chương trình gộp nhiều lời gọi để mỗi mẫu kéo dài khoảng 10 nhịp đồng hồ trở lên. Thời gian trong `timings.csv` được quy về mili giây trên một lần lọc.
- Bảng tổng hợp lấy median của 20 median theo ảnh cho từng cấu hình. `Speedup = T_seq / T_omp`, `Efficiency (%) = 100 × Speedup / P`. Max Absolute Error và RMSE được tính theo từng pixel, rồi lấy giá trị lớn nhất trên 20 ảnh ở mỗi dòng tổng hợp.

## Kết quả benchmark Windows

Lượt đo ngày **16/11/2026** chạy trên Windows 11 x64 (build 26300), CPU **AMD Ryzen AI 7 H 450** với 16 bộ xử lý logic và GCC 16.2.0 (w64devkit). Bộ đếm là `omp_get_wtime()` với cách lấy mẫu theo nhóm lời gọi mô tả ở trên.

Với Gaussian, bảng dưới chọn cấu hình có trung vị thời gian OpenMP thấp nhất tại mỗi kích thước trong lượt đo này.

| Kích thước ảnh | Số luồng | Tuần tự (ms) | OpenMP (ms) | Hệ số tăng tốc |
| --- | ---: | ---: | ---: | ---: |
| 512×512 | 16 | 5,500 | 1,219 | 4,51× |
| 1024×1024 | 12 | 22,750 | 5,000 | 4,55× |
| 2048×2048 | 16 | 109,000 | 18,250 | 5,97× |

Tất cả 900 cặp so sánh theo ảnh và số luồng của ba bộ lọc đều khớp từng điểm ảnh với bản tuần tự.

| Tệp | Nội dung |
| --- | --- |
| [run_config.json](results/windows_20261116_152359_7285082/run_config.json) | Môi trường, cờ biên dịch, bộ đếm và mã băm dữ liệu. |
| [summary.csv](results/windows_20261116_152359_7285082/summary.csv) | 45 dòng tổng hợp thời gian, hệ số tăng tốc, hiệu suất và sai số. |
| [metrics_per_image.csv](results/windows_20261116_152359_7285082/metrics_per_image.csv) | 900 dòng kết quả theo từng ảnh và cấu hình. |
| [timings.csv](results/windows_20261116_152359_7285082/timings.csv) | Các mẫu thời gian và sai số điểm ảnh dùng để tổng hợp. |
| [Ảnh sau lọc](results/windows_20261116_152359_7285082/images/) | 1.080 ảnh PNG theo bộ lọc, kích thước và số luồng. |
| [Ảnh mẫu .bin](results/windows_20261116_152359_7285082/samples/) | Đầu ra tuần tự và OpenMP 4 luồng của mỗi bộ lọc. |
| [Biểu đồ Windows](results/charts/windows_20261116_152359_7285082/) | 9 biểu đồ về thời gian, hệ số tăng tốc và hiệu suất. |

[Lượt đo WSL2 ngày 15/11](results/wsl_20261115_204823_469249545/summary.csv) được lưu riêng, với mỗi mẫu đo một lời gọi hàm lọc. [Xem biểu đồ WSL2](results/charts/wsl_20261115_204823_469249545/).

## Xử lý WinError 4551

Nếu Windows báo `WinError 4551`, Application Control đã chặn tệp `.bench-cache/build/image_benchmark.exe`. Để tiếp tục chạy qua Ubuntu WSL2, mở PowerShell tại thư mục gốc của repo rồi dùng lệnh sau.

```powershell
wsl -d Ubuntu -- bash run_all.sh --images
```

Lệnh này đo trong WSL2 và lưu kết quả trong `results/wsl_.../`.
