# Parallel Image Filtering in C with OpenMP

So sánh phiên bản tuần tự và OpenMP của ba bộ lọc ảnh mức xám Gaussian, Sobel và Laplacian. Benchmark dùng QueryPerformanceCounter trên Windows, `omp_get_wtime()` trên Ubuntu/WSL2 và xuất CSV để phân tích thời gian, hệ số tăng tốc, hiệu suất song song và sai số điểm ảnh.

## Trạng thái trong báo cáo

Bản báo cáo hiện tại đã trình bày phần thuật toán và kết quả thực nghiệm chủ yếu cho Gaussian. Sobel và Laplacian mới có phần cơ sở lý thuyết; phần thuật toán và đánh giá tương ứng chưa hoàn thiện. Mã nguồn, CSV và biểu đồ của cả hai bộ lọc được giữ trong repo để tiếp tục hoàn thiện, nhưng các kết quả đó chưa nên được xem là nội dung đã trình bày trong báo cáo.

## Liên kết GitHub

| Mục | Repository |
| --- | --- |
| Nhóm | [ngynneyu/parallel-image-filtering-openmp](https://github.com/ngynneyu/parallel-image-filtering-openmp) |
| Người 2 · Sobel | [nhungnt795/Parallel-Image-Filtering, commit 1bbc4f0](https://github.com/nhungnt795/Parallel-Image-Filtering/tree/1bbc4f05f057a7dbbb98dfe18840e43ee4b67178) |
| Người 3 · Laplacian và dữ liệu | [vietquang8071/ttss, commit 91e2ed2](https://github.com/vietquang8071/ttss/tree/91e2ed2ac05a56f2dc8bc258f60028242777b977) |

## Cấu trúc thư mục

```text
data/
├── README.md             Mô tả bộ dữ liệu và cách chuẩn bị local
├── SOURCES.md            Ghi chú nguồn dữ liệu và tình trạng giấy phép
├── raw/                  20 ảnh gốc (local, không push)
├── processed/            metadata.csv (push) và 60 ảnh .bin (local)
└── processed_png/        Ảnh xem trước (local, không push)
src/
├── gaussian/             Kernel, bản tuần tự và OpenMP của Gaussian
├── sobel/                Sobel tuần tự, OpenMP và header dùng chung
└── laplacian/            Mã Laplacian và adapter cho benchmark
scripts/
└── data/                 Script tiền xử lý và chương trình đọc ảnh .bin
benchmark/                Biên dịch, đo, tổng hợp và vẽ biểu đồ
results/
├── windows_20261119_152801_1982409/
├── windows_20261119_153808_5914934/
└── windows_20261119_154648_2619652/  Ba lượt chính thức
experiment_analysis/      CSV tổng hợp, kiểm chứng và 9 biểu đồ
image_filter_scaling/     Mục 3.4: benchmark V5 trên 100 ảnh
run_all.ps1               Chạy toàn bộ trên Windows
run_all.sh                Chạy toàn bộ trên Ubuntu hoặc WSL2
```

- Sobel dùng kernel 3×3 và tính biên độ gradient `sqrt(Gx² + Gy²)` kiểu `float32`. Viền ngoài một pixel bằng 0. Bản OpenMP chia hàng bằng `schedule(static)`. Mã Sobel nằm trong `sobel_seq.c`, `sobel_omp.c` và `sobel.h`.

- Gaussian tách 1D mặc định dùng kernel `13×13`, `σ = 2`, replicate padding và đầu ra `uint8`. `gaussian.c` chứa kernel, bộ đệm và bản tuần tự, `gaussian_omp.c` chứa bản song song, còn `gaussian.h` khai báo giao diện.
- Laplacian 4 lân cận dùng zero padding và đầu ra `float32` có dấu.

Gaussian được duy trì trong `src/gaussian/` của repo nhóm. Benchmark ghi các commit nguồn của Sobel và Laplacian vào mục `sources` trong `run_config.json`. `benchmark/run.py` biên dịch mã có sẵn trong repo và lưu bản build vào `.bench-cache/build/`.

## Chạy lại

Cần Python 3.11 trở lên và GCC hỗ trợ C11 và OpenMP. Các ảnh gốc và 60 tệp `.bin` không nằm trong repository; xem [hướng dẫn dữ liệu](data/README.md) để chuẩn bị chúng local trước khi chạy. `data/processed/metadata.csv` được giữ trong repository. Mỗi lần chạy tự biên dịch, đo cả ba bộ lọc và tổng hợp CSV trong thư mục kết quả mới.

Ảnh xem trước dạng PNG được lưu cục bộ trong `data/processed_png/`; thư mục này không được đưa lên GitHub.

### Chạy trên Windows và xuất PNG

Mở PowerShell trong thư mục gốc của repo. Cài Pillow một lần vào bản Python dùng để chạy benchmark.

```powershell
python -m pip install "Pillow>=10.3"
```

Để ghi lại cấu hình máy và chế độ nguồn cho báo cáo, có thể kiểm tra trước khi đo.

```powershell
Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors
powercfg /getactivescheme
```

Chạy benchmark và xuất ảnh sau lọc dạng PNG. Tùy chọn `-ExecutionPolicy RemoteSigned` áp dụng cho phiên PowerShell chạy lệnh này.

```powershell
powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File .\run_all.ps1 -Images
```

Để tạo thêm biểu đồ ngay sau khi chạy, cài Matplotlib và dùng `-Plots`.

```powershell
python -m pip install matplotlib
powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File .\run_all.ps1 -Images -Plots
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

Khi bật xuất PNG, mỗi lượt chạy thành công lưu 1.260 ảnh tại `images/<bộ lọc>/<kích thước>/<seq hoặc ompN>/`. Tên PNG lấy từ tên ảnh gốc trong `data/processed/metadata.csv` khi ánh xạ duy nhất. Nếu thiếu ánh xạ, ảnh dùng mã `image_XX` của bộ dữ liệu. Ảnh PNG ở thang xám. Gaussian dùng trực tiếp dữ liệu 8 bit. Sobel và Laplacian lấy độ lớn, chia cho 4 và đưa về khoảng hiển thị 0–255.

Ảnh của từng mốc luồng được xuất sau khi OpenMP đã khớp từng pixel với bản tuần tự. Việc lưu tệp và mã hóa PNG nằm ngoài khoảng đo hàm lọc.

### Vẽ lại biểu đồ từ kết quả đã đo

Cài Matplotlib rồi chạy từ thư mục gốc của repo:

```sh
python -m pip install matplotlib
python benchmark/plot.py results/windows_20261119_154648_2619652 --out experiment_analysis/charts/latest
```

Thay thư mục đầu vào bằng đường dẫn ở dòng `Results:` để vẽ cho lượt đo khác. Script đọc `summary.csv` và `run_config.json`; lệnh trên ghi 9 biểu đồ vào thư mục đã chỉ định. Các biểu đồ đối chiếu ba lượt chính thức được lưu trong `experiment_analysis/charts/`.

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
- Với mỗi ảnh và mỗi cấu hình, warm-up một lần rồi lấy 10 mẫu thời gian để tính median. Các cấu hình gồm bản tuần tự và OpenMP với 1, 2, 4, 8, 12, 16 luồng. Bản OpenMP 1 luồng chạy qua vùng song song và giúp đánh giá chi phí thực thi OpenMP. Thứ tự các mốc OpenMP xoay vòng theo ảnh.
- Benchmark C11 dùng `QueryPerformanceCounter()` trên Windows và `omp_get_wtime()` trên Ubuntu/WSL2 cho cả bản tuần tự và bản OpenMP. Khoảng đo gồm lời gọi hàm lọc và chi phí vùng OpenMP. Đọc ảnh, cấp phát, khởi tạo kernel, kiểm tra sai số và lưu kết quả nằm ngoài khoảng đo.
- Chương trình gộp nhiều lời gọi nếu mẫu ngắn hơn 20 ms hoặc 10 nhịp đồng hồ, tùy ngưỡng nào lớn hơn. Thời gian trong `timings.csv` được quy về mili giây trên một lần lọc.
- Bảng tổng hợp báo cáo median, Q1 và Q3 tính trên 20 median theo ảnh của mỗi cấu hình. Q1–Q3 mô tả độ phân tán giữa các ảnh. `Speedup = T_seq / T_omp`, `Efficiency (%) = 100 × Speedup / P`. Max Absolute Error và RMSE được tính theo từng pixel, rồi lấy giá trị lớn nhất trên 20 ảnh ở mỗi dòng tổng hợp.
- Với bảng kết quả chính thức của ba lượt độc lập, trước tiên lấy trung vị thời gian giữa ba lượt cho từng cấu hình, sau đó tính lại Speedup và Efficiency từ các thời gian đã gộp. Bảng theo từng lượt và bảng gộp được lưu riêng trong `experiment_analysis/`.

## Kết quả thực nghiệm chính (mục 3.3)

Repository giữ ba lượt chính thức ngày **19/11/2026**, đo trên Windows 11 x64 với CPU AMD Ryzen AI 7 H 450, 16 bộ xử lý logic và GCC 16.2.0. Các lượt dùng QueryPerformanceCounter, 20 ảnh mỗi kích thước, ba bộ lọc, OpenMP 1/2/4/8/12/16 luồng, warm-up một lần và 10 lần lặp. Mỗi lượt có 54 dòng tổng hợp, 1.080 dòng timing và 1.080 phép đối chiếu ảnh/cấu hình; kết quả được tổng hợp trong [experiment_analysis](experiment_analysis/).

Với Gaussian, bảng dưới lấy cấu hình OpenMP có thời gian trung vị thấp nhất ở mỗi kích thước từ ba lượt.

| Kích thước ảnh | Luồng | Tuần tự (ms) | OpenMP (ms) | Tăng tốc |
| --- | ---: | ---: | ---: | ---: |
| 512×512 | 16 | 5,024 | 0,724 | 6,94× |
| 1024×1024 | 16 | 20,652 | 2,708 | 7,63× |
| 2048×2048 | 16 | 89,466 | 11,808 | 7,58× |

Các kết quả Sobel và Laplacian vẫn được giữ trong CSV/biểu đồ để hoàn thiện báo cáo. Xem thêm [bảng tổng hợp ba lượt](experiment_analysis/main_summary_3runs.csv), [biểu đồ](experiment_analysis/charts/) và [cấu hình của lượt thứ ba](results/windows_20261119_154648_2619652/run_config.json).

## Kết quả mở rộng (mục 3.4)

Benchmark V5 trên 100 ảnh có ba lượt chính thức trong [`image_filter_scaling/results/`](image_filter_scaling/results/). Bảng gộp và hai biểu đồ cuối nằm trong [`image_filter_scaling/batch_report_v5/`](image_filter_scaling/batch_report_v5/). Ảnh đầu vào đã đóng gói `.f32` được giữ local và không push; manifest và thông tin dataset được lưu để đối chiếu.

| Thuật toán | Tuần tự (ms) | OpenMP 16 luồng (ms) | Speedup | Efficiency |
| --- | ---: | ---: | ---: | ---: |
| Gaussian | 6.181,07 | 1.083,54 | 5,70× | 35,65% |
| Sobel | 156,08 | 49,59 | 3,15× | 19,67% |
| Laplacian | 389,71 | 59,09 | 6,59× | 41,22% |

## Tái tạo kết quả chính thức

Có thể tái tạo bảng số liệu và biểu đồ từ CSV đã lưu mà không chạy lại benchmark hoặc tải ảnh đầu vào. Cài các thư viện Python trước:

```sh
python -m pip install -r requirements.txt
```

Tổng hợp thực nghiệm chính từ ba lượt chính thức:

```sh
python experiment_analysis/tong_hop.py
```

Tổng hợp thực nghiệm mở rộng từ ba thư mục kết quả V5 chính thức:

```sh
python image_filter_scaling/plot.py image_filter_scaling/results/batch_v5_20261119_173042_808370 image_filter_scaling/results/batch_v5_20261119_174236_515234 image_filter_scaling/results/batch_v5_20261119_174444_749662 --out image_filter_scaling/batch_report_v5
```

Lệnh này chỉ đọc ba lượt thực nghiệm V5 chính thức được chỉ định, tránh tổng hợp nhầm các lượt chạy thử hoặc các phiên bản benchmark cũ. `experiment_analysis/tong_hop.py` kiểm tra mẫu thời gian gốc và tạo bảng/biểu đồ cho ba lượt chính; `benchmark/plot.py` vẽ một lượt riêng lẻ từ thư mục kết quả được chỉ định.

Phạm vi dữ liệu đã xác minh trong sáu lượt chính thức:

| Nội dung | Thực nghiệm chính | Thực nghiệm mở rộng |
| --- | ---: | ---: |
| Số lượt độc lập | 3 | 3 |
| Số ảnh đầu vào mỗi lượt | 60 | 100 |
| Mẫu thời gian | 37.800 | 180 |
| Phép đối chiếu đầu ra | 3.240 | 900 |
| Max Absolute Error | 0 | 0 |
| RMSE | 0 | 0 |

## Xử lý WinError 4551

Nếu Windows báo `WinError 4551`, Application Control đã chặn tệp `.bench-cache/build/image_benchmark.exe`. Để tiếp tục chạy qua Ubuntu WSL2, mở PowerShell tại thư mục gốc của repo rồi dùng lệnh sau.

```powershell
wsl -d Ubuntu -- bash run_all.sh --images
```

Lệnh này đo trong WSL2 và lưu kết quả trong `results/wsl_.../`.
